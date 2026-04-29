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
