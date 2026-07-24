get_property(PERFLOOP_IS_ALREADY_SORTED_TRAILING_KEY_VERIFY_INCLUDED GLOBAL PROPERTY PERFLOOP_IS_ALREADY_SORTED_TRAILING_KEY_VERIFY_INCLUDED)
if (NOT PERFLOOP_IS_ALREADY_SORTED_TRAILING_KEY_VERIFY_INCLUDED)
    set_property(GLOBAL PROPERTY PERFLOOP_IS_ALREADY_SORTED_TRAILING_KEY_VERIFY_INCLUDED ON)

    set(PERFLOOP_TRAILING_KEY_VERIFY_SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")

    function(add_perfloop_is_already_sorted_trailing_key_verify)
        clickhouse_add_executable(verify_is_already_sorted_trailing_key
            "${PERFLOOP_TRAILING_KEY_VERIFY_SOURCE_DIR}/Interpreters/benchmarks/verify_is_already_sorted_trailing_key.cpp"
            "${PERFLOOP_TRAILING_KEY_VERIFY_SOURCE_DIR}/Interpreters/benchmarks/benchmark_is_already_sorted_datatype_stubs.cpp"
            "${PERFLOOP_TRAILING_KEY_VERIFY_SOURCE_DIR}/Interpreters/sortBlock.cpp"
            "${PERFLOOP_TRAILING_KEY_VERIFY_SOURCE_DIR}/Core/Block.cpp"
            "${PERFLOOP_TRAILING_KEY_VERIFY_SOURCE_DIR}/Core/ColumnWithTypeAndName.cpp"
            "${PERFLOOP_TRAILING_KEY_VERIFY_SOURCE_DIR}/Columns/ColumnConst.cpp"
            "${PERFLOOP_TRAILING_KEY_VERIFY_SOURCE_DIR}/Columns/IColumn.cpp"
            "${PERFLOOP_TRAILING_KEY_VERIFY_SOURCE_DIR}/Columns/ColumnVector.cpp"
            "${PERFLOOP_TRAILING_KEY_VERIFY_SOURCE_DIR}/Columns/ColumnsCommon.cpp"
            "${PERFLOOP_TRAILING_KEY_VERIFY_SOURCE_DIR}/Columns/MaskOperations.cpp"
            "${PERFLOOP_TRAILING_KEY_VERIFY_SOURCE_DIR}/Columns/ColumnCompressed.cpp"
            "${PERFLOOP_TRAILING_KEY_VERIFY_SOURCE_DIR}/Columns/ColumnIndex.cpp"
            "${PERFLOOP_TRAILING_KEY_VERIFY_SOURCE_DIR}/Columns/ColumnNullable.cpp"
            "${PERFLOOP_TRAILING_KEY_VERIFY_SOURCE_DIR}/Core/Field.cpp"
            "${PERFLOOP_TRAILING_KEY_VERIFY_SOURCE_DIR}/DataTypes/NestedUtils.cpp")
        target_link_libraries(verify_is_already_sorted_trailing_key PRIVATE
            ch_contrib::lz4
            clickhouse_common_io)
    endfunction()

    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL add_perfloop_is_already_sorted_trailing_key_verify)
endif()
