# Inject proof-only native targets without modifying contract-owned manifests.
# This file is loaded through CMAKE_PROJECT_INCLUDE and deferred until the
# ClickHouse target helpers and dbms target have been defined.
set(PERFLOOP_DEFAULT_MERGE_GUARD_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(perfloop_add_default_merge_guard_targets)
    clickhouse_add_executable(
        benchmark_merging_sorted_default_guards
        "${PERFLOOP_DEFAULT_MERGE_GUARD_DIR}/benchmark_merging_sorted_default_guards.cpp")
    target_link_libraries(benchmark_merging_sorted_default_guards PRIVATE
        ch_contrib::gbenchmark_all
        dbms)

    if (ENABLE_TESTS)
        clickhouse_add_executable(
            gtest_merging_sorted_default_guards
            "${PERFLOOP_DEFAULT_MERGE_GUARD_DIR}/gtest_merging_sorted_default_guards.cpp")
        target_link_libraries(gtest_merging_sorted_default_guards PRIVATE
            ch_contrib::gmock_all
            dbms)
    endif()
endfunction()

cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL perfloop_add_default_merge_guard_targets)
