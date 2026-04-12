#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <random>
#include <atomic>

struct Task {
    int id;
    int duration_seconds;
    std::function<void()> execute;

    bool operator<(const Task& other) const {
        return duration_seconds > other.duration_seconds;
    }
};
