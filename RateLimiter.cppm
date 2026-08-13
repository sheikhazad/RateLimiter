
//File    : RateLimiter.cppm
//Module  : RateLimiter
//Classes : FixedWindowRateLimiter
//          SlidingWindowRateLimiter

module; //global module fragment
#include <chrono>
#include <cstddef>

//export module RateLimiter does 2 things:
//1. Declares that this source file is a module interface unit.
//2. Gives the module its name: RateLimiter.
export module RateLimiter;

/* ============================================================
// 1. Fixed Window Rate Limiter

Major problem with this approach is that it can allow bursts of traffic at the edges of the windows.
Say, Fixed Window: 100 requests / second

Each window is exactly 1 second:

Window 1
[12:00:00.000 ───────────────── 12:00:01.000)
                                  ↑
                              boundary

Suppose we send all 100 requests near the end of Window 1:

Window 1
[12:00:00.000 ───────────────── 12:00:01.000)
                              ███████████
                              100 requests
                                  ↑
                            12:00:00.999

Then the window changes at:

12:00:01.000
        ↑
     boundary

The counter resets.

Now we immediately send another 100 requests at the beginning of Window 2:

Window 2
[12:00:01.000 ───────────────── 12:00:02.000)
███████████
100 requests
↑
12:00:01.001
Putting them together:

                  Window 1                         Window 2
        [12:00:00.000 ─────── 12:00:01.000) [12:00:01.000 ─────── 12:00:02.000)
                              ███████████│███████████
                              100        │100
                              requests   │requests
                                         ↑
                                      boundary

So we have:
12:00:00.999  → 100 requests
12:00:01.001  → 100 requests

Therefore:
100 + 100 = 200 requests in approximately:
12:00:01.001 - 12:00:00.999
≈ 2 milliseconds while the configured limit is: 100 requests / second
// ============================================================*/
export class FixedWindowRateLimiter
{
private:
    using Clock = std::chrono::steady_clock;

    const std::size_t _limitPerWindow;
    const std::chrono::milliseconds _window;

    std::size_t _request_count{0};
    Clock::time_point _windowstart_tp{Clock::now()};

public:
    FixedWindowRateLimiter(std::size_t limitPerWindow, std::chrono::milliseconds window)
        : _limitPerWindow(limitPerWindow), _window(window)
    {
    }

    bool allow()
    {
        const auto now = Clock::now();

        if (now - _windowstart_tp >= _window) {
            _windowstart_tp = now;
            _request_count = 0;
        }

        if (_request_count < _limitPerWindow) {
            ++_request_count;
            return true;
        }

        return false;
    }
};

// ============================================================
// 2. Sliding Window Rate Limiter
// ============================================================
export class SlidingWindowRateLimiter
{
private:
    using Clock = std::chrono::steady_clock;

    const std::size_t _limitPerWindow;
    const std::chrono::milliseconds _window;

    std::deque<Clock::time_point> _requestTimes_tp_dq;

public:
    SlidingWindowRateLimiter(
        std::size_t limitPerWindow,
        std::chrono::milliseconds window)
        : _limitPerWindow(limitPerWindow),
          _window(window)
    {
    }

    bool allow()
    {
        const auto now = Clock::now();

        // Remove requests outside the sliding window.
        while (!_requestTimes_tp_dq.empty() &&
               now - _requestTimes_tp_dq.front() >= _window)
        {
            _requestTimes_tp_dq.pop_front();
        }

        // Window still contains the maximum number of requests.
        if (_requestTimes_tp_dq.size() >= _limitPerWindow) {
            return false;
        }

        // Record this request.
        _requestTimes_tp_dq.push_back(now);

        return true;
    }
};