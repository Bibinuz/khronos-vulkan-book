#include "triapp.hpp"
#include <cstdlib>
#include <print>

int main() {
    try {
        vke::TriApp app{};
        app.run();
    } catch (const std::exception &e) {
        std::println(stderr, "'{}'", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
