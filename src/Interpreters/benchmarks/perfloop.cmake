# Loaded through CMAKE_PROJECT_INCLUDE by the Perfloop benchmark build. This
# focused source slice keeps the native Google Benchmark target practical without
# building the entire dbms archive. It compiles the production sort predicate,
# block lookup, and concrete column implementations used by the workloads.
get_property(perfloop_is_already_sorted_benchmark_registered GLOBAL PROPERTY PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_REGISTERED)

if (NOT perfloop_is_already_sorted_benchmark_registered)
    set_property(GLOBAL PROPERTY PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_REGISTERED TRUE)
    set(PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")

    function(perfloop_add_is_already_sorted_benchmark)
        clickhouse_add_executable(benchmark_is_already_sorted
            "${PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE_DIR}/Interpreters/benchmarks/benchmark_is_already_sorted.cpp"
            "${PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE_DIR}/Interpreters/benchmarks/benchmark_is_already_sorted_datatype_stubs.cpp"
            "${PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE_DIR}/Interpreters/sortBlock.cpp"
            "${PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE_DIR}/Core/Block.cpp"
            "${PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE_DIR}/Core/ColumnWithTypeAndName.cpp"
            "${PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE_DIR}/Columns/ColumnConst.cpp"
            "${PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE_DIR}/Columns/IColumn.cpp"
            "${PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE_DIR}/Columns/ColumnVector.cpp"
            "${PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE_DIR}/Columns/ColumnsCommon.cpp"
            "${PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE_DIR}/Columns/MaskOperations.cpp"
            "${PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE_DIR}/Columns/ColumnCompressed.cpp"
            "${PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE_DIR}/Columns/ColumnIndex.cpp"
            "${PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE_DIR}/Columns/ColumnNullable.cpp"
            "${PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE_DIR}/Core/Field.cpp"
            "${PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE_DIR}/DataTypes/NestedUtils.cpp")
        target_link_libraries(benchmark_is_already_sorted PRIVATE
            ch_contrib::gbenchmark_all
            ch_contrib::lz4
            clickhouse_common_io)
    endfunction()

    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL perfloop_add_is_already_sorted_benchmark)
endif()
