#include <Columns/ColumnConst.h>
#include <Columns/ColumnsNumber.h>
#include <Core/Block.h>
#include <Core/SortDescription.h>
#include <Interpreters/sortBlock.h>

#include <benchmark/benchmark.h>

#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

using namespace DB;

namespace
{

struct SortWorkload
{
    Block block;
    SortDescription description;
};

SortDescription makeSortDescription()
{
    SortDescription description;
    description.reserve(4);

    for (size_t key = 0; key < 4; ++key)
        description.emplace_back("key" + std::to_string(key), 1, 1);

    return description;
}

SortWorkload makeTrailingNonConstantWorkload(size_t rows, UInt64 seed, bool make_unsorted)
{
    ColumnsWithTypeAndName columns;
    columns.reserve(4);

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

    return {Block(std::move(columns)), makeSortDescription()};
}

bool verifyTrailingNonConstantSemantics()
{
    auto sorted = makeTrailingNonConstantWorkload(/*rows=*/8, /*seed=*/11, /*make_unsorted=*/false);
    if (!isAlreadySorted(sorted.block, sorted.description))
    {
        std::cerr << "sorted trailing non-constant key was not recognized as sorted\n";
        return false;
    }

    auto unsorted = makeTrailingNonConstantWorkload(/*rows=*/8, /*seed=*/23, /*make_unsorted=*/true);
    if (isAlreadySorted(unsorted.block, unsorted.description))
    {
        std::cerr << "unsorted trailing non-constant key was recognized as sorted\n";
        return false;
    }

    return true;
}

UInt64 runtimeSeed()
{
    return static_cast<UInt64>(std::chrono::steady_clock::now().time_since_epoch().count()) & 0xFFFF;
}

void benchmarkTrailingNonConstant(benchmark::State & state)
{
    auto workload = makeTrailingNonConstantWorkload(/*rows=*/1048576, runtimeSeed(), /*make_unsorted=*/false);

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(workload.block);
        const bool already_sorted = isAlreadySorted(workload.block, workload.description);
        benchmark::DoNotOptimize(already_sorted);

        if (!already_sorted)
        {
            state.SkipWithError("sorted trailing non-constant key must be sorted");
            break;
        }
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * 1048576);
    state.counters["rows"] = 1048576;
    state.counters["keys"] = 4;
}

}

int main(int argc, char ** argv)
{
    if (!verifyTrailingNonConstantSemantics())
        return 1;

    if (argc == 2 && std::string_view(argv[1]) == "--verify")
        return 0;

    benchmark::RegisterBenchmark("is_already_sorted/mixed_tail_const/1048576_rows/4_keys", benchmarkTrailingNonConstant)
        ->Unit(benchmark::kNanosecond);
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    return 0;
}
