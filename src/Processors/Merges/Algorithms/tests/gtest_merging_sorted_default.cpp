#include <gtest/gtest.h>

#include <Columns/ColumnsNumber.h>
#include <Common/assert_cast.h>
#include <Core/Block.h>
#include <Core/SortDescription.h>
#include <DataTypes/DataTypesNumber.h>
#include <IO/WriteBufferFromString.h>
#include <Processors/Executors/PullingPipelineExecutor.h>
#include <Processors/Merges/Algorithms/MergeTreeReadInfo.h>
#include <Processors/Merges/MergingSortedTransform.h>
#include <Processors/Sources/SourceFromChunks.h>
#include <Processors/Transforms/ColumnGathererTransform.h>
#include <QueryPipeline/Pipe.h>
#include <QueryPipeline/QueryPipeline.h>

#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace DB;

namespace
{

struct Row
{
    UInt64 key;
    UInt64 value;
    UInt8 keep = 1;
};

struct MergeResult
{
    std::vector<UInt64> keys;
    std::vector<UInt64> values;
    std::vector<UInt8> filters;
    std::vector<size_t> block_rows;
    std::vector<RowSourcePart> row_sources;
};

SharedHeader makeHeader(bool with_filter)
{
    Block header;
    header.insert(ColumnWithTypeAndName(ColumnUInt64::create(), std::make_shared<DataTypeUInt64>(), "key"));
    header.insert(ColumnWithTypeAndName(ColumnUInt64::create(), std::make_shared<DataTypeUInt64>(), "value"));
    if (with_filter)
        header.insert(ColumnWithTypeAndName(ColumnUInt8::create(), std::make_shared<DataTypeUInt8>(), "keep"));

    return std::make_shared<const Block>(std::move(header));
}

SortDescription makeSortDescription()
{
    SortDescription description;
    description.emplace_back("key", 1, 1);
    return description;
}

Chunk makeChunk(const std::vector<Row> & rows, bool with_filter)
{
    auto keys = ColumnUInt64::create();
    auto values = ColumnUInt64::create();
    auto filters = ColumnUInt8::create();

    for (const auto & row : rows)
    {
        keys->insertValue(row.key);
        values->insertValue(row.value);
        if (with_filter)
            filters->insertValue(row.keep);
    }

    Columns columns{std::move(keys), std::move(values)};
    if (with_filter)
        columns.emplace_back(std::move(filters));

    return Chunk(std::move(columns), rows.size());
}

Chunk makeVirtualRow(UInt64 key)
{
    auto key_column = ColumnUInt64::create();
    key_column->insertValue(key);

    Block pk_block;
    pk_block.insert(ColumnWithTypeAndName(std::move(key_column), std::make_shared<DataTypeUInt64>(), "key"));

    /// The normal columns keep the source alive; MergingSortedAlgorithm replaces
    /// them with the primary-key virtual row before it enters the queue.
    Chunk virtual_row = makeChunk({Row{.key = key, .value = 0}}, false);
    virtual_row.getChunkInfos().add(std::make_shared<MergeTreeReadInfo>(0, pk_block, nullptr));
    return virtual_row;
}

template <typename... Args>
Chunks makeChunks(Args &&... chunks)
{
    Chunks result;
    result.reserve(sizeof...(chunks));
    (result.emplace_back(std::forward<Args>(chunks)), ...);
    return result;
}

template <typename... Args>
std::vector<Chunks> makeInputChunks(Args &&... inputs)
{
    std::vector<Chunks> result;
    result.reserve(sizeof...(inputs));
    (result.emplace_back(std::forward<Args>(inputs)), ...);
    return result;
}

MergeResult runDefaultMerge(
    std::vector<Chunks> input_chunks,
    size_t max_block_size,
    UInt64 limit = 0,
    const std::optional<String> & filter_column_name = std::nullopt)
{
    const bool with_filter = filter_column_name.has_value();
    auto header = makeHeader(with_filter);

    Pipes pipes;
    pipes.reserve(input_chunks.size());
    for (auto & chunks : input_chunks)
        pipes.emplace_back(std::make_shared<SourceFromChunks>(header, std::move(chunks)));

    Pipe pipe = Pipe::unitePipes(std::move(pipes));
    WriteBufferFromOwnString row_sources_buffer;

    auto transform = std::make_shared<MergingSortedTransform>(
        pipe.getSharedHeader(),
        pipe.numOutputPorts(),
        makeSortDescription(),
        max_block_size,
        /*max_block_size_bytes=*/ 0,
        std::nullopt,
        SortingQueueStrategy::Default,
        limit,
        /*always_read_till_end=*/ false,
        &row_sources_buffer,
        filter_column_name,
        /*use_average_block_sizes=*/ false,
        /// Virtual rows in this focused test already contain the final key type.
        /*apply_virtual_row_conversions=*/ false);
    pipe.addTransform(std::move(transform));

    MergeResult result;
    {
        QueryPipeline pipeline(std::move(pipe));
        PullingPipelineExecutor executor(pipeline);

        Block block;
        while (executor.pull(block))
        {
            if (!block.rows())
                continue;

            result.block_rows.push_back(block.rows());
            const auto & keys = assert_cast<const ColumnUInt64 &>(*block.getByName("key").column).getData();
            const auto & values = assert_cast<const ColumnUInt64 &>(*block.getByName("value").column).getData();
            result.keys.insert(result.keys.end(), keys.begin(), keys.end());
            result.values.insert(result.values.end(), values.begin(), values.end());

            if (with_filter)
            {
                const auto & filters = assert_cast<const ColumnUInt8 &>(*block.getByName("keep").column).getData();
                result.filters.insert(result.filters.end(), filters.begin(), filters.end());
            }
        }
    }

    const std::string & serialized = row_sources_buffer.str();
    EXPECT_EQ(serialized.size() % sizeof(RowSourcePart), 0u);
    for (size_t offset = 0; offset < serialized.size(); offset += sizeof(RowSourcePart))
    {
        RowSourcePart row_source;
        std::memcpy(&row_source, serialized.data() + offset, sizeof(row_source));
        result.row_sources.push_back(row_source);
    }

    return result;
}

void expectRowSources(const std::vector<RowSourcePart> & actual, const std::vector<std::pair<size_t, bool>> & expected)
{
    ASSERT_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_EQ(actual[i].getSourceNum(), expected[i].first) << "row source " << i;
        EXPECT_EQ(actual[i].getSkipFlag(), expected[i].second) << "row source " << i;
    }
}

}

