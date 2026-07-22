/// Benchmark the Default sorting queue on overlapping MergeTree-style input chunks.
///
/// Each source has sorted chunks made of runs that are globally interleaved with
/// the other sources. The chunks therefore overlap and cannot use mergeImpl's
/// whole-chunk fast path. `safe_run_rows` controls the proven prefix available
/// from the top cursor: 64 exercises the proposed range path, while 1 is the
/// short-run control where every heap update remains necessary.

#include <Columns/ColumnsNumber.h>
#include <Common/assert_cast.h>
#include <Core/Block.h>
#include <Core/SortDescription.h>
#include <DataTypes/DataTypesNumber.h>
#include <Processors/Merges/Algorithms/MergingSortedAlgorithm.h>

#include <benchmark/benchmark.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace DB;

namespace
{

constexpr size_t rows_per_input = 4096;

struct Workload
{
    size_t input_count;
    size_t column_count;
    size_t output_block_rows;
    size_t safe_run_rows;
};

struct PreparedInputs
{
    IMergingAlgorithm::Inputs inputs;
    UInt64 expected_checksum = 0;
};

SharedHeader makeHeader(size_t column_count)
{
    Block header;
    header.insert(ColumnWithTypeAndName(ColumnUInt64::create(), std::make_shared<DataTypeUInt64>(), "key"));

    for (size_t column = 1; column < column_count; ++column)
    {
        header.insert(ColumnWithTypeAndName(
            ColumnUInt64::create(), std::make_shared<DataTypeUInt64>(), "payload_" + std::to_string(column)));
    }

    return std::make_shared<const Block>(std::move(header));
}

SortDescription makeSortDescription()
{
    SortDescription description;
    description.emplace_back("key", 1, 1);
    return description;
}

PreparedInputs makeInputs(const Workload & workload, UInt64 runtime_seed)
{
    chassert(workload.column_count >= 2);
    chassert(rows_per_input % workload.safe_run_rows == 0);

    PreparedInputs prepared;
    prepared.inputs.resize(workload.input_count);

    const size_t run_count = rows_per_input / workload.safe_run_rows;
    const UInt64 key_offset = (runtime_seed & 0xff) << 40;

    for (size_t source = 0; source < workload.input_count; ++source)
    {
        MutableColumns columns;
        columns.reserve(workload.column_count);
        for (size_t column = 0; column < workload.column_count; ++column)
            columns.emplace_back(ColumnUInt64::create());

        for (size_t run = 0; run < run_count; ++run)
        {
            for (size_t row_in_run = 0; row_in_run < workload.safe_run_rows; ++row_in_run)
            {
                const UInt64 key = key_offset
                    + ((static_cast<UInt64>(run) * workload.input_count + source) * workload.safe_run_rows + row_in_run);
                assert_cast<ColumnUInt64 &>(*columns[0]).insertValue(key);

                for (size_t column = 1; column < workload.column_count; ++column)
                {
                    const UInt64 payload = (static_cast<UInt64>(column) << 56)
                        ^ (static_cast<UInt64>(source) << 48) ^ key;
                    assert_cast<ColumnUInt64 &>(*columns[column]).insertValue(payload);

                    if (column == 1)
                        prepared.expected_checksum += key ^ payload;
                }
            }
        }

        prepared.inputs[source].set(Chunk(std::move(columns), rows_per_input));
    }

    return prepared;
}

UInt64 mergeAndChecksum(const Workload & workload, PreparedInputs prepared, UInt64 & output_rows)
{
    auto header = makeHeader(workload.column_count);
    MergingSortedAlgorithm algorithm(
        header,
        workload.input_count,
        makeSortDescription(),
        workload.output_block_rows,
        /*max_block_size_bytes=*/ 0,
        std::nullopt,
        SortingQueueStrategy::Default);

    algorithm.initialize(std::move(prepared.inputs));

    UInt64 checksum = 0;
    output_rows = 0;
    while (true)
    {
        auto status = algorithm.merge();
        if (status.chunk.hasRows())
        {
            const auto & columns = status.chunk.getColumns();
            const auto & keys = assert_cast<const ColumnUInt64 &>(*columns[0]).getData();
            const auto & payloads = assert_cast<const ColumnUInt64 &>(*columns[1]).getData();

            for (size_t row = 0; row < keys.size(); ++row)
                checksum += keys[row] ^ payloads[row];

            output_rows += status.chunk.getNumRows();
        }

        if (status.is_finished)
            return checksum;
    }
}

void BM_MergingSortedDefault(benchmark::State & state, Workload workload)
{
    UInt64 iteration_seed = 0;
    for (auto _ : state)
    {
        state.PauseTiming();
        const UInt64 runtime_seed = static_cast<UInt64>(state.iterations()) + iteration_seed++;
        auto prepared = makeInputs(workload, runtime_seed);
        const UInt64 expected_checksum = prepared.expected_checksum;
        state.ResumeTiming();

        UInt64 output_rows = 0;
        const UInt64 checksum = mergeAndChecksum(workload, std::move(prepared), output_rows);
        const UInt64 expected_rows = static_cast<UInt64>(workload.input_count) * rows_per_input;

        if (output_rows != expected_rows || checksum != expected_checksum)
        {
            state.SkipWithError("merge did not preserve the expected row payload");
            break;
        }

        /// The checksum consumes a payload from the returned chunks, so the merge
        /// result cannot be discarded by the optimizer.
        benchmark::DoNotOptimize(checksum);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations())
        * static_cast<int64_t>(workload.input_count) * static_cast<int64_t>(rows_per_input));
}

BENCHMARK_CAPTURE(
    BM_MergingSortedDefault,
    LongRuns_4Inputs_8Columns_Block8192,
    Workload{.input_count = 4, .column_count = 8, .output_block_rows = 8192, .safe_run_rows = 64});

/// Control: identical input volume and columns, but interleaved one-row runs.
BENCHMARK_CAPTURE(
    BM_MergingSortedDefault,
    ShortRuns_4Inputs_8Columns_Block8192,
    Workload{.input_count = 4, .column_count = 8, .output_block_rows = 8192, .safe_run_rows = 1});

/// Boundary guard: the range is capped by a small output block rather than its full safe run.
BENCHMARK_CAPTURE(
    BM_MergingSortedDefault,
    LongRuns_4Inputs_8Columns_Block64,
    Workload{.input_count = 4, .column_count = 8, .output_block_rows = 64, .safe_run_rows = 64});

/// Payload-width guard: one payload column changes the per-row column-dispatch balance.
BENCHMARK_CAPTURE(
    BM_MergingSortedDefault,
    LongRuns_4Inputs_2Columns_Block8192,
    Workload{.input_count = 4, .column_count = 2, .output_block_rows = 8192, .safe_run_rows = 64});

/// Heap-shape guard: more inputs changes the queue-update cost at every safe-prefix boundary.
BENCHMARK_CAPTURE(
    BM_MergingSortedDefault,
    LongRuns_8Inputs_8Columns_Block8192,
    Workload{.input_count = 8, .column_count = 8, .output_block_rows = 8192, .safe_run_rows = 64});

}
