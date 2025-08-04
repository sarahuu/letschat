// video_server.cpp
#include <iostream>
#include <thread>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 7070

void relay(int sock1, int sock2) {
    char buffer[65536];
    while (true) {
        uint32_t size;
        ssize_t s = recv(sock1, &size, sizeof(size), 0);
        if (s <= 0) break;
        send(sock2, &size, sizeof(size), 0);

        size = ntohl(size);
        size_t received = 0;
        while (received < size) {
            ssize_t b = recv(sock1, buffer, std::min((int)size - (int)received, 65536), 0);
            if (b <= 0) break;
            send(sock2, buffer, b, 0);
            received += b;
        }
    }
    close(sock1);
    close(sock2);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 2);

    std::cout << "Waiting for 2 video clients...\n";
    int sock1 = accept(server_fd, nullptr, nullptr);
    std::cout << "Client A connected.\n";
    int sock2 = accept(server_fd, nullptr, nullptr);
    std::cout << "Client B connected.\n";

    std::thread t1(relay, sock1, sock2);
    std::thread t2(relay, sock2, sock1);

    t1.join();
    t2.join();

    close(server_fd);
    return 0;
}
