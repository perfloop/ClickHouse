# Loaded through CMAKE_PROJECT_INCLUDE by the Perfloop benchmark build. Deferring
# the target creation keeps the measurement-only target out of normal builds and
# lets it link the dbms target after src/CMakeLists.txt has completed.
get_property(perfloop_is_already_sorted_benchmark_registered GLOBAL PROPERTY PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_REGISTERED)

if (NOT perfloop_is_already_sorted_benchmark_registered)
    set_property(GLOBAL PROPERTY PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_REGISTERED TRUE)
    set(PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE
        "${CMAKE_CURRENT_LIST_DIR}/benchmark_is_already_sorted.cpp")

    function(perfloop_add_is_already_sorted_benchmark)
        clickhouse_add_executable(benchmark_is_already_sorted
            "${PERFLOOP_IS_ALREADY_SORTED_BENCHMARK_SOURCE}")
        target_link_libraries(benchmark_is_already_sorted PRIVATE
            ch_contrib::gbenchmark_all
            dbms)
    endfunction()

    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL perfloop_add_is_already_sorted_benchmark)
endif()
