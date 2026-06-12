#include <dlfcn.h>

#include "strategy.hpp"
#include "engine.hpp"

int32_t main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Incorrect arguments. Usage: " << argv[0] << " <strategy_so_path> <csv_path>" << std::endl;
        return 1;
    }

    std::string csv_path = argv[2], strategy_path = argv[1];

    void* strat_handle = dlopen(strategy_path.c_str(), RTLD_NOW);
    if (!strat_handle) {
        std::cerr << "Error opening " << strategy_path << ": " << dlerror() << std::endl;
        return 1;
    }

    auto create_strategy = (csot::Strategy*(*)())dlsym(strat_handle, "create_strategy");
    if (!create_strategy) {
        std::cerr << "Error loading symbol create_strategy: " << dlerror() << std::endl;
        return 1;
    }

    csot::Strategy* strategy = create_strategy();
    csot::Engine engine;
    engine.load_ticks(csv_path);
    engine.run(*strategy);
    engine.print_histogram();

    delete strategy;
    dlclose(strat_handle);
    return 0;
}