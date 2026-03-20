#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

struct CalcResult {
    long long sum = 0;
    int minValue = INT_MAX;
    bool found = false;
    double milliseconds = 0.0;
};

std::vector<int> generateData(std::size_t size, int minVal = 1, int maxVal = 1'000'000) {
    std::vector<int> data(size);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(minVal, maxVal);
    for (auto& x : data) x = dist(rng);
    return data;
}

CalcResult sequentialSumAndMin(const std::vector<int>& data) {
    const auto start = std::chrono::high_resolution_clock::now();
    CalcResult result;
    for (int value : data) {
        if (value % 10 == 0) {
            result.sum += value;
            if (!result.found || value < result.minValue) {
                result.minValue = value;
                result.found = true;
            }
        }
    }
    const auto end = std::chrono::high_resolution_clock::now();
    result.milliseconds = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

CalcResult parallelWithMutex(const std::vector<int>& data, std::size_t threadCount) {
    const auto start = std::chrono::high_resolution_clock::now();
    long long sharedSum = 0;
    int sharedMin = INT_MAX;
    bool found = false;
    std::mutex mtx;

    const std::size_t n = data.size();
    const std::size_t chunk = (n + threadCount - 1) / threadCount;
    std::vector<std::thread> workers;

    for (std::size_t t = 0; t < threadCount; ++t) {
        const std::size_t begin = t * chunk;
        const std::size_t end = std::min(begin + chunk, n);
        if (begin >= end) break;

        workers.emplace_back([&data, begin, end, &sharedSum, &sharedMin, &found, &mtx]() {
            for (std::size_t i = begin; i < end; ++i) {
                if (data[i] % 10 == 0) {
                    std::lock_guard<std::mutex> lock(mtx);
                    sharedSum += data[i];
                    if (!found || data[i] < sharedMin) {
                        sharedMin = data[i];
                        found = true;
                    }
                }
            }
        });
    }

    for (auto& th : workers) th.join();
    const auto end = std::chrono::high_resolution_clock::now();
    
    CalcResult result = {sharedSum, sharedMin, found, std::chrono::duration<double, std::milli>(end - start).count()};
    return result;
}

CalcResult parallelLockFreeCAS(const std::vector<int>& data, std::size_t threadCount) {
    const auto start = std::chrono::high_resolution_clock::now();

    std::atomic<long long> sum{ 0 };
    std::atomic<int> minValue{ INT_MAX };
    std::atomic<bool> found{ false };

    const std::size_t n = data.size();
    const std::size_t chunk = (n + threadCount - 1) / threadCount;
    std::vector<std::thread> workers;

    for (std::size_t t = 0; t < threadCount; ++t) {
        const std::size_t begin = t * chunk;
        const std::size_t end = std::min(begin + chunk, n);
        if (begin >= end) break;

        workers.emplace_back([&data, begin, end, &sum, &minValue, &found]() {
            for (std::size_t i = begin; i < end; ++i) {
                const int value = data[i];
                if (value % 10 == 0) {
                    found.store(true, std::memory_order_relaxed);
                    sum.fetch_add(value, std::memory_order_relaxed);

                    int expectedMin = minValue.load(std::memory_order_relaxed);
                    do {
                        if (value >= expectedMin) break;
                    } while (!minValue.compare_exchange_weak(expectedMin, value, std::memory_order_relaxed));
                }
            }
        });
    }

    for (auto& th : workers) th.join();
    const auto end = std::chrono::high_resolution_clock::now();

    CalcResult result = {sum.load(), minValue.load(), found.load(), std::chrono::duration<double, std::milli>(end - start).count()};
    return result;
}

void printResultRow(const char* mode, const CalcResult& result) {
    std::cout << std::left << std::setw(20) << mode
        << std::right << std::setw(16) << result.sum
        << std::setw(16) << (result.found ? std::to_string(result.minValue) : "N/A")
        << std::setw(14) << std::fixed << std::setprecision(3) << result.milliseconds
        << " ms\n";
}

int main() {
    const std::size_t threadCount = std::max<std::size_t>(2, std::thread::hardware_concurrency());
    const std::vector<std::size_t> sizes = { 100'000, 500'000, 1'000'000, 5'000'000 };

    std::cout << "Lab #2: Atomic operations and lock-free approach\n";
    std::cout << "Fixed thread count: " << threadCount << "\n\n";

    for (std::size_t size : sizes) {
        std::cout << "Data size: " << size << "\n";
        const auto data = generateData(size);

        const CalcResult seq = sequentialSumAndMin(data);
        const CalcResult mtx = parallelWithMutex(data, threadCount);
        const CalcResult cas = parallelLockFreeCAS(data, threadCount);

        std::cout << std::left << std::setw(20) << "Mode" << std::right << std::setw(16) << "Sum" 
                  << std::setw(16) << "Min %10" << std::setw(14) << "Time\n";
        std::cout << std::string(66, '-') << "\n";

        printResultRow("Sequential", seq);
        printResultRow("Parallel+Mutex", mtx);
        printResultRow("Parallel+CAS", cas);
        std::cout << "Validation: " << (seq.sum == cas.sum ? "OK" : "FAILED") << "\n\n";
    }
    return 0;
}
