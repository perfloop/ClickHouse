#include <Columns/ColumnConst.h>
#include <Columns/ColumnsNumber.h>
#include <Core/Block.h>
#include <Core/SortDescription.h>
#include <DataTypes/DataTypesNumber.h>
#include <Interpreters/sortBlock.h>

#include <benchmark/benchmark.h>

#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
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

SortDescription makeSortDescription(size_t key_count)
{
    SortDescription description;
    description.reserve(key_count);

    for (size_t key = 0; key < key_count; ++key)
        description.emplace_back("key" + std::to_string(key), 1, 1);

    return description;
}

SortWorkload makeAllConstantWorkload(size_t rows, size_t key_count, UInt64 seed)
{
    ColumnsWithTypeAndName columns;
    columns.reserve(key_count);

    for (size_t key = 0; key < key_count; ++key)
    {
        auto value = ColumnUInt64::create();
        value->insert(seed + key);
        columns.emplace_back(ColumnConst::create(std::move(value), rows), std::make_shared<DataTypeUInt64>(), "key" + std::to_string(key));
    }

    return {Block(std::move(columns)), makeSortDescription(key_count)};
}

SortWorkload makeMixedWorkload(size_t rows, size_t key_count, UInt64 seed, bool make_unsorted)
{
    ColumnsWithTypeAndName columns;
    columns.reserve(key_count);

    auto first_key = ColumnUInt64::create();
    for (size_t row = 0; row < rows; ++row)
    {
        const UInt64 value = make_unsorted && row == rows / 2 ? seed + row - 2 : seed + row;
        first_key->insert(value);
    }
    columns.emplace_back(std::move(first_key), std::make_shared<DataTypeUInt64>(), "key0");

    for (size_t key = 1; key < key_count; ++key)
    {
        auto value = ColumnUInt64::create();
        value->insert(seed + key);
        columns.emplace_back(ColumnConst::create(std::move(value), rows), std::make_shared<DataTypeUInt64>(), "key" + std::to_string(key));
    }

    return {Block(std::move(columns)), makeSortDescription(key_count)};
}

bool verifyIsAlreadySortedSemantics()
{
    auto all_constant = makeAllConstantWorkload(/*rows=*/8, /*key_count=*/4, /*seed=*/11);
    if (!isAlreadySorted(all_constant.block, all_constant.description))
    {
        std::cerr << "all-constant sort keys were not recognized as sorted\n";
        return false;
    }

    auto mixed_sorted = makeMixedWorkload(/*rows=*/8, /*key_count=*/4, /*seed=*/23, /*make_unsorted=*/false);
    if (!isAlreadySorted(mixed_sorted.block, mixed_sorted.description))
    {
        std::cerr << "sorted non-constant leading key was not recognized as sorted\n";
        return false;
    }

    auto mixed_unsorted = makeMixedWorkload(/*rows=*/8, /*key_count=*/4, /*seed=*/37, /*make_unsorted=*/true);
    if (isAlreadySorted(mixed_unsorted.block, mixed_unsorted.description))
    {
        std::cerr << "unsorted non-constant leading key was recognized as sorted\n";
        return false;
    }

    return true;
}

UInt64 runtimeSeed()
{
    return static_cast<UInt64>(std::chrono::steady_clock::now().time_since_epoch().count()) & 0xFFFF;
}

void benchmarkAllConstant(benchmark::State & state, size_t rows, size_t key_count)
{
    auto workload = makeAllConstantWorkload(rows, key_count, runtimeSeed());

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(workload.block);
        const bool already_sorted = isAlreadySorted(workload.block, workload.description);
        benchmark::DoNotOptimize(already_sorted);

        if (!already_sorted)
        {
            state.SkipWithError("all-constant sort keys must be sorted");
            break;
        }
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(rows));
    state.counters["rows"] = static_cast<double>(rows);
    state.counters["keys"] = static_cast<double>(key_count);
}

void benchmarkMixedSorted(benchmark::State & state, size_t rows, size_t key_count)
{
    auto workload = makeMixedWorkload(rows, key_count, runtimeSeed(), /*make_unsorted=*/false);

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(workload.block);
        const bool already_sorted = isAlreadySorted(workload.block, workload.description);
        benchmark::DoNotOptimize(already_sorted);

        if (!already_sorted)
        {
            state.SkipWithError("sorted mixed keys must be sorted");
            break;
        }
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(rows));
    state.counters["rows"] = static_cast<double>(rows);
    state.counters["keys"] = static_cast<double>(key_count);
}

void registerBenchmarks()
{
    benchmark::RegisterBenchmark("is_already_sorted/all_const/65536_rows/1_keys", [](benchmark::State & state) { benchmarkAllConstant(state, 65536, 1); })
        ->Unit(benchmark::kNanosecond);
    benchmark::RegisterBenchmark("is_already_sorted/all_const/65536_rows/4_keys", [](benchmark::State & state) { benchmarkAllConstant(state, 65536, 4); })
        ->Unit(benchmark::kNanosecond);
    benchmark::RegisterBenchmark("is_already_sorted/all_const/262144_rows/1_keys", [](benchmark::State & state) { benchmarkAllConstant(state, 262144, 1); })
        ->Unit(benchmark::kNanosecond);
    benchmark::RegisterBenchmark("is_already_sorted/all_const/262144_rows/4_keys", [](benchmark::State & state) { benchmarkAllConstant(state, 262144, 4); })
        ->Unit(benchmark::kNanosecond);
    benchmark::RegisterBenchmark("is_already_sorted/all_const/1048576_rows/1_keys", [](benchmark::State & state) { benchmarkAllConstant(state, 1048576, 1); })
        ->Unit(benchmark::kNanosecond);
    benchmark::RegisterBenchmark("is_already_sorted/all_const/1048576_rows/4_keys", [](benchmark::State & state) { benchmarkAllConstant(state, 1048576, 4); })
        ->Unit(benchmark::kNanosecond);
    benchmark::RegisterBenchmark("is_already_sorted/mixed_sorted/1048576_rows/4_keys", [](benchmark::State & state) { benchmarkMixedSorted(state, 1048576, 4); })
        ->Unit(benchmark::kNanosecond);
}

}

int main(int argc, char ** argv)
{
    if (!verifyIsAlreadySortedSemantics())
        return 1;

    if (argc == 2 && std::string_view(argv[1]) == "--verify")
        return 0;

    registerBenchmarks();
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    return 0;
}
