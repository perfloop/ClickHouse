# Extend the previously sealed guard-target injection with the refill transition
# benchmark. Both modules are source-only harness material and are loaded through
# CMAKE_PROJECT_INCLUDE for the dedicated proof build.
include("${CMAKE_CURRENT_LIST_DIR}/../perfloop_default_merge_guard/perfloop_add_targets.cmake")

set(PERFLOOP_DEFAULT_MERGE_TRANSITION_GUARD_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(perfloop_add_default_merge_transition_guard_target)
    clickhouse_add_executable(
        benchmark_merging_sorted_default_transition_guard
        "${PERFLOOP_DEFAULT_MERGE_TRANSITION_GUARD_DIR}/benchmark_merging_sorted_default_transition_guard.cpp")
    target_link_libraries(benchmark_merging_sorted_default_transition_guard PRIVATE
        ch_contrib::gbenchmark_all
        dbms)
endfunction()

cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL perfloop_add_default_merge_transition_guard_target)
