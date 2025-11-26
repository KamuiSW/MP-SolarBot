#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cmath>
#include <iostream>

#include "movement.h"

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5005);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (sockaddr*)&addr, sizeof(addr));

    char buf[256];

    while (true) {
        recv(sock, buf, sizeof(buf), 0);

        float f = 0, t = 0;
        sscanf(buf, "F %f T %f", &f, &t);

        // control scaling
        if (std::abs(f) > 0.01f)
            moveStraight(f * 5);

        if (std::abs(t) > 0.01f)
            tankTurn(t * 3);
    }

    return 0;
}
