#include <sstream>
#include <fstream>
#include <chrono>

#include "engine.hpp"

namespace csot {
    Engine::Engine() = default; 


    void Engine::load_ticks(std::string csv_path) {
        // TODO : memory-mapped parsing/or some high performance parsing library
        std::ifstream file(csv_path);
        if (!file) {
            std::cerr << "Can't open csv: " << csv_path << std::endl;
            return;
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
    }

    void Engine::run(Strategy& strategy) {
        strategy.on_init();
        for (const Tick& tick : ticks) {
            auto start = std::chrono::high_resolution_clock::now();
            std::vector<Order> orders = strategy.on_tick(tick);
            auto end = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            latency_histogram.record(latency);

            for (const Order& order : orders) {
                strategy.on_fill(order, order.price, order.qty);
            }
        }
    }

    void Engine::print_histogram() const {
        latency_histogram.print(std::cout);
    }
}