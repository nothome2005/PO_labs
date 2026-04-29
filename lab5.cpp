#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define BUFFER_SIZE 4096

//fro html reading
std::string readFile(const std::string& fileName) {
    std::ifstream file(fileName, std::ios::in | std::ios::binary);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}


void handleRequest(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE] = { 0 };
    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);

    if (bytesReceived > 0) {
        std::string request(buffer);
        std::cout << "--- get requests ---\n" << request.substr(0, request.find("\r\n")) << "\n";

        
        std::istringstream iss(request);
        std::string method, path;
        iss >> method >> path;

        if (method == "GET") {
            // Root-запит веде на index.html
            std::string fileName = (path == "/" || path == "/index.html") ? "index.html" : path.substr(1);
            std::string content = readFile(fileName);

            std::string response;
            if (!content.empty()) {
                // 200 OK
                response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: text/html\r\n";
                response += "Content-Length: " + std::to_string(content.size()) + "\r\n";
                response += "Connection: close\r\n\r\n";
                response += content;
            }
            else {
                //404
                std::string errorPage = "<html><body><h1>404 Not Found</h1></body></html>";
                response = "HTTP/1.1 404 Not Found\r\n";
                response += "Content-Type: text/html\r\n";
                response += "Content-Length: " + std::to_string(errorPage.size()) + "\r\n\r\n";
                response += errorPage;
            }
            send(clientSocket, response.c_str(), (int)response.size(), 0);
        }
    }
    closesocket(clientSocket);
}

int main() {
   
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Serverr started on port: " << PORT << "...\n";

    
    while (true) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);

        if (clientSocket != INVALID_SOCKET) {
            std::thread(handleRequest, clientSocket).detach();
        }
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
