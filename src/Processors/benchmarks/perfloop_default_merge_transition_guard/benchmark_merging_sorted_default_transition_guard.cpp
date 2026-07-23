/// Native guard for the Default-to-Batch transition after a source refill.
///
/// The first source starts with a three-row proven prefix, then is refilled after
/// the Batch queue reaches a later one-row prefix. The measured operation checks
/// the externally visible merge contract while consuming every result chunk.
#include <Columns/ColumnsNumber.h>
#include <Common/assert_cast.h>
#include <Core/Block.h>
#include <Core/SortDescription.h>
#include <DataTypes/DataTypesNumber.h>
#include <IO/WriteBufferFromString.h>
#include <Processors/Merges/Algorithms/MergingSortedAlgorithm.h>
#include <Processors/Transforms/ColumnGathererTransform.h>

#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace DB;

namespace
{

SharedHeader makeHeader()
{
    Block header;
    header.insert(ColumnWithTypeAndName(ColumnUInt64::create(), std::make_shared<DataTypeUInt64>(), "key"));
    header.insert(ColumnWithTypeAndName(ColumnUInt64::create(), std::make_shared<DataTypeUInt64>(), "value"));
    return std::make_shared<const Block>(std::move(header));
}

SortDescription makeSortDescription()
{
    SortDescription description;
    description.emplace_back("key", 1, 1);
    return description;
}

Chunk makeChunk(std::initializer_list<UInt64> relative_keys, UInt64 key_offset)
{
    auto keys = ColumnUInt64::create();
    auto values = ColumnUInt64::create();
    for (const UInt64 relative_key : relative_keys)
    {
        const UInt64 key = key_offset + relative_key;
        keys->insertValue(key);
        values->insertValue(key + 100);
    }

    Columns columns{std::move(keys), std::move(values)};
    return Chunk(std::move(columns), relative_keys.size());
}

struct PreparedTransition
{
    IMergingAlgorithm::Inputs inputs{2};
    IMergingAlgorithm::Input source_zero_refill;
    UInt64 key_offset;
};

PreparedTransition makeTransition(UInt64 key_offset)
{
    PreparedTransition prepared;
    prepared.key_offset = key_offset;
    prepared.inputs[0].set(makeChunk({1, 2, 3}, key_offset));
    prepared.inputs[1].set(makeChunk({4, 5, 7, 8, 10}, key_offset));
    prepared.source_zero_refill.set(makeChunk({6, 9}, key_offset));
    return prepared;
}

bool mergeAndValidateTransition(const SharedHeader & header, PreparedTransition prepared, UInt64 & checksum)
{
    WriteBufferFromOwnString row_sources_buffer;
    MergingSortedAlgorithm algorithm(
        header,
        /*num_inputs=*/ 2,
        makeSortDescription(),
        /*max_block_size=*/ 3,
        /*max_block_size_bytes=*/ 0,
        std::nullopt,
        SortingQueueStrategy::Default,
        /*limit=*/ 0,
        &row_sources_buffer,
        std::nullopt,
        /*use_average_block_sizes=*/ false);
    algorithm.initialize(std::move(prepared.inputs));

    constexpr std::array<UInt64, 10> expected_relative_keys{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    constexpr std::array<size_t, 10> expected_sources{0, 0, 0, 1, 1, 0, 1, 1, 0, 1};
    std::vector<size_t> block_rows;
    std::vector<ssize_t> required_sources;
    size_t output_index = 0;
    bool supplied_refill = false;
    bool finished = false;
    checksum = 0;

    for (size_t call = 0; call < 16; ++call)
    {
        auto status = algorithm.merge();
        if (status.chunk.hasRows())
        {
            if (status.chunk.getNumRows() > 3)
                return false;
            block_rows.push_back(status.chunk.getNumRows());

            const auto & columns = status.chunk.getColumns();
            const auto & keys = assert_cast<const ColumnUInt64 &>(*columns[0]).getData();
            const auto & values = assert_cast<const ColumnUInt64 &>(*columns[1]).getData();
            for (size_t row = 0; row < keys.size(); ++row)
            {
                if (output_index == expected_relative_keys.size()
                    || keys[row] != prepared.key_offset + expected_relative_keys[output_index]
                    || values[row] != keys[row] + 100)
                    return false;

                checksum += keys[row] ^ values[row];
                ++output_index;
            }
        }

        if (status.required_source >= 0)
        {
            required_sources.push_back(status.required_source);
            if (status.required_source == 0 && !supplied_refill)
            {
                algorithm.consume(prepared.source_zero_refill, /*source_num=*/ 0);
                supplied_refill = true;
            }
        }

        if (status.is_finished)
        {
            finished = true;
            break;
        }
    }

    if (!finished || !supplied_refill || output_index != expected_relative_keys.size()
        || required_sources != std::vector<ssize_t>{0, 0, 1}
        || block_rows != std::vector<size_t>{3, 3, 3, 1})
        return false;

    const std::string & serialized = row_sources_buffer.str();
    if (serialized.size() != expected_sources.size() * sizeof(RowSourcePart))
        return false;

    for (size_t row = 0; row < expected_sources.size(); ++row)
    {
        RowSourcePart row_source;
        std::memcpy(&row_source, serialized.data() + row * sizeof(row_source), sizeof(row_source));
        if (row_source.getSourceNum() != expected_sources[row] || row_source.getSkipFlag())
            return false;
    }

    return true;
}

void BM_MergingSortedDefaultTransition(benchmark::State & state)
{
    const auto header = makeHeader();
    UInt64 iteration_seed = 0;
    for (auto _ : state)
    {
        state.PauseTiming();
        PreparedTransition prepared = makeTransition((iteration_seed++ & 0xff) << 16);
        state.ResumeTiming();

        UInt64 checksum = 0;
        if (!mergeAndValidateTransition(header, std::move(prepared), checksum))
        {
            state.SkipWithError("Default Batch transition violated the merge contract");
            break;
        }
        benchmark::DoNotOptimize(checksum);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * 10);
}

BENCHMARK(BM_MergingSortedDefaultTransition);

}
