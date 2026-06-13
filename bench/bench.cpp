#include <benchmark/benchmark.h>

#include <dlfcn.h>
#include <memory>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

#include "strategy.hpp"

constexpr std::string_view DEFAULT_SYMBOLS[] = {"SYM0", "SYM1", "SYM2", "SYM3"};
constexpr int32_t symbolc = 4;

static std::unique_ptr<csot::Strategy> load_strategy(const char* so_path) {
    void* handle = dlopen(so_path, RTLD_NOW);
    if (!handle) throw std::runtime_error(dlerror());

    using create_fn_t = csot::Strategy*(*)();

    auto create_fn =
        reinterpret_cast<create_fn_t>(
            dlsym(handle, "create_strategy"));

    if (!create_fn)
        throw std::runtime_error(dlerror());

    return std::unique_ptr<csot::Strategy>(create_fn());
};

std::vector<csot::Tick> load_ticks(std::string csv_path) {
    std::vector<csot::Tick> ticks;
    std::ifstream file(csv_path);
    if (!file) {
        std::cerr << "Can't open csv: " << csv_path << std::endl;
        return {};
    }
    
    std::string line;
    std::getline(file, line);
    while (std::getline(file, line)) {
        std::stringstream stream(line);
        std::string timestamp, symbol, bid_px, ask_px, bid_qty, ask_qty;
        std::getline(stream, timestamp, ',');
        std::getline(stream, symbol, ',');
        std::getline(stream, bid_px, ',');
        std::getline(stream, ask_px, ',');
        std::getline(stream, bid_qty, ',');
        std::getline(stream, ask_qty, ',');
        
        std::string_view sv = DEFAULT_SYMBOLS[(symbol[3]-'0')];

        ticks.emplace_back(
            std::stoull(timestamp), sv, 
            std::stod(bid_px), std::stod(ask_px), std::stoul(bid_qty), std::stoul(ask_qty)
        );
    }
    return ticks;
}

static void BM_OnTick(benchmark::State& state) {
    static auto strategy = load_strategy("./build/spec_strategy.so");
    static auto ticks = load_ticks("./data/synthetic_large1.csv");

    strategy->on_init();
    std::size_t idx = 0;

    for (auto _ : state) {
        const auto& tick = ticks[idx];

        benchmark::DoNotOptimize(tick);

        auto orders = strategy->on_tick(tick);

        benchmark::DoNotOptimize(orders);

        idx++;
        if (idx == ticks.size())
            idx = 0;
    }
}

BENCHMARK(BM_OnTick)
    ->Unit(benchmark::kNanosecond)
    ->MinWarmUpTime(3)
    ->Iterations(10000000);

BENCHMARK_MAIN();