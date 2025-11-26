#include <SDL2/SDL.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>

const char* PI_IP = "192.168.1.50";
const int PI_PORT = 5005;

int main() {
    SDL_Init(SDL_INIT_JOYSTICK);

    SDL_Joystick* js = SDL_JoystickOpen(0);
    if (!js) {
        std::cout << "joystickless\n";
        return 1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PI_PORT);
    inet_pton(AF_INET, PI_IP, &addr.sin_addr);

    while (true) {
        SDL_JoystickUpdate();

        float y = SDL_JoystickGetAxis(js, 1) / 32767.0f;
        float x = SDL_JoystickGetAxis(js, 0) / 32767.0f;

        if (std::abs(y) < 0.1) y = 0;
        if (std::abs(x) < 0.1) x = 0;

        char msg[64];
        snprintf(msg, sizeof(msg), "F %.2f T %.2f", -y, x);

        sendto(sock, msg, strlen(msg), 0, 
               (sockaddr*)&addr, sizeof(addr));

        SDL_Delay(50);
    }

    close(sock);
    return 0;
}
