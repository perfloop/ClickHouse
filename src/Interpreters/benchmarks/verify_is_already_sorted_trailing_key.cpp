#include <Columns/ColumnConst.h>
#include <Columns/ColumnsNumber.h>
#include <Core/Block.h>
#include <Core/SortDescription.h>
#include <Interpreters/sortBlock.h>

#include <iostream>
#include <string>
#include <utility>

using namespace DB;

namespace
{

struct SortWorkload
{
    Block block;
    SortDescription description;
};

SortWorkload makeTrailingVariableWorkload(bool make_unsorted)
{
    constexpr size_t rows = 8;
    constexpr UInt64 seed = 11;

    ColumnsWithTypeAndName columns;
    columns.reserve(4);

    SortDescription description;
    description.reserve(4);
    for (size_t key = 0; key < 4; ++key)
        description.emplace_back("key" + std::to_string(key), 1, 1);

    for (size_t key = 0; key < 3; ++key)
    {
        auto value = ColumnUInt64::create();
        value->insert(seed + key);
        columns.emplace_back(ColumnConst::create(std::move(value), rows), DataTypePtr{}, "key" + std::to_string(key));
    }

    auto trailing_key = ColumnUInt64::create();
    for (size_t row = 0; row < rows; ++row)
    {
        const UInt64 value = make_unsorted && row == rows / 2 ? seed + row - 2 : seed + row;
        trailing_key->insert(value);
    }
    columns.emplace_back(std::move(trailing_key), DataTypePtr{}, "key3");

    return {Block(std::move(columns)), std::move(description)};
}

}

int main()
{
    auto sorted = makeTrailingVariableWorkload(/*make_unsorted=*/false);
    if (!isAlreadySorted(sorted.block, sorted.description))
    {
        std::cerr << "sorted trailing variable key was not recognized as sorted\n";
        return 1;
    }

    auto unsorted = makeTrailingVariableWorkload(/*make_unsorted=*/true);
    if (isAlreadySorted(unsorted.block, unsorted.description))
    {
        std::cerr << "unsorted trailing variable key was recognized as sorted\n";
        return 1;
    }

    return 0;
}
