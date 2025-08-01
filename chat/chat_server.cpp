// chat_server.cpp
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <algorithm>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

std::vector<int> clients;
std::mutex clients_mutex;

void broadcast_message(const std::string& message, int sender_fd) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (int client_fd : clients) {
        if (client_fd != sender_fd) {
            send(client_fd, message.c_str(), message.size(), 0);
        }
    }
}

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    while (true) {
        ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            std::cout << "Client disconnected.\n";
            close(client_fd);
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.erase(std::remove(clients.begin(), clients.end(), client_fd), clients.end());
            break;
        }

        buffer[bytes_received] = '\0';
        std::string message(buffer);

        // Handle file transfer
        if (message.rfind("FILE:", 0) == 0) {
            std::string filename;
            size_t filesize = 0;

            size_t pos1 = message.find(':', 5);
            size_t pos2 = message.find('\n', pos1 + 1);
            filename = message.substr(5, pos1 - 5);
            filesize = std::stoul(message.substr(pos1 + 1, pos2 - pos1 - 1));

            std::cout << "Receiving file: " << filename << " (" << filesize << " bytes)\n";

            std::vector<char> file_buffer(filesize);
            size_t total_received = 0;
            while (total_received < filesize) {
                ssize_t bytes = recv(client_fd, &file_buffer[total_received], filesize - total_received, 0);
                if (bytes <= 0) break;
                total_received += bytes;
            }

            // Forward to other clients
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (int client : clients) {
                if (client != client_fd) {
                    send(client, message.c_str(), message.size(), 0); // header
                    send(client, file_buffer.data(), filesize, 0);     // content
                }
            }
            continue;
        }

        std::cout << "Received: " << message;
        broadcast_message(message, client_fd);
    }
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in server_addr{}, client_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        return 1;
    }

    if (listen(server_fd, MAX_CLIENTS) == -1) {
        perror("Listen failed");
        return 1;
    }

    std::cout << "Chat server started on port " << PORT << "...\n";

    while (true) {
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd == -1) {
            perror("Accept failed");
            continue;
        }

        std::lock_guard<std::mutex> lock(clients_mutex);
        if (clients.size() >= MAX_CLIENTS) {
            std::cerr << "Max clients reached. Connection rejected.\n";
            close(client_fd);
        } else {
            clients.push_back(client_fd);
            std::thread(handle_client, client_fd).detach();
            std::cout << "New client connected.\n";
        }
    }

    close(server_fd);
    return 0;
}
