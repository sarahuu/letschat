// voice/voice_client.cpp

#include <iostream>
#include <thread>
#include <cstring>
#include <portaudio.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "../common/config.h"

#define SAMPLE_RATE VOICE_SAMPLE_RATE
#define FRAMES_PER_BUFFER VOICE_FRAMES_PER_BUFFER
#define BUFFER_SIZE VOICE_BUFFER_SIZE
#define SERVER_IP "127.0.0.1"  // Change if needed

void error_exit(const std::string& msg) {
    std::cerr << msg << std::endl;
    exit(1);
}

void send_audio(PaStream* stream, int sockfd, struct sockaddr_in serverAddr) {
    int16_t buffer[FRAMES_PER_BUFFER];

    while (true) {
        PaError err = Pa_ReadStream(stream, buffer, FRAMES_PER_BUFFER);
        if (err != paNoError) {
            std::cerr << "Error reading from mic: " << Pa_GetErrorText(err) << std::endl;
            continue;
        }

        sendto(sockfd, buffer, BUFFER_SIZE, 0,
               (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    }
}

void receive_audio(PaStream* stream, int sockfd) {
    int16_t buffer[FRAMES_PER_BUFFER];
    socklen_t addrLen;
    struct sockaddr_in fromAddr;

    while (true) {
        ssize_t recvLen = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                                   (struct sockaddr*)&fromAddr, &addrLen);

        if (recvLen > 0) {
            PaError err = Pa_WriteStream(stream, buffer, FRAMES_PER_BUFFER);
            if (err != paNoError) {
                std::cerr << "Error playing audio: " << Pa_GetErrorText(err) << std::endl;
            }
        }
    }
}

int main() {
    // UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) error_exit("Failed to create socket");

    sockaddr_in serverAddr{}, clientAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(VOICE_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    clientAddr.sin_port = htons(0);  // OS assigns port
    bind(sockfd, (struct sockaddr*)&clientAddr, sizeof(clientAddr));

    // PortAudio Init
    Pa_Initialize();

    PaStreamParameters inputParams, outputParams;
    inputParams.device = Pa_GetDefaultInputDevice();
    outputParams.device = Pa_GetDefaultOutputDevice();
    if (inputParams.device == paNoDevice || outputParams.device == paNoDevice)
        error_exit("No audio device found");

    inputParams.channelCount = 1;
    inputParams.sampleFormat = paInt16;
    inputParams.suggestedLatency =
        Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = nullptr;

    outputParams.channelCount = 1;
    outputParams.sampleFormat = paInt16;
    outputParams.suggestedLatency =
        Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
    outputParams.hostApiSpecificStreamInfo = nullptr;

    PaStream* stream;
    PaError err = Pa_OpenStream(&stream, &inputParams, &outputParams,
                                SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff,
                                nullptr, nullptr);
    if (err != paNoError) error_exit("Failed to open PortAudio stream");

    Pa_StartStream(stream);

    std::thread sender(send_audio, stream, sockfd, serverAddr);
    std::thread receiver(receive_audio, stream, sockfd);

    std::cout << "Voice client running. Press Ctrl+C to exit." << std::endl;

    sender.join();
    receiver.join();

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    close(sockfd);
    return 0;
}
