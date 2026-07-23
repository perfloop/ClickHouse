/// Additional proof-only boundary cases for benchmark_merging_sorted_default.
///
/// Keep the workload implementation and result consumption in the native benchmark
/// source; this translation unit only registers the short-prefix shapes that the
/// sealed benchmark did not originally cover.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wheader-hygiene"
#include "../benchmark_merging_sorted_default.cpp"
#pragma clang diagnostic pop

BENCHMARK_CAPTURE(
    BM_MergingSortedDefault,
    Prefix2_4Inputs_8Columns_Block8192,
    Workload{.input_count = 4, .column_count = 8, .output_block_rows = 8192, .safe_run_rows = 2});

BENCHMARK_CAPTURE(
    BM_MergingSortedDefault,
    Prefix4_4Inputs_8Columns_Block8192,
    Workload{.input_count = 4, .column_count = 8, .output_block_rows = 8192, .safe_run_rows = 4});
