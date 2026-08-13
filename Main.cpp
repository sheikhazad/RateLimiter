import RateLimiter;

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    using namespace std::chrono_literals;

    // ========================================================
    // Fixed Window
    // ========================================================

    std::cout << "===== Fixed Window =====\n";

    FixedWindowRateLimiter fixed(
        5,
        1s);

    for (int i = 1; i <= 7; ++i) {
        std::cout << "Request " << i
                  << ": "
                  << (fixed.allow() ? "ALLOWED" : "REJECTED")
                  << '\n';
    }


    // ========================================================
    // Sliding Window
    // ========================================================

    std::cout << "\n===== Sliding Window =====\n";

    SlidingWindowRateLimiter sliding(
        5,
        1s);

    for (int i = 1; i <= 7; ++i) {
        std::cout << "Request " << i
                  << ": "
                  << (sliding.allow() ? "ALLOWED" : "REJECTED")
                  << '\n';
    }

    return 0;
}