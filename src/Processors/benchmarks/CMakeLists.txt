clickhouse_add_executable(benchmark_merging_sorted_default benchmark_merging_sorted_default.cpp)
target_link_libraries(benchmark_merging_sorted_default PRIVATE
    ch_contrib::gbenchmark_all
    dbms)
