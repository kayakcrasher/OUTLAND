#include <exception>
#include <iostream>

void test_health();
void test_player();
void test_world();
void test_game_time();

int main() {
    std::cout << "================================\n";
    std::cout << "      OUTLAND SELF TESTS        \n";
    std::cout << "================================\n";

    try {
        test_health();
        std::cout << "[PASS] Health\n";

        test_player();
        std::cout << "[PASS] Player\n";

        test_world();
        std::cout << "[PASS] World\n";

        test_game_time();
        std::cout << "[PASS] GameTime\n";
    }
    catch (const std::exception& error) {
        std::cerr << "[FAIL] "
                  << error.what()
                  << '\n';

        return 1;
    }

    std::cout << "================================\n";
    std::cout << "       ALL TESTS GREEN          \n";
    std::cout << "================================\n";

    return 0;
}
