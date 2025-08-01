#include <iostream>
#include <thread>
#include <string>
#include <cstring>      // <-- Fix for strncmp
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fstream>
#include <algorithm>    // <-- Fix for std::min

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 1024

void receive_messages(int sock) {
    char buffer[BUFFER_SIZE];
    std::string partial;

    while (true) {
        ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            std::cout << "\nDisconnected from server.\n";
            close(sock);
            exit(0);
        }

        buffer[bytes_received] = '\0';
        partial += buffer;

        // Process lines (handle multiple messages in one buffer)
        size_t newline_pos;
        while ((newline_pos = partial.find('\n')) != std::string::npos) {
            std::string line = partial.substr(0, newline_pos);
            partial.erase(0, newline_pos + 1);

            if (line.rfind("FILE:", 0) == 0) {
                // Parse file header
                std::string filename;
                size_t filesize;
                size_t colon1 = line.find(':');
                size_t colon2 = line.find(':', colon1 + 1);

                filename = line.substr(colon1 + 1, colon2 - colon1 - 1);
                filesize = std::stoul(line.substr(colon2 + 1));

                std::cout << "\nReceiving file: " << filename << " (" << filesize << " bytes)\n";

                std::ofstream outfile(filename, std::ios::binary);
                size_t received = 0;

                // If partial already contains file data (after header)
                if (!partial.empty()) {
                    size_t chunk_size = std::min(filesize, partial.size());
                    outfile.write(partial.data(), chunk_size);
                    received += chunk_size;
                    partial.erase(0, chunk_size);
                }

                // Read remaining bytes
                while (received < filesize) {
                    ssize_t bytes = recv(sock, buffer, std::min(static_cast<size_t>(BUFFER_SIZE), filesize - received), 0);
                    if (bytes <= 0) {
                        std::cerr << "\nConnection lost during file transfer\n";
                        break;
                    }
                    outfile.write(buffer, bytes);
                    received += bytes;
                }

                outfile.close();
                std::cout << "File saved as: " << filename << "\n> " << std::flush;
            } else {
                // Regular chat message
                std::cout << "\r" << line << "\n> " << std::flush;
            }
        }
    }
}


int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr);

    if (connect(sock, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        return 1;
    }

    std::cout << "Connected to server. Type your message (or /file <path> to send a file):\n";

    std::thread receiver(receive_messages, sock);

    std::string input;
    while (true) {
        std::getline(std::cin, input);

        if (input.rfind("/file ", 0) == 0) {
            std::string path = input.substr(6);
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file) {
                std::cerr << "Cannot open file.\n";
                continue;
            }

            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::string filename = path.substr(path.find_last_of("/\\") + 1);
            std::string header = "[FILE]NAME:" + filename + "SIZE:" + std::to_string(size);
            send(sock, header.c_str(), header.size(), 0);

            char buffer[BUFFER_SIZE];
            while (!file.eof()) {
                file.read(buffer, BUFFER_SIZE);
                send(sock, buffer, file.gcount(), 0);
            }

            file.close();
            std::cout << "File sent: " << filename << "\n";
        } else {
            send(sock, input.c_str(), input.size(), 0);
        }
    }

    receiver.join();
    close(sock);
    return 0;
}
