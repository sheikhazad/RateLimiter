
//File    : RateLimiter.cppm
//Module  : RateLimiter
//Classes : FixedWindowRateLimiter
//          SlidingWindowRateLimiter

module; //global module fragment
#include <chrono>
#include <cstddef>
#include <algorithm>
#include <mutex> //For lock_guard and std::mutex

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
    const std::chrono::milliseconds _window_ms;

    std::size_t _request_count{0};
    Clock::time_point _windowstart_tp{Clock::now()};

public:
    FixedWindowRateLimiter(std::size_t limitPerWindow, std::chrono::milliseconds window_ms)
        : _limitPerWindow(limitPerWindow), _window_ms(window_ms)
    {
    }

    bool allow()
    {
        const auto now = Clock::now();

        if (now - _windowstart_tp >= _window_ms) {
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
//    Problem: Problem with Sliding Window is that it requires storing timestamps of all requests within the window, 
//    which can consume more memory and may not be efficient for high traffic scenarios. 
//    However, it provides a more accurate rate limiting mechanism compared to Fixed Window, 
//    as it allows for a smoother distribution of requests over time.
// ============================================================
export class SlidingWindowRateLimiter
{
private:
    using Clock = std::chrono::steady_clock;

    const std::size_t _limitPerWindow;
    const std::chrono::milliseconds _window_ms;

    std::deque<Clock::time_point> _requestTimes_tp_dq;

public:
    SlidingWindowRateLimiter(
        std::size_t limitPerWindow,
        std::chrono::milliseconds window_ms)
        : _limitPerWindow(limitPerWindow),
          _window_ms(window_ms)
    {
    }

    bool allow()
    {
        const auto now = Clock::now();

        // Remove requests outside the sliding window.
        while (!_requestTimes_tp_dq.empty() &&
               now - _requestTimes_tp_dq.front() >= _window_ms)
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



// ============================================================
// 3. Token Bucket Rate Limiter
// Bucket contains tokens limited by a maximum capacity. Each request consumes a token, and tokens are replenished(filled) at a fixed rate.
// Allows bursts, up to _capacity, but the average rate is limited by _refillRate.
// ============================================================

export class TokenBucketRateLimiter
{
private:
    using Clock = std::chrono::steady_clock;

    const std::size_t _capacity;
    const double _refillRate; // tokens per second

    double _tokens;
    Clock::time_point _lastRefill_tp;

public:
    TokenBucketRateLimiter(
        std::size_t capacity,
        double refillRate)
        : _capacity(capacity),
          _refillRate(refillRate),
          _tokens(static_cast<double>(capacity)),
          _lastRefill_tp(Clock::now())
    {
    }

    bool allow()
    {
        const auto now = Clock::now();

        // Calculate elapsed time since the last refill.
        const std::chrono::duration<double> elapsed =
            now - _lastRefill_tp;

        // Calculate how many tokens should have been generated.
        const double tokensToAdd =
            elapsed.count() * _refillRate;

        // Refill the bucket, but never exceed capacity.
        _tokens = std::min(
            static_cast<double>(_capacity),
            _tokens + tokensToAdd);

        _lastRefill_tp = now;

        // Need at least one token for this request.
        if (_tokens >= 1.0) {
            _tokens -= 1.0;
            return true;
        }

        return false;
    }

    double getTokens() const
    {
        return _tokens;
    }
};

// ============================================================
// 4. Leaky Bucket Rate Limiter
//
// The bucket contains requests(_bucketLevel) up to a maximum capacity.
// Each accepted request is added to the bucket, increasing bucket level, 
// and requests leave (leak) from the bucket at a fixed rate, creating more room to accept further requests in bucket.
//
// Allows bursts up to _capacity, while limiting the rate
// at which requests leave the bucket to _leakRate.
// ============================================================

export class LeakyBucketRateLimiter
{
private:
    using Clock = std::chrono::steady_clock;

    const std::size_t _capacity; // maximum number of requests that can be held in the bucket
    const double _leakRate; // requests per second leaking out of the bucket

    double _bucketLevel{0.0}; // current number of requests in the bucket
    Clock::time_point _lastLeak_tp{Clock::now()};

public:
    LeakyBucketRateLimiter(
        std::size_t capacity,
        double leakRate)
        : _capacity(capacity),
          _leakRate(leakRate)
    {
    }

    bool allow()
    {
        const auto now = Clock::now();

        // Calculate elapsed time since the last leak.
        const std::chrono::duration<double> elapsed =
            now - _lastLeak_tp;

        // Calculate how many requests should have leaked.
        const double requestsToLeak =
            elapsed.count() * _leakRate;

        // Remove leaked requests from the bucket, minimum bucket level is 0 (empty bucket).
        _bucketLevel = std::max(0.0, _bucketLevel - requestsToLeak);

        _lastLeak_tp = now;

        // Will adding one more request exceed the bucket capacity?
        if (_bucketLevel + 1.0 > static_cast<double>(_capacity)) {
            return false;
        }

        // Add this request to the bucket.
        _bucketLevel += 1.0;

        return true;
    }

    double getBucketLevel() const
    {
        return _bucketLevel;
    }
};

// ============================================================
// 5. Thread-Safe Token Bucket Rate Limiter
//
// Same Token Bucket logic, but allow() is protected by a mutex
// so multiple threads cannot modify _tokens and _lastRefill_tp
// concurrently.
// ============================================================

export class ThreadSafeTokenBucketRateLimiter
{
private:
    using Clock = std::chrono::steady_clock;

    const std::size_t _capacity;
    const double _refillRate; // tokens per second

    double _tokens{0.0};
    Clock::time_point _lastRefill_tp{Clock::now()};

    std::mutex _mutex;

public:
    ThreadSafeTokenBucketRateLimiter(
        std::size_t capacity,
        double refillRate)
        : _capacity(capacity),
          _refillRate(refillRate),
          _tokens(static_cast<double>(capacity))
    {
    }

    bool allow()
    {
        std::lock_guard<std::mutex> lock(_mutex); // It's the Only addition in TokenBucketRateLimiter to make ThreadSafeTokenBucketRateLimiter

        const auto now = Clock::now();

        // Calculate elapsed time since the last refill.
        const std::chrono::duration<double> elapsed =
            now - _lastRefill_tp;

        // Calculate how many tokens should have been generated.
        const double tokensToAdd =
            elapsed.count() * _refillRate;

        // Refill the bucket, but never exceed capacity.
        _tokens = std::min(
            static_cast<double>(_capacity),
            _tokens + tokensToAdd);

        _lastRefill_tp = now;

        // Need at least one token for this request.
        if (_tokens >= 1.0) {
            _tokens -= 1.0;
            return true;
        }

        return false;
    }

    double getTokens() 
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _tokens;
    }
};


// ============================================================
// 6. Lock-Free Token Bucket Rate Limiter using CAS
//
// Same Token Bucket logic, but using CAS.
//
// _tokens and _lastRefill_tp are wrapped inside State because
// both values represent one logical state and must be updated
// atomically together.
// ============================================================
export class LockFreeTokenBucketRateLimiter
{
private:
    using Clock = std::chrono::steady_clock;

    struct State
    {
        double _tokens;
        Clock::time_point _lastRefill_tp;
    };

    const std::size_t _capacity;
    const double _refillRate;

    std::atomic<State> _state;

public:
    LockFreeTokenBucketRateLimiter(
        std::size_t capacity,
        double refillRate)
        : _capacity(capacity),
          _refillRate(refillRate),
          _state({static_cast<double>(capacity),
                  Clock::now()})
    {
    }

    bool allow()
    {

        // No need to be inside the CAS loop because there is no risk of
        // using a stale state. On CAS failure, compare_exchange_weak()
        // updates oldState with the latest state, and we recalculate based
        // on that.
        State oldState =
            _state.load(std::memory_order_relaxed);

        while (true)
        {
            const auto now = Clock::now();

            // Calculate elapsed time since the last refill.
            const std::chrono::duration<double> elapsed =
                now - oldState._lastRefill_tp;

            // Calculate how many tokens should have been generated.
            const double tokensToAdd =
                elapsed.count() * _refillRate;

            // Refill the bucket, but never exceed capacity.
            const double newTokens =
                std::min(
                    static_cast<double>(_capacity),
                    oldState._tokens + tokensToAdd);

            // Need at least one token.
            if (newTokens < 1.0)
            {
                State newState{
                    newTokens,
                    now
                };

                //The atomic state itself provides the required atomicity and modification ordering. 
                //We don't need release/acquire synchronization because the state isn't being used to publish other non-atomic
                //data to another thread. Each thread only needs to atomically load the latest state and use CAS to update it.
                if (_state.compare_exchange_weak(
                        oldState,
                        newState,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed))
                {
                    return false;
                }

                continue;
            }

            // Consume one token.
            State newState{
                newTokens - 1.0,
                now
            };

            // Try to atomically replace the complete state.
            if (_state.compare_exchange_weak(
                    oldState,
                    newState,
                    std::memory_order_relaxed, // Same reason as above, we don't need release/acquire synchronization here.
                    std::memory_order_relaxed))
            {
                return true;
            }

            // CAS failed.
            // oldState now contains the latest state.
            // Recalculate and retry.
        }
    }

    double getTokens() const
    {
        return _state.load(
                   std::memory_order_relaxed)
            ._tokens;
    }
};