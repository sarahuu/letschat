// video_client.cpp
#include <iostream>
#include <opencv2/opencv.hpp>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT 7070
#define BUFFER_SIZE 65536

void error_exit(const std::string& msg) {
    std::cerr << msg << std::endl;
    exit(1);
}

void run_sender(int sock) {
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        error_exit("Could not open webcam");
    }

    cv::Mat frame;
    std::vector<uchar> encoded;
    std::vector<int> encode_param = { cv::IMWRITE_JPEG_QUALITY, 80 };

    while (true) {
        cap >> frame;
        if (frame.empty()) break;

        cv::imencode(".jpg", frame, encoded, encode_param);
        uint32_t size = htonl(encoded.size());
        send(sock, &size, sizeof(size), 0);
        send(sock, encoded.data(), encoded.size(), 0);

        cv::imshow("You", frame);
        if (cv::waitKey(1) == 27) break; // ESC key
    }
}

void run_receiver(int sock) {
    char buffer[BUFFER_SIZE];
    while (true) {
        uint32_t size;
        ssize_t bytes = recv(sock, &size, sizeof(size), 0);
        if (bytes <= 0) break;

        size = ntohl(size);
        std::vector<uchar> data(size);
        size_t received = 0;
        while (received < size) {
            ssize_t chunk = recv(sock, data.data() + received, size - received, 0);
            if (chunk <= 0) break;
            received += chunk;
        }

        cv::Mat img = cv::imdecode(data, cv::IMREAD_COLOR);
        if (!img.empty()) {
            cv::imshow("Partner", img);
            if (cv::waitKey(1) == 27) break; // ESC
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2 || (std::string(argv[1]) != "send" && std::string(argv[1]) != "recv")) {
        std::cout << "Usage: " << argv[0] << " [send|recv]\n";
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) error_exit("Socket creation failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        error_exit("Connection to server failed");
    }

    if (std::string(argv[1]) == "send") {
        run_sender(sock);
    }
    else {
        run_receiver(sock);
    }

    close(sock);
    return 0;
}
