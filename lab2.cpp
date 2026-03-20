#include <algorithm>
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

int main() {
    const std::size_t threadCount = std::thread::hardware_concurrency();
    const std::vector<std::size_t> sizes = { 100'000, 500'000, 1'000'000 };

    for (std::size_t size : sizes) {
        std::cout << "Data size: " << size << "\n";
        const auto data = generateData(size);
        const auto seq = sequentialSumAndMin(data);
        const auto mtx = parallelWithMutex(data, threadCount);

        std::cout << "Seq time: " << seq.milliseconds << " ms\n";
        std::cout << "Mtx time: " << mtx.milliseconds << " ms\n\n";
    }
    return 0;
}
