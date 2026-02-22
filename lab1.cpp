#include <iostream>
#include <vector>
#include <random>
#include <iomanip>

void printMatrix(const std::vector<std::vector<double>>& matrix, const std::string& title) {
    std::cout << "\n" << title << ":\n";
    if (matrix.size() > 10) return;
    for (const auto& row : matrix) {
        for (const auto& elem : row) std::cout << std::setw(10) << std::fixed << std::setprecision(2) << elem << " ";
        std::cout << "\n";
    }
}

void fillMatrixRandom(std::vector<std::vector<double>>& matrix, int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> dist(1.0, 100.0);
    for (size_t i = 0; i < matrix.size(); i++) {
        for (size_t j = 0; j < matrix[i].size(); j++) matrix[i][j] = dist(gen);
    }
}

void sequentialVersion(std::vector<std::vector<double>>& matrix) {
    size_t n = matrix.size();
    std::vector<double> columnMaxs(n);
    for (size_t col = 0; col < n; col++) {
        double maxVal = matrix[0][col];
        for (size_t row = 1; row < n; row++) {
            if (matrix[row][col] > maxVal) maxVal = matrix[row][col];
        }
        columnMaxs[col] = maxVal;
    }
    for (size_t i = 0; i < n; i++) matrix[i][i] = columnMaxs[i];
}

void printMatrix(const std::vector<std::vector<double>>& matrix, const std::string& title) { /*...*/ }
void fillMatrixRandom(std::vector<std::vector<double>>& matrix, int seed) { /*...*/ }

void processColumns(const std::vector<std::vector<double>>& matrix, std::vector<double>& columnMaxs, size_t startCol, size_t endCol) {
    size_t n = matrix.size();
    for (size_t col = startCol; col < endCol; col++) {
        double maxVal = matrix[0][col];
        for (size_t row = 1; row < n; row++) {
            if (matrix[row][col] > maxVal) maxVal = matrix[row][col];
        }
        columnMaxs[col] = maxVal;
    }
}

void parallelVersion(std::vector<std::vector<double>>& matrix, size_t numThreads) {
    size_t n = matrix.size();
    std::vector<double> columnMaxs(n);
    std::vector<std::thread> threads;
    size_t colsPerThread = n / numThreads;
    size_t remainingCols = n % numThreads;
    size_t startCol = 0;

    for (size_t i = 0; i < numThreads; i++) {
        size_t endCol = startCol + colsPerThread + (i < remainingCols ? 1 : 0);
        threads.emplace_back(processColumns, std::ref(matrix), std::ref(columnMaxs), startCol, endCol);
        startCol = endCol;
    }
    for (auto& t : threads) t.join();
    for (size_t i = 0; i < n; i++) matrix[i][i] = columnMaxs[i];
}

int main() {
    setlocale(LC_ALL, "");
    size_t size = 5;
    std::vector<std::vector<double>> matrix(size, std::vector<double>(size));
    fillMatrixRandom(matrix);
    parallelVersion(matrix, 2);
    printMatrix(matrix, "Паралельна версія (2 потоки)");
    return 0;
}
