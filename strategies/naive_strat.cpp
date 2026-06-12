#include "strategy.hpp"

class AlwaysBid : public csot::Strategy {
    public:
        std::vector<csot::Order> on_tick(const csot::Tick& tick) {
            return {
                csot::Order{
                    csot::Order::Side::BUY,
                    tick.symbol, tick.ask_px, 1
                }
            };
        }
};

extern "C" csot::Strategy* create_strategy() {
    return new AlwaysBid();
}