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
#include <iomanip>


struct Task {
    int id;
    int duration_seconds;
    std::chrono::steady_clock::time_point arrival_time;
    std::function<void()> execute;

    bool operator<(const Task& other) const {
        return duration_seconds > other.duration_seconds;
    }
};

class PriorityThreadPool {
private:
    std::vector<std::thread> workers;
    std::priority_queue<Task> tasks;

    std::mutex queue_mutex;
    std::condition_variable_any condition;

    
    std::atomic<bool> m_terminated{ false };
    std::atomic<bool> m_paused{ false };
    std::atomic<bool> m_initialized{ false };

   
    std::atomic<long long> total_wait_ms{ 0 };
    std::atomic<long long> total_exec_ms{ 0 };
    std::atomic<int> completed_tasks{ 0 };
    std::atomic<long long> total_queue_length{ 0 };
    std::atomic<int> queue_length_samples{ 0 };

public:
    PriorityThreadPool(size_t threads = 4) {
        initialize(threads);
    }

    ~PriorityThreadPool() {
        terminate(true);
    }

    void initialize(size_t worker_count) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (m_initialized) return;

        for (size_t i = 0; i < worker_count; ++i) {
            workers.emplace_back(&PriorityThreadPool::worker_routine, this);
        }
        m_initialized = true;
    }

    void add_task(int id, int duration_sec) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (m_terminated) return;

            Task new_task;
            new_task.id = id;
            new_task.duration_seconds = duration_sec;
            new_task.arrival_time = std::chrono::steady_clock::now();
            new_task.execute = [id, duration_sec]() {
                std::this_thread::sleep_for(std::chrono::seconds(duration_sec));
                };

            tasks.push(new_task);

            total_queue_length += tasks.size();
            queue_length_samples++;
        }
        condition.notify_one(); 
    }

    void pause() { m_paused = true; }
    void resume() {
        m_paused = false;
        condition.notify_all();
    }

    void terminate(bool finish_remaining_tasks = true) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (m_terminated) return;
            m_terminated = true;

            if (!finish_remaining_tasks) {
                while (!tasks.empty()) tasks.pop();
            }
        }
        condition.notify_all();
        for (std::thread& worker : workers) {
            if (worker.joinable()) worker.join();
        }
        m_initialized = false;
    }

    void print_statistics() {
        std::cout << "\n--- Статистика виконання ---" << std::endl;
        if (completed_tasks > 0) {
            double avg_wait = (total_wait_ms.load() / 1000.0) / completed_tasks.load();
            double avg_exec = (total_exec_ms.load() / 1000.0) / completed_tasks.load();
            double avg_queue = (double)total_queue_length / queue_length_samples;

            std::cout << "Виконано задач: " << completed_tasks << std::endl;
            std::cout << "Середній час очікування в черзі: " << std::fixed << std::setprecision(2) << avg_wait << " сек." << std::endl;
            std::cout << "Середній час виконання задачі: " << avg_exec << " сек." << std::endl;
            std::cout << "Середня довжина черги: " << avg_queue << std::endl;
        }
        else {
            std::cout << "Задачі не були виконані." << std::endl;
        }
    }

private:
    void worker_routine() {
        while (true) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                condition.wait(lock, [this] {
                    return m_terminated || (!tasks.empty() && !m_paused);
                    });

                if (m_terminated && tasks.empty()) return;

                task = tasks.top();
                tasks.pop();
            }

            auto wait_end = std::chrono::steady_clock::now();
            total_wait_ms += std::chrono::duration_cast<std::chrono::milliseconds>(wait_end - task.arrival_time).count();

            std::cout << "[Пул] Потік " << std::this_thread::get_id() << " взяв задачу #" << task.id
                << " (пріоритет: " << task.duration_seconds << "с)\n";

            auto exec_start = std::chrono::steady_clock::now();
            task.execute();
            auto exec_end = std::chrono::steady_clock::now();

            total_exec_ms += std::chrono::duration_cast<std::chrono::milliseconds>(exec_end - exec_start).count();
            completed_tasks++;

            std::cout << "[Пул] Задача #" << task.id << " завершена.\n";
        }
    }
};

void producer_routine(PriorityThreadPool& pool, int start_id, int count) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(5, 10);

    for (int i = 0; i < count; ++i) {
        int duration = dist(gen);
        pool.add_task(start_id + i, duration);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

int main() {

    std::cout << "Запуск пулу потоків (Варіант 9: 4 потоки, Пріоритетна черга)\n";
    PriorityThreadPool pool(4);

    std::thread p1(producer_routine, std::ref(pool), 100, 5);
    std::thread p2(producer_routine, std::ref(pool), 200, 5);

    p1.join();
    p2.join();

    std::this_thread::sleep_for(std::chrono::seconds(30));

    pool.terminate(true);
    pool.print_statistics();

    return 0;
}
