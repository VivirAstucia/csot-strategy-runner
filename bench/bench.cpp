// bench/bench.cpp
#include <benchmark/benchmark.h>
#include <dlfcn.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <filesystem>

#include "strategy.hpp"

static std::string g_strategy_so;
static std::string g_ticks_csv = "data/synthetic_small.csv";

static const char* DEFAULT_SYMBOLS_C[] = {"SYM0", "SYM1", "SYM2", "SYM3"};

static void load_ticks(const std::string& csv_path,
                       std::vector<csot::Tick>& ticks,
                       std::vector<std::string>& /*storage*/) {
    std::ifstream file(csv_path);
    if (!file) {
        std::cerr << "can't open csv: " << csv_path << std::endl;
        return;
    }

    std::string line;
    std::getline(file, line); // skip header
    while (std::getline(file, line)) {
        std::stringstream stream(line);
        std::string timestamp, symbol, bid_px, ask_px, bid_qty, ask_qty;
        std::getline(stream, timestamp, ',');
        std::getline(stream, symbol, ',');
        std::getline(stream, bid_px, ',');
        std::getline(stream, ask_px, ',');
        std::getline(stream, bid_qty, ',');
        std::getline(stream, ask_qty, ',');

        if (symbol.empty()) continue;

        csot::Tick t{};
        try {
            t.timestamp_ns = std::stoull(timestamp);
            int idx = 0;
            if (symbol.size() > 3 && std::isdigit(static_cast<unsigned char>(symbol[3]))) {
                idx = (symbol[3] - '0') % 4;
            }
            t.symbol = std::string_view(DEFAULT_SYMBOLS_C[idx]);
            t.bid_px = std::stod(bid_px);
            t.ask_px = std::stod(ask_px);
            t.bid_qty = static_cast<uint32_t>(std::stoul(bid_qty));
            t.ask_qty = static_cast<uint32_t>(std::stoul(ask_qty));
            ticks.push_back(t);
        } catch (...) {
            // skip malformed
        }
    }
}

static void BM_PerTick(benchmark::State& state) {
    static std::vector<csot::Tick> ticks;
    static std::vector<std::string> storage;
    static bool loaded = false;
    if (!loaded) {
        load_ticks(g_ticks_csv, ticks, storage);
        if (ticks.empty()) return;
        loaded = true;
    }

    // resolve strategy path if needed
    std::string so_path = g_strategy_so;
    if (!std::filesystem::exists(so_path)) {
        std::string alt = std::string("build/") + so_path;
        if (std::filesystem::exists(alt)) so_path = alt;
        else {
            std::string alt2 = std::string("./") + so_path;
            if (std::filesystem::exists(alt2)) so_path = alt2;
        }
    }

    // dlopen the strategy .so and create the strategy instance
    void* handle = dlopen(so_path.c_str(), RTLD_NOW);
    if (!handle) {
        std::cerr << "dlopen failed: " << dlerror() << " (tried '" << so_path << "')\n";
        return;
    }
    using create_fn_t = csot::Strategy*(*)();
    auto create_fn = (create_fn_t)dlsym(handle, "create_strategy");
    if (!create_fn) {
        std::cerr << "dlsym(create_strategy) failed: " << dlerror() << std::endl;
        dlclose(handle);
        return;
    }

    std::unique_ptr<csot::Strategy> strat(create_fn());
    strat->on_init();

    size_t idx = 0;
    for (auto _ : state) {
        const csot::Tick& t = ticks[idx++];
        auto orders = strat->on_tick(t);
        benchmark::DoNotOptimize(orders);
        if (idx == ticks.size()) idx = 0;
    }
    state.SetItemsProcessed(state.iterations());

    // cleanup
    strat.reset();
    dlclose(handle);
}

BENCHMARK(BM_PerTick)->UseRealTime();

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <strategy.so> [ticks.csv]" << std::endl;
        return 2;
    }
    g_strategy_so = argv[1];
    if (argc >= 3) g_ticks_csv = argv[2];

    // build argv for benchmark: keep argv[0] as program name, drop argv[1..2]
    std::vector<char*> bargs;
    bargs.push_back(argv[0]);
    for (int i = 3; i < argc; ++i) bargs.push_back(argv[i]);
    int bargc = static_cast<int>(bargs.size());
    char** bargv = bargs.empty() ? nullptr : bargs.data();
    benchmark::Initialize(&bargc, bargv);
    if (benchmark::ReportUnrecognizedArguments(bargc, bargv)) return 1;
    benchmark::RunSpecifiedBenchmarks();
    return 0;
}
