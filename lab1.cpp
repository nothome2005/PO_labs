#include <iostream>
#include <vector>
#include <random>
#include <iomanip>

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

int main() {
    setlocale(LC_ALL, "");
    size_t size = 5;
    std::vector<std::vector<double>> matrix(size, std::vector<double>(size));
    fillMatrixRandom(matrix);
    printMatrix(matrix, "Початкова матриця");
    return 0;
}
