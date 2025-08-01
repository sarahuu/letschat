// voice_server.cpp
#include <iostream>
#include <thread>
#include <unordered_map>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 9090
#define BUFFER_SIZE 1024

int main() {
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in server_addr{}, client_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    std::cout << "[Voice Server] Listening on port " << PORT << "...\n";

    sockaddr_in clientA{}, clientB{};
    bool hasClientA = false, hasClientB = false;

    char buffer[BUFFER_SIZE];
    while (true) {
        socklen_t addr_len = sizeof(sockaddr_in);
        ssize_t bytes = recvfrom(server_fd, buffer, BUFFER_SIZE, 0, (sockaddr*)&client_addr, &addr_len);

        if (bytes < 0) continue;

        // Register clients
        if (!hasClientA) {
            clientA = client_addr;
            hasClientA = true;
            std::cout << "[Server] Client A registered.\n";
            continue;
        } else if (!hasClientB && client_addr.sin_port != clientA.sin_port) {
            clientB = client_addr;
            hasClientB = true;
            std::cout << "[Server] Client B registered.\n";
            continue;
        }

        // Relay between A and B
        sockaddr_in target = (client_addr.sin_port == clientA.sin_port) ? clientB : clientA;
        sendto(server_fd, buffer, bytes, 0, (sockaddr*)&target, sizeof(target));
    }

    close(server_fd);
    return 0;
}
