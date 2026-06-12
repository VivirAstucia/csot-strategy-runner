#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>

#include "strategy.hpp"
#include "histogram.hpp"

namespace csot {
    class Engine {
        public:
            explicit Engine();
            void load_ticks(std::string csv_path);
            void print_histogram() const;
            void run(Strategy& strategy);
            
        private:
            std::vector<Tick> ticks;
            std::string DEFAULT_SYMBOLS[4] = {"SYM0", "SYM1", "SYM2", "SYM3"};
            csot::LatencyHistogram latency_histogram;
    };
}