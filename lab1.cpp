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

int main() {
    setlocale(LC_ALL, "");
    size_t size = 5;
    std::vector<std::vector<double>> matrix(size, std::vector<double>(size));
    fillMatrixRandom(matrix);
    printMatrix(matrix, "Початкова матриця");
    sequentialVersion(matrix);
    printMatrix(matrix, "Після обробки (послідовно)");
    return 0;
}