TEST(MergingSortedDefault, PreservesTieOrderAndRowSources)
{
    MergeResult result = runDefaultMerge(
        makeInputChunks(
            makeChunks(makeChunk({{1, 100}, {2, 101}, {4, 102}, {8, 103}}, false)),
            makeChunks(makeChunk({{1, 200}, {3, 201}, {4, 202}, {9, 203}}, false)),
            makeChunks(makeChunk({{1, 300}, {2, 301}, {4, 302}, {10, 303}}, false))),
        /*max_block_size=*/ 64);

    EXPECT_EQ(result.keys, (std::vector<UInt64>{1, 1, 1, 2, 2, 3, 4, 4, 4, 8, 9, 10}));
    EXPECT_EQ(result.values, (std::vector<UInt64>{100, 200, 300, 101, 301, 201, 102, 202, 302, 103, 203, 303}));
    expectRowSources(
        result.row_sources,
        {{0, false}, {1, false}, {2, false}, {0, false}, {2, false}, {1, false},
            {0, false}, {1, false}, {2, false}, {0, false}, {1, false}, {2, false}});
}

TEST(MergingSortedDefault, RespectsLimitAndMaxBlockSplits)
{
    MergeResult result = runDefaultMerge(
        makeInputChunks(
            makeChunks(makeChunk({{1, 100}, {2, 101}, {4, 102}, {8, 103}}, false)),
            makeChunks(makeChunk({{1, 200}, {3, 201}, {4, 202}, {9, 203}}, false)),
            makeChunks(makeChunk({{1, 300}, {2, 301}, {4, 302}, {10, 303}}, false))),
        /*max_block_size=*/ 3,
        /*limit=*/ 7);

    EXPECT_EQ(result.block_rows, (std::vector<size_t>{3, 3, 1}));
    EXPECT_EQ(result.keys, (std::vector<UInt64>{1, 1, 1, 2, 2, 3, 4}));
    EXPECT_EQ(result.values, (std::vector<UInt64>{100, 200, 300, 101, 301, 201, 102}));
    expectRowSources(
        result.row_sources,
        {{0, false}, {1, false}, {2, false}, {0, false}, {2, false}, {1, false}, {0, false}});
}

TEST(MergingSortedDefault, SkipsVirtualRowsAtSourceBoundaries)
{
    MergeResult result = runDefaultMerge(
        makeInputChunks(
            makeChunks(
                makeChunk({{1, 100}, {2, 101}}, false),
                makeVirtualRow(4),
                makeChunk({{4, 102}, {7, 103}}, false)),
            makeChunks(makeChunk({{0, 200}, {3, 201}, {5, 202}, {6, 203}}, false))),
        /*max_block_size=*/ 64);

    EXPECT_EQ(result.keys, (std::vector<UInt64>{0, 1, 2, 3, 4, 5, 6, 7}));
    EXPECT_EQ(result.values, (std::vector<UInt64>{200, 100, 101, 201, 102, 202, 203, 103}));
    expectRowSources(
        result.row_sources,
        {{1, false}, {0, false}, {0, false}, {1, false}, {0, false}, {1, false}, {1, false}, {0, false}});
}

TEST(MergingSortedDefault, KeepsFilteredRowsOnTheRowWiseFallback)
{
    MergeResult result = runDefaultMerge(
        makeInputChunks(
            makeChunks(makeChunk({{1, 100, 1}, {3, 101, 0}, {5, 102, 1}}, true)),
            makeChunks(makeChunk({{2, 200, 1}, {4, 201, 0}, {6, 202, 1}}, true))),
        /*max_block_size=*/ 64,
        /*limit=*/ 0,
        /*filter_column_name=*/ String{"keep"});

    EXPECT_EQ(result.keys, (std::vector<UInt64>{1, 2, 5, 6}));
    EXPECT_EQ(result.values, (std::vector<UInt64>{100, 200, 102, 202}));
    EXPECT_EQ(result.filters, (std::vector<UInt8>{1, 1, 1, 1}));
    expectRowSources(
        result.row_sources,
        {{0, false}, {1, false}, {0, true}, {1, true}, {0, false}, {1, false}});
}
