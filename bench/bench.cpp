// bench/bench.cpp
#include <chrono>
#include <cctype>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "histogram.hpp"
#include "strategy.hpp"

static std::string g_strategy_so;
static std::string g_ticks_csv = "data/synthetic_small.csv";
static const char* DEFAULT_SYMBOLS_C[] = {"SYM0", "SYM1", "SYM2", "SYM3"};

static std::string resolve_strategy_path(const std::string& strategy_so) {
    if (std::filesystem::exists(strategy_so)) {
        return strategy_so;
    }

    const std::string build_path = "build/" + strategy_so;
    if (std::filesystem::exists(build_path)) {
        return build_path;
    }

    const std::string local_path = "./" + strategy_so;
    if (std::filesystem::exists(local_path)) {
        return local_path;
    }

    return strategy_so;
}

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

        if (symbol.empty()) {
            continue;
        }

        try {
            csot::Tick tick{};
            tick.timestamp_ns = std::stoull(timestamp);
            int idx = 0;
            if (symbol.size() > 3 && std::isdigit(static_cast<unsigned char>(symbol[3]))) {
                idx = (symbol[3] - '0') % 4;
            }
            tick.symbol = std::string_view(DEFAULT_SYMBOLS_C[idx]);
            tick.bid_px = std::stod(bid_px);
            tick.ask_px = std::stod(ask_px);
            tick.bid_qty = static_cast<uint32_t>(std::stoul(bid_qty));
            tick.ask_qty = static_cast<uint32_t>(std::stoul(ask_qty));
            ticks.push_back(tick);
        } catch (...) {
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <strategy.so> [ticks.csv]" << std::endl;
        return 2;
    }

    g_strategy_so = argv[1];
    if (argc >= 3) {
        g_ticks_csv = argv[2];
    }

    std::vector<csot::Tick> ticks;
    std::vector<std::string> symbol_storage;
    load_ticks(g_ticks_csv, ticks, symbol_storage);
    if (ticks.empty()) {
        std::cerr << "no ticks loaded" << std::endl;
        return 1;
    }

    const std::string so_path = resolve_strategy_path(g_strategy_so);
    void* handle = dlopen(so_path.c_str(), RTLD_NOW);
    if (!handle) {
        std::cerr << "dlopen failed: " << dlerror() << " (tried '" << so_path << "')" << std::endl;
        return 1;
    }

    using create_fn_t = csot::Strategy*(*)();
    auto create_fn = reinterpret_cast<create_fn_t>(dlsym(handle, "create_strategy"));
    if (!create_fn) {
        std::cerr << "dlsym(create_strategy) failed: " << dlerror() << std::endl;
        dlclose(handle);
        return 1;
    }

    {
        std::unique_ptr<csot::Strategy> strategy(create_fn());
        strategy->on_init();

        csot::LatencyHistogram histogram;
        for (const csot::Tick& tick : ticks) {
            const auto start = std::chrono::steady_clock::now();
            std::vector<csot::Order> orders = strategy->on_tick(tick);
            const auto stop = std::chrono::steady_clock::now();

            const auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
            histogram.record(static_cast<std::uint64_t>(latency));

            for (const csot::Order& order : orders) {
                strategy->on_fill(order, order.price, order.qty);
            }
        }
        std::ifstream freq_file(
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");

        long freq_khz{};
        if (freq_file >> freq_khz) {
            std::cout << "CPU freq: "
                    << (freq_khz / 1000.0)
                    << " MHz\n";
        }
        histogram.print(std::cout);
    }

    dlclose(handle);
    return 0;
}