// PO_lab1.cpp : Лабораторна робота №1 - Дослiдження базових операцiй з потоками виконання
// Завдання 9: Заповнити квадратну матрицю випадковими числами. 
// На головнiй дiагоналi розмiстити суми елементiв, якi лежать в тому ж рядку.

#include <iostream>
#include <vector>
#include <random>
#include <thread>
#include <chrono>
#include <iomanip>

using std::chrono::nanoseconds;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;


void printMatrix(const std::vector<std::vector<double>>& matrix, const std::string& title) {
    std::cout << "\n" << title << ":\n";
    if (matrix.size() > 10) {
        std::cout << "Матриця занадто велика для виведення (розмiр: " << matrix.size() << "x" << matrix.size() << ")\n";
        return;
    }
    for (const auto& row : matrix) {
        for (const auto& elem : row) {
            std::cout << std::setw(10) << std::fixed << std::setprecision(2) << elem << " ";
        }
        std::cout << "\n";
    }
}

void fillMatrixRandom(std::vector<std::vector<double>>& matrix, int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> dist(1.0, 100.0);

    for (size_t i = 0; i < matrix.size(); i++) {
        for (size_t j = 0; j < matrix[i].size(); j++) {
            matrix[i][j] = dist(gen);
        }
    }
}

void sequentialVersion(std::vector<std::vector<double>>& matrix) {
    size_t n = matrix.size();
    std::vector<double> rowSums(n);

    for (size_t row = 0; row < n; row++) {
        double sum = 0.0;
        for (size_t col = 0; col < n; col++) {
            sum += matrix[row][col];
        }
        rowSums[row] = sum;
    }

    for (size_t i = 0; i < n; i++) {
        matrix[i][i] = rowSums[i];
    }
}

void processRows(std::vector<std::vector<double>>& matrix,
    std::vector<double>& rowSums,
    size_t startRow, size_t endRow) {
    size_t n = matrix.size();

    for (size_t row = startRow; row < endRow; row++) {
        double sum = 0.0;
        for (size_t col = 0; col < n; col++) {
            sum += matrix[row][col];
        }
        rowSums[row] = sum;
    }
}

