clickhouse_add_executable(benchmark_merging_sorted_default benchmark_merging_sorted_default.cpp)
target_link_libraries(benchmark_merging_sorted_default PRIVATE
    ch_contrib::gbenchmark_all
    dbms)

if (ENABLE_TESTS)
    clickhouse_add_executable(gtest_merging_sorted_default ../Merges/Algorithms/tests/gtest_merging_sorted_default.cpp)
    target_link_libraries(gtest_merging_sorted_default PRIVATE
        ch_contrib::gmock_all
        dbms)
endif()
