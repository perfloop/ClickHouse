# Loaded through CMAKE_PROJECT_INCLUDE by the Perfloop mixed-tail guard build.
# This target isolates the descriptor-order regression risk of the all-constant
# fast path without building the complete dbms archive.
get_property(perfloop_mixed_tail_benchmark_registered GLOBAL PROPERTY PERFLOOP_MIXED_TAIL_BENCHMARK_REGISTERED)

if (NOT perfloop_mixed_tail_benchmark_registered)
    set_property(GLOBAL PROPERTY PERFLOOP_MIXED_TAIL_BENCHMARK_REGISTERED TRUE)
    set(PERFLOOP_MIXED_TAIL_BENCHMARK_SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")

    function(perfloop_add_mixed_tail_benchmark)
        clickhouse_add_executable(benchmark_is_already_sorted_mixed_tail
            "${PERFLOOP_MIXED_TAIL_BENCHMARK_SOURCE_DIR}/Interpreters/benchmarks/benchmark_is_already_sorted_mixed_tail.cpp"
            "${PERFLOOP_MIXED_TAIL_BENCHMARK_SOURCE_DIR}/Interpreters/benchmarks/benchmark_is_already_sorted_datatype_stubs.cpp"
            "${PERFLOOP_MIXED_TAIL_BENCHMARK_SOURCE_DIR}/Interpreters/sortBlock.cpp"
            "${PERFLOOP_MIXED_TAIL_BENCHMARK_SOURCE_DIR}/Core/Block.cpp"
            "${PERFLOOP_MIXED_TAIL_BENCHMARK_SOURCE_DIR}/Core/ColumnWithTypeAndName.cpp"
            "${PERFLOOP_MIXED_TAIL_BENCHMARK_SOURCE_DIR}/Columns/ColumnConst.cpp"
            "${PERFLOOP_MIXED_TAIL_BENCHMARK_SOURCE_DIR}/Columns/IColumn.cpp"
            "${PERFLOOP_MIXED_TAIL_BENCHMARK_SOURCE_DIR}/Columns/ColumnVector.cpp"
            "${PERFLOOP_MIXED_TAIL_BENCHMARK_SOURCE_DIR}/Columns/ColumnsCommon.cpp"
            "${PERFLOOP_MIXED_TAIL_BENCHMARK_SOURCE_DIR}/Columns/MaskOperations.cpp"
            "${PERFLOOP_MIXED_TAIL_BENCHMARK_SOURCE_DIR}/Columns/ColumnCompressed.cpp"
            "${PERFLOOP_MIXED_TAIL_BENCHMARK_SOURCE_DIR}/Columns/ColumnIndex.cpp"
            "${PERFLOOP_MIXED_TAIL_BENCHMARK_SOURCE_DIR}/Columns/ColumnNullable.cpp"
            "${PERFLOOP_MIXED_TAIL_BENCHMARK_SOURCE_DIR}/Core/Field.cpp"
            "${PERFLOOP_MIXED_TAIL_BENCHMARK_SOURCE_DIR}/DataTypes/NestedUtils.cpp")
        target_link_libraries(benchmark_is_already_sorted_mixed_tail PRIVATE
            ch_contrib::gbenchmark_all
            ch_contrib::lz4
            clickhouse_common_io)
    endfunction()

    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL perfloop_add_mixed_tail_benchmark)
endif()
