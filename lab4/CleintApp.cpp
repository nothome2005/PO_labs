#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "protocol.h"

#pragma comment(lib, "ws2_32.lib")

bool sendAll(SOCKET s, const char* data, int len) {
    int totalSent = 0;
    while (totalSent < len) {
        int sent = send(s, data + totalSent, len - totalSent, 0);
        if (sent <= 0) return false;
        totalSent += sent;
    }
    return true;
}

bool recvAll(SOCKET s, char* data, int len) {
    int totalRead = 0;
    while (totalRead < len) {
        int read = recv(s, data + totalRead, len - totalRead, 0);
        if (read <= 0) return false;
        totalRead += read;
    }
    return true;
}

bool sendCommand(SOCKET s, Command cmd, uint32_t payloadSize = 0, const char* payload = nullptr) {
    PacketHeader header = { htonl(cmd), htonl(payloadSize) };
    if (!sendAll(s, reinterpret_cast<char*>(&header), sizeof(header))) return false;
    if (payloadSize > 0 && payload != nullptr) {
        if (!sendAll(s, payload, static_cast<int>(payloadSize))) return false;
    }
    return true;
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "connect failed: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server." << std::endl;

    uint32_t N = 4;
    std::vector<uint32_t> matrix(N * N);

    std::mt19937 gen(1337);
    std::uniform_int_distribution<uint32_t> dist(1, 9);
    for (auto& val : matrix) val = dist(gen);

    std::cout << "Source matrix:" << std::endl;
    for (uint32_t i = 0; i < N; ++i) {
        for (uint32_t j = 0; j < N; ++j) std::cout << matrix[i * N + j] << " ";
        std::cout << std::endl;
    }

    uint32_t n_net = htonl(N);
    if (!sendCommand(clientSocket, CMD_CONFIG, sizeof(n_net), reinterpret_cast<char*>(&n_net))) {
        std::cerr << "send CMD_CONFIG failed." << std::endl;
        closesocket(clientSocket); WSACleanup(); return 1;
    }
    PacketHeader resp;
    if (!recvAll(clientSocket, reinterpret_cast<char*>(&resp), sizeof(resp))) {
        std::cerr << "recv CMD_CONFIG response failed." << std::endl;
        closesocket(clientSocket); WSACleanup(); return 1;
    }

    std::vector<uint32_t> netMatrix(matrix.size());
    for (size_t i = 0; i < matrix.size(); ++i) netMatrix[i] = htonl(matrix[i]);
        if (!sendCommand(clientSocket, CMD_DATA, static_cast<uint32_t>(netMatrix.size() * sizeof(uint32_t)), reinterpret_cast<char*>(netMatrix.data()))) {
            std::cerr << "send CMD_DATA failed." << std::endl;
            closesocket(clientSocket); WSACleanup(); return 1;
        }
        if (!recvAll(clientSocket, reinterpret_cast<char*>(&resp), sizeof(resp))) {
            std::cerr << "recv CMD_DATA response failed." << std::endl;
            closesocket(clientSocket); WSACleanup(); return 1;
        }

        if (!sendCommand(clientSocket, CMD_START)) {
            std::cerr << "send CMD_START failed." << std::endl;
            closesocket(clientSocket); WSACleanup(); return 1;
        }
        if (!recvAll(clientSocket, reinterpret_cast<char*>(&resp), sizeof(resp))) {
            std::cerr << "recv CMD_START response failed." << std::endl;
            closesocket(clientSocket); WSACleanup(); return 1;
    }

    uint32_t status = STATUS_IN_PROGRESS;
    while (status != STATUS_DONE) {
        if (!sendCommand(clientSocket, CMD_STATUS)) {
            std::cerr << "send CMD_STATUS failed." << std::endl;
            closesocket(clientSocket); WSACleanup(); return 1;
        }
        if (!recvAll(clientSocket, reinterpret_cast<char*>(&resp), sizeof(resp))) {
            std::cerr << "recv CMD_STATUS header failed." << std::endl;
            closesocket(clientSocket); WSACleanup(); return 1;
        }

        uint32_t payloadSize = ntohl(resp.payloadSize);
        if (payloadSize != sizeof(uint32_t)) {
            std::cerr << "invalid CMD_STATUS payload size: " << payloadSize << std::endl;
            closesocket(clientSocket); WSACleanup(); return 1;
        }

        uint32_t statusNet;
        if (!recvAll(clientSocket, reinterpret_cast<char*>(&statusNet), sizeof(statusNet))) {
            std::cerr << "recv CMD_STATUS payload failed." << std::endl;
            closesocket(clientSocket); WSACleanup(); return 1;
        }
        status = ntohl(statusNet);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (!sendCommand(clientSocket, CMD_RESULT)) {
        std::cerr << "send CMD_RESULT failed." << std::endl;
        closesocket(clientSocket); WSACleanup(); return 1;
    }
    if (!recvAll(clientSocket, reinterpret_cast<char*>(&resp), sizeof(resp))) {
        std::cerr << "recv CMD_RESULT header failed." << std::endl;
        closesocket(clientSocket); WSACleanup(); return 1;
    }

    const uint32_t resultSize = ntohl(resp.payloadSize);
    if (resultSize != netMatrix.size() * sizeof(uint32_t)) {
        std::cerr << "invalid CMD_RESULT payload size: " << resultSize << std::endl;
        closesocket(clientSocket); WSACleanup(); return 1;
    }

    if (!recvAll(clientSocket, reinterpret_cast<char*>(netMatrix.data()), static_cast<int>(resultSize))) {
        std::cerr << "recv CMD_RESULT payload failed." << std::endl;
        closesocket(clientSocket); WSACleanup(); return 1;
    }
    for (size_t i = 0; i < matrix.size(); ++i) matrix[i] = ntohl(netMatrix[i]);

    std::cout << "\nProcessed matrix:" << std::endl;
    for (uint32_t i = 0; i < N; ++i) {
        for (uint32_t j = 0; j < N; ++j) std::cout << matrix[i * N + j] << " ";
        std::cout << std::endl;
    }

    closesocket(clientSocket);
    WSACleanup();

    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    return 0;
}