#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <climits>
#include <iomanip>

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

int main() {
    const std::vector<std::size_t> sizes = {100'000, 500'000, 1'000'000};
    for (std::size_t size : sizes) {
        auto data = generateData(size);
        auto seq = sequentialSumAndMin(data);
        std::cout << "Size: " << size << " | Sum: " << seq.sum << " | Time: " << seq.milliseconds << "ms\n";
    }
    return 0;
}