void parallelVersion(std::vector<std::vector<double>>& matrix, size_t numThreads) {
    size_t n = matrix.size();
    std::vector<double> rowSums(n);
    std::vector<std::thread> threads;

    size_t rowsPerThread = n / numThreads;
    size_t remainingRows = n % numThreads;

    size_t startRow = 0;
    for (size_t i = 0; i < numThreads; i++) {
        size_t endRow = startRow + rowsPerThread + (i < remainingRows ? 1 : 0);

        threads.emplace_back(processRows, std::ref(matrix), std::ref(rowSums), startRow, endRow);

        startRow = endRow;
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (size_t i = 0; i < n; i++) {
        matrix[i][i] = rowSums[i];
    }
}


void runTests() {
    unsigned int hardwareThreads = std::thread::hardware_concurrency();

    struct TestResult {
        size_t size;
        size_t threads;
        double timeComputation;
        double speedup;
        bool isSequential;
    };
    std::vector<TestResult> results;

    std::vector<size_t> matrixSizes = { 1000, 3000, 5000, 7000, 10000 };
    std::vector<size_t> threadCounts;
    if (hardwareThreads >= 2) {
        threadCounts.push_back(hardwareThreads / 2);
    }
    threadCounts.push_back(hardwareThreads);
    threadCounts.push_back(hardwareThreads * 2);
    threadCounts.push_back(hardwareThreads * 4);
    threadCounts.push_back(hardwareThreads * 8);
    threadCounts.push_back(hardwareThreads * 16);

    for (size_t size : matrixSizes) {
        // Warm-up для холодного старту
        {
            std::vector<std::vector<double>> warmupMatrix(size, std::vector<double>(size));
            fillMatrixRandom(warmupMatrix);
            sequentialVersion(warmupMatrix);
        }

        // Послідовна версія
        std::vector<std::vector<double>> matrixSeq(size, std::vector<double>(size));
        fillMatrixRandom(matrixSeq);

        auto seqBegin = high_resolution_clock::now();
        sequentialVersion(matrixSeq);
        auto seqEnd = high_resolution_clock::now();
        double seqTime = duration_cast<nanoseconds>(seqEnd - seqBegin).count() * 1e-9;

        volatile double seqResult = matrixSeq[0][0];
        seqResult = seqResult;

        results.push_back({ size, 0, seqTime, 0.0, true });

        // Паралельна версія
        for (size_t numThreads : threadCounts) {
            // Warm-up
            {
                std::vector<std::vector<double>> warmupMatrix(size, std::vector<double>(size));
                fillMatrixRandom(warmupMatrix);
                parallelVersion(warmupMatrix, numThreads);
            }

            std::vector<std::vector<double>> matrixPar(size, std::vector<double>(size));
            fillMatrixRandom(matrixPar);

            auto parBegin = high_resolution_clock::now();
            parallelVersion(matrixPar, numThreads);
            auto parEnd = high_resolution_clock::now();
            double parTime = duration_cast<nanoseconds>(parEnd - parBegin).count() * 1e-9;

            volatile double parResult = matrixPar[0][0];
            parResult = parResult;

            double speedup = seqTime / parTime;
            results.push_back({ size, numThreads, parTime, speedup, false });
        }
    }

    // Вивід результатів
    std::cout << "\n=== РЕЗУЛЬТАТИ ТЕСТУВАННЯ ===\n";
    std::cout << "Доступних апаратних потокiв: " << hardwareThreads << "\n\n";
    std::cout << std::setw(12) << "Розмiр" 
              << std::setw(12) << "Послiдовно" 
              << std::setw(12) << "Потокiв" 
              << std::setw(15) << "Паралельно" 
              << std::setw(12) << "Прискорення\n";
    std::cout << std::string(63, '-') << "\n";

    for (const auto& result : results) {
        if (result.isSequential) {
            std::cout << std::setw(12) << (result.size == 10000 ? "10000x10000" : std::to_string(result.size) + "x" + std::to_string(result.size))
                      << std::setw(12) << std::fixed << std::setprecision(6) << result.timeComputation << " с"
                      << std::setw(12) << "-"
                      << std::setw(15) << "-"
                      << std::setw(12) << "-\n";
        } else {
            std::cout << std::setw(12) << ""
                      << std::setw(12) << ""
                      << std::setw(12) << result.threads
                      << std::setw(15) << std::fixed << std::setprecision(6) << result.timeComputation << " с"
                      << std::setw(12) << std::fixed << std::setprecision(2) << result.speedup << "x\n";
        }
    }
}

void demonstrateOnSmallMatrix() {
    std::cout << "\n=== ДЕМОНСТРАЦiЯ НА МАЛiЙ МАТРИЦi (5x5) ===\n";

    size_t size = 5;
    std::vector<std::vector<double>> matrix(size, std::vector<double>(size));

    fillMatrixRandom(matrix);
    printMatrix(matrix, "Початкова матриця");

    sequentialVersion(matrix);
    printMatrix(matrix, "Матриця пiсля розмiщення сум рядкiв на дiагоналi");
}

int main()
{
    setlocale(LC_ALL, "");

    std::cout << "Лабораторна робота №1\n";
    std::cout << "Завдання 9: Заповнити квадратну матрицю випадковими числами.\n";
    std::cout << "На головнiй дiагоналi розмiстити суми елементiв, якi лежать в тому ж рядку.\n";
    demonstrateOnSmallMatrix();

    std::cout << "\n\nНатиснiть Enter для початку тестування продуктивностi...\n";
    std::cin.get();

    runTests();

    std::cout << "\n\nТестування завершено. Натиснiть Enter для виходу...\n";
    std::cin.get();

    return 0;
}
