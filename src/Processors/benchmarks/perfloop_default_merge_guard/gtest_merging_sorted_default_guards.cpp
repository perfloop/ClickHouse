/// Additional proof-only transition coverage for the Default-to-Batch promotion.
///
/// Reuse the focused native Default merge test helpers without modifying its
/// contract-owned source. This case starts with a Batch-eligible prefix, then
/// supplies the same source's next chunk through consume() after a later one-row
/// prefix has been reached.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wheader-hygiene"
#include "../../Merges/Algorithms/tests/gtest_merging_sorted_default.cpp"
#pragma clang diagnostic pop

TEST(MergingSortedDefaultGuard, PreservesOutputAndRequiredSourceAcrossBatchFallback)
{
    WriteBufferFromOwnString row_sources_buffer;
    MergingSortedAlgorithm algorithm(
        makeHeader(/*with_filter=*/ false),
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

    IMergingAlgorithm::Inputs inputs(2);
    inputs[0].set(makeChunk({{1, 101}, {2, 102}, {3, 103}}, false));
    inputs[1].set(makeChunk({{4, 104}, {5, 105}, {7, 107}, {8, 108}, {10, 110}}, false));
    algorithm.initialize(std::move(inputs));

    std::vector<UInt64> keys;
    std::vector<UInt64> values;
    std::vector<size_t> block_rows;
    std::vector<ssize_t> required_sources;
    bool supplied_second_source_zero_chunk = false;
    bool finished = false;

    for (size_t call = 0; call < 16; ++call)
    {
        auto status = algorithm.merge();
        if (status.chunk.hasRows())
        {
            ASSERT_LE(status.chunk.getNumRows(), 3u);
            block_rows.push_back(status.chunk.getNumRows());

            const auto & columns = status.chunk.getColumns();
            const auto & chunk_keys = assert_cast<const ColumnUInt64 &>(*columns[0]).getData();
            const auto & chunk_values = assert_cast<const ColumnUInt64 &>(*columns[1]).getData();
            keys.insert(keys.end(), chunk_keys.begin(), chunk_keys.end());
            values.insert(values.end(), chunk_values.begin(), chunk_values.end());
        }

        if (status.required_source >= 0)
        {
            required_sources.push_back(status.required_source);
            IMergingAlgorithm::Input next_input;
            if (status.required_source == 0 && !supplied_second_source_zero_chunk)
            {
                next_input.set(makeChunk({{6, 106}, {9, 109}}, false));
                supplied_second_source_zero_chunk = true;
                algorithm.consume(next_input, /*source_num=*/ 0);
            }
        }

        if (status.is_finished)
        {
            finished = true;
            break;
        }
    }

    EXPECT_TRUE(finished);
    EXPECT_TRUE(supplied_second_source_zero_chunk);
    EXPECT_EQ(required_sources, (std::vector<ssize_t>{0, 0, 1}));
    EXPECT_EQ(block_rows, (std::vector<size_t>{3, 3, 3, 1}));
    EXPECT_EQ(keys, (std::vector<UInt64>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
    EXPECT_EQ(values, (std::vector<UInt64>{101, 102, 103, 104, 105, 106, 107, 108, 109, 110}));

    const std::string & serialized = row_sources_buffer.str();
    ASSERT_EQ(serialized.size() % sizeof(RowSourcePart), 0u);
    std::vector<RowSourcePart> row_sources;
    for (size_t offset = 0; offset < serialized.size(); offset += sizeof(RowSourcePart))
    {
        RowSourcePart row_source;
        std::memcpy(&row_source, serialized.data() + offset, sizeof(row_source));
        row_sources.push_back(row_source);
    }
    expectRowSources(
        row_sources,
        {{0, false}, {0, false}, {0, false}, {1, false}, {1, false}, {0, false}, {1, false}, {1, false}, {0, false}, {1, false}});
}
