import RateLimiter;

#include <chrono>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>

int main()
{
    // For convenience, we can use the chrono literals for seconds, milliseconds, etc.
    // 
    using namespace std::chrono_literals;

    // ========================================================
    // 1. Fixed Window
    // ========================================================

    std::cout << "===== 1. Fixed Window =====\n";

    //FixedWindowRateLimiter fixed(5, std::chrono::seconds(1));
    FixedWindowRateLimiter fixed(5, 1s);

    for (int i = 1; i <= 7; ++i) {
        std::cout << "Request " << i
                  << ": "
                  << (fixed.allow() ? "ALLOWED" : "REJECTED")
                  << '\n';
    }


    // ========================================================
    // 2. Sliding Window
    // ========================================================

    std::cout << "\n===== 2. Sliding Window =====\n";

    SlidingWindowRateLimiter sliding(5, 1s);

    for (int i = 1; i <= 7; ++i) {
        std::cout << "Request " << i
                  << ": "
                  << (sliding.allow() ? "ALLOWED" : "REJECTED")
                  << '\n';
    }


    // ========================================================
    // 3. Token Bucket
    // ========================================================

    std::cout << "\n===== 3. Token Bucket =====\n";

    // Capacity    = 5 tokens
    // Refill rate = 2 tokens / second
    TokenBucketRateLimiter tokenBucket(5, 2.0);

    // --------------------------------------------------------
    // Initial burst
    //
    // Bucket initially contains 5 tokens.
    //
    // Request 1 -> ALLOWED (4 tokens remain)
    // Request 2 -> ALLOWED (3 tokens remain)
    // Request 3 -> ALLOWED (2 tokens remain)
    // Request 4 -> ALLOWED (1 token remains)
    // Request 5 -> ALLOWED (0 tokens remain)
    // Request 6 -> REJECTED
    // Request 7 -> REJECTED
    // --------------------------------------------------------

    std::cout << "\nInitial burst:\n";

    for (int i = 1; i <= 7; ++i) {
        std::cout << "Request " << i
                  << ": "
                  << (tokenBucket.allow() ? "ALLOWED" : "REJECTED")
                  << " : Tokens remaining: " << tokenBucket.getTokens()
                  << '\n';
    }


    // --------------------------------------------------------
    // Wait for tokens to refill.
    //
    // Refill rate = 2 tokens / second
    // Waiting      = 1 second
    //
    // Therefore approximately:
    //
    // 2 tokens will be added.
    // --------------------------------------------------------

    std::cout << "\nWaiting 1 second to generate more tokens to refill bucket, making more availibility for further requests...\n";

    std::this_thread::sleep_for(1s);


    // --------------------------------------------------------
    // After refill
    //
    // Approximately 2 tokens should now be available.
    //
    // Request 1 -> ALLOWED
    // Request 2 -> ALLOWED
    // Request 3 -> REJECTED
    // --------------------------------------------------------

    std::cout << "\nAfter 1 second:\n";

    for (int i = 1; i <= 3; ++i) {
        std::cout << "Request " << i
                  << ": "
                  << (tokenBucket.allow() ? "ALLOWED" : "REJECTED")
                  << " : Tokens remaining: " << tokenBucket.getTokens()
                  << '\n';
    }


    // ========================================================
    // 4. Leaky Bucket
    // ========================================================

    std::cout << "\n===== 4. Leaky Bucket =====\n";

    // Capacity   = 5 requests
    // Leak rate  = 2 requests / second
    LeakyBucketRateLimiter leakyBucket(5, 2.0);

    // --------------------------------------------------------
    // Initial burst
    //
    // The bucket can hold up to 5 requests.
    //
    // Request 1 -> ALLOWED (1 request in bucket)
    // Request 2 -> ALLOWED (2 requests in bucket)
    // Request 3 -> ALLOWED (3 requests in bucket)
    // Request 4 -> ALLOWED (4 requests in bucket)
    // Request 5 -> ALLOWED (5 requests in bucket)
    // Request 6 -> REJECTED (bucket full)
    // Request 7 -> REJECTED (bucket full)
    // --------------------------------------------------------

    std::cout << "\nInitial burst:\n";

    for (int i = 1; i <= 7; ++i) {
        std::cout << "Request " << i
                  << ": "
                  << (leakyBucket.allow() ? "ALLOWED" : "REJECTED")
                  << " : Requests in bucket: "
                  << leakyBucket.getBucketLevel()
                  << '\n';
    }


    // --------------------------------------------------------
    // Wait for requests to leak.
    //
    // Leak rate = 2 requests / second
    // Waiting   = 1 second
    //
    // Therefore approximately:
    //
    // 2 requests should leak from the bucket.
    // --------------------------------------------------------

    std::cout << "\nWaiting 1 second for requests to leak from bucket, creating more room to accept further request in bucket...\n";

    std::this_thread::sleep_for(1s);


    // --------------------------------------------------------
    // After 1 second
    //
    // Approximately 3 requests should remain in the bucket:
    //
    // 5 - 2 = 3
    //
    // Therefore approximately 2 new requests can be accepted.
    // --------------------------------------------------------

    std::cout << "\nAfter 1 second:\n";

    for (int i = 1; i <= 4; ++i) {
        std::cout << "Request " << i
                  << ": "
                  << (leakyBucket.allow() ? "ALLOWED" : "REJECTED")
                  << " : Requests in bucket: "
                  << leakyBucket.getBucketLevel()
                  << '\n';
    }


// ========================================================
// 5. Thread-Safe Token Bucket Rate Limiter
// ========================================================

std::cout << "\n===== 5. Thread-Safe Token Bucket =====\n";

// Capacity    = 10 tokens
// Refill rate = 0 tokens / second
//
// Starting with 10 tokens means exactly 10 requests
// should be allowed, regardless of which thread gets them.


/*
constexpr int numThreads = 4;
constexpr int requestsPerThread = 5;
ThreadSafeTokenBucketRateLimiter threadSafeBucket(10, 0.0);
*/

//Increase contention: For a more rigorous test, we can increase the number of threads and requests per thread.
constexpr int numThreads = 100;
constexpr int requestsPerThread = 1000;
ThreadSafeTokenBucketRateLimiter threadSafeBucket(100, 0.0);

/* RESULTS:
i) Without mutex:
Allowed : 130  ==> More than expected allowed requests due to race conditions
Rejected: 99870
Total   : 100000

i) With mutex:
Allowed : 100
Rejected: 99900
Total   : 100000
*/

std::atomic<int> allowed{0};
std::atomic<int> rejected{0};

std::vector<std::thread> threads;

for (int threadId = 0; threadId < numThreads; ++threadId) {

    threads.emplace_back([&]() {

        for (int i = 0; i < requestsPerThread; ++i) {

            if (threadSafeBucket.allow()) {
                ++allowed;
            }
            else {
                ++rejected;
            }
        }
    });
}

for (auto& thread : threads) {
    thread.join();
}

std::cout << "Allowed : " << allowed << '\n';
std::cout << "Rejected: " << rejected << '\n';
std::cout << "Total   : "
          << allowed + rejected
          << '\n';

// ========================================================
// 6. Lock-Free Token Bucket using CAS
// ========================================================

std::cout << "\n===== 6. Lock-Free Token Bucket using CAS =====\n";

// Capacity    = 100 tokens
// Refill rate = 0 tokens / second
//
// Starting with 100 tokens means exactly 100 requests
// should be allowed, regardless of which thread gets them.
//
// Increase contention:
// 100 threads × 1000 requests = 100,000 total requests.

constexpr int numThreadsCAS = 100;
constexpr int requestsPerThreadCAS = 1000;

LockFreeTokenBucketRateLimiter lockFreeBucket(100, 0.0);

std::atomic<int> allowedCAS{0};
std::atomic<int> rejectedCAS{0};

std::vector<std::thread> threadsCAS;

for (int threadId = 0;
     threadId < numThreadsCAS;
     ++threadId)
{
    threadsCAS.emplace_back([&]()
    {
        for (int i = 0;
             i < requestsPerThreadCAS;
             ++i)
        {
            if (lockFreeBucket.allow()) {
                ++allowedCAS;
            }
            else {
                ++rejectedCAS;
            }
        }
    });
}

for (auto& thread : threadsCAS) {
    thread.join();
}

std::cout << "Allowed : " << allowedCAS << '\n';
std::cout << "Rejected: " << rejectedCAS << '\n';
std::cout << "Total   : "
          << allowedCAS + rejectedCAS
          << '\n';


          
   return 0;
}