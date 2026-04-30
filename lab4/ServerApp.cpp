#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "protocol.h"

#pragma comment(lib, "ws2_32.lib")

std::mutex g_consoleMutex;

void printMatrix(const std::vector<uint32_t>& matrix, uint32_t n, const char* title, SOCKET clientSocket) {
    std::lock_guard<std::mutex> lock(g_consoleMutex);
    std::cout << "\n[Client " << clientSocket << "] " << title << std::endl;
    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = 0; j < n; ++j) {
            std::cout << matrix[i * n + j] << ' ';
        }
        std::cout << std::endl;
    }
}

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

struct ClientSession {
    SOCKET socket;
    std::vector<uint32_t> matrix;
    uint32_t matrixSize = 0;
    std::atomic<uint32_t> status{ STATUS_IDLE };
    std::thread taskThread;

    ~ClientSession() {
        if (taskThread.joinable()) taskThread.join();
        closesocket(socket);
    }
};


void processTask(ClientSession* session) {
    session->status = STATUS_IN_PROGRESS;
    uint32_t n = session->matrixSize;


    for (uint32_t i = 0; i < n; ++i) {
        uint32_t rowSum = 0;
        for (uint32_t j = 0; j < n; ++j) {
            rowSum += session->matrix[i * n + j];
        }
        session->matrix[i * n + i] = rowSum;
    }

    printMatrix(session->matrix, session->matrixSize, "Processed matrix", session->socket);
    session->status = STATUS_DONE;
}


void handleClient(SOCKET clientSocket) {
    ClientSession session;
    session.socket = clientSocket;

    try {
        while (true) {
            PacketHeader header;
            if (!recvAll(clientSocket, reinterpret_cast<char*>(&header), sizeof(header))) break;

            header.command = ntohl(header.command);
            header.payloadSize = ntohl(header.payloadSize);

            PacketHeader responseHeader = { 0, 0 };
            uint32_t responseData = 0;

            switch (header.command) {
            case CMD_CONFIG: {
                uint32_t n_network;
                if (!recvAll(clientSocket, reinterpret_cast<char*>(&n_network), sizeof(n_network))) break;
                session.matrixSize = ntohl(n_network);
                session.matrix.resize(session.matrixSize * session.matrixSize);

                responseHeader.command = htonl(CMD_CONFIG);
                if (!sendAll(clientSocket, reinterpret_cast<char*>(&responseHeader), sizeof(responseHeader))) break;
                break;
            }
            case CMD_DATA: {
                if (header.payloadSize != session.matrix.size() * sizeof(uint32_t)) break;

                std::vector<uint32_t> buffer(session.matrix.size());
                if (!recvAll(clientSocket, reinterpret_cast<char*>(buffer.data()), static_cast<int>(header.payloadSize))) break;

                for (size_t i = 0; i < buffer.size(); ++i) {
                    session.matrix[i] = ntohl(buffer[i]);
                }

                printMatrix(session.matrix, session.matrixSize, "Received matrix", session.socket);

                responseHeader.command = htonl(CMD_DATA);
                if (!sendAll(clientSocket, reinterpret_cast<char*>(&responseHeader), sizeof(responseHeader))) break;
                break;
            }
            case CMD_START: {
                if (session.taskThread.joinable()) session.taskThread.join();
                session.taskThread = std::thread(processTask, &session);

                responseHeader.command = htonl(CMD_START);
                if (!sendAll(clientSocket, reinterpret_cast<char*>(&responseHeader), sizeof(responseHeader))) break;
                break;
            }
            case CMD_STATUS: {
                responseHeader.command = htonl(CMD_STATUS);
                responseHeader.payloadSize = htonl(sizeof(uint32_t));
                responseData = htonl(session.status.load());

                if (!sendAll(clientSocket, reinterpret_cast<char*>(&responseHeader), sizeof(responseHeader))) break;
                if (!sendAll(clientSocket, reinterpret_cast<char*>(&responseData), sizeof(responseData))) break;
                break;
            }
            case CMD_RESULT: {
                if (session.status != STATUS_DONE) {
                    responseHeader.command = htonl(CMD_RESULT);
                    responseHeader.payloadSize = htonl(0);
                    sendAll(clientSocket, reinterpret_cast<char*>(&responseHeader), sizeof(responseHeader));
                    break;
                }

                std::this_thread::sleep_for(std::chrono::seconds(5));

                const uint32_t resultBytes = static_cast<uint32_t>(session.matrix.size() * sizeof(uint32_t));
                responseHeader.command = htonl(CMD_RESULT);
                responseHeader.payloadSize = htonl(resultBytes);
                if (!sendAll(clientSocket, reinterpret_cast<char*>(&responseHeader), sizeof(responseHeader))) break;

                std::vector<uint32_t> outBuffer(session.matrix.size());
                for (size_t i = 0; i < session.matrix.size(); ++i) {
                    outBuffer[i] = htonl(session.matrix[i]);
                }
                if (!sendAll(clientSocket, reinterpret_cast<char*>(outBuffer.data()), static_cast<int>(resultBytes))) break;
                break;
            }
            }
        }
    }
    catch (...) {
        std::cerr << "Client processing error." << std::endl;
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server started. Waiting for connections on port 8080..." << std::endl;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        std::cout << "Client connected." << std::endl;
        std::thread(handleClient, clientSocket).detach();
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}