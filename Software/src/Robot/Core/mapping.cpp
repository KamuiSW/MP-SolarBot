#include <wiringPi.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

// Map size in cells
const int MAP_W = 20;
const int MAP_H = 20;

// Each cell represents this many centimeters in the real world
const float CELL_SIZE_CM = 5.0f;

const float CLIFF_THRESHOLD_CM = 30.0f;

const int TRIG_FRONT = 2;
const int ECHO_FRONT = 3;

const int TRIG_BACK  = 4;
const int ECHO_BACK  = 17;

const int TRIG_LEFT  = 27;
const int ECHO_LEFT  = 22;

const int TRIG_RIGHT = 10;
const int ECHO_RIGHT = 9;

struct Cell {
    bool isEdge = false;
    bool onPath = false;
};

class PanelMap {
public:
    int w, h;
    std::vector<std::vector<Cell>> grid;

    PanelMap(int width, int height) : w(width), h(height) {
        grid.resize(h, std::vector<Cell>(w));
    }

    void markEdge(int x, int y) {
        if (x >= 0 && x < w && y >= 0 && y < h) {
            grid[y][x].isEdge = true;
        }
    }

    void markPath(int x, int y) {
        if (x >= 0 && x < w && y >= 0 && y < h) {
            grid[y][x].onPath = true;
        }
    }

    void printAscii(int robotX, int robotY) {
        //E = edge, . = path, R = robot, space = unknown
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                char c = ' ';
                const Cell& cell = grid[y][x];

                if (cell.onPath) c = '.';
                if (cell.isEdge) c = 'E';
                if (x == robotX && y == robotY) c = 'R';

                std::cout << c;
            }
            std::cout << "\n";
        }
        for (int i = 0; i < w; ++i) std::cout << "-";
        std::cout << "\n";
    }
};

enum class Direction {
    UP = 0,
    RIGHT = 1,
    DOWN = 2,
    LEFT = 3
};

class RobotLogic {
public:
    int x;
    int y;
    Direction dir;

    RobotLogic(int startX, int startY)
        : x(startX), y(startY), dir(Direction::UP) {}

    void stepForward() {
        switch (dir) {
        case Direction::UP:    y -= 1; break;
        case Direction::DOWN:  y += 1; break;
        case Direction::LEFT:  x -= 1; break;
        case Direction::RIGHT: x += 1; break;
        }
    }

    void turnLeft() {
        dir = static_cast<Direction>((static_cast<int>(dir) + 3) % 4);
    }

    void turnRight() {
        dir = static_cast<Direction>((static_cast<int>(dir) + 1) % 4);
    }

    void markCurrentAsPath(PanelMap& map) {
        map.markPath(x, y);
    }
};

class Ultrasonic {
public:
    int trigPin;
    int echoPin;

    Ultrasonic(int trig, int echo) : trigPin(trig), echoPin(echo) {}

    void begin() {
        pinMode(trigPin, OUTPUT);
        pinMode(echoPin, INPUT);
        digitalWrite(trigPin, LOW);
        delay(50);
    }

    float readDistanceCm(float timeoutSeconds = 0.03f) {
        digitalWrite(trigPin, LOW);
        delayMicroseconds(2);

        digitalWrite(trigPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(trigPin, LOW);

        auto start = std::chrono::high_resolution_clock::now();
        while (digitalRead(echoPin) == LOW) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = now - start;
            if (elapsed.count() > timeoutSeconds) {
                return 999.0f;
            }
        }
        auto pulseStart = std::chrono::high_resolution_clock::now();

        while (digitalRead(echoPin) == HIGH) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = now - pulseStart;
            if (elapsed.count() > timeoutSeconds) {
                return 999.0f;
            }
        }
        auto pulseEnd = std::chrono::high_resolution_clock::now();

        std::chrono::duration<float> pulseDuration = pulseEnd - pulseStart;
        float seconds = pulseDuration.count();

        float distanceCm = (seconds * 34300.0f) / 2.0f;
        return distanceCm;
    }

    bool isCliff() {
        float d = readDistanceCm();
        if (d <= 0 || d > 900.0f) {
            return true;
        }
        return d > CLIFF_THRESHOLD_CM;
    }
};

struct CliffSensors {
    bool frontCliff = false;
    bool backCliff  = false;
    bool leftCliff  = false;
    bool rightCliff = false;
};

CliffSensors readCliffSensors(Ultrasonic& front, Ultrasonic& back, Ultrasonic& left, Ultrasonic& right) {
    CliffSensors cs;
    cs.frontCliff = front.isCliff();
    cs.backCliff  = back.isCliff();
    cs.leftCliff  = left.isCliff();
    cs.rightCliff = right.isCliff();
    return cs;
}

void motorsInit() {

}

void moveForwardOneCell() {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

void turnRight90() {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

void turnLeft90() {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

void updateEdgeMapFromCliff(PanelMap& map, RobotLogic& robot, const CliffSensors& cs) {
    if (cs.frontCliff || cs.backCliff || cs.leftCliff || cs.rightCliff) {
        map.markEdge(robot.x, robot.y);
    }
}

int main() {
    if (wiringPiSetupGpio() == -1) {
        std::cerr << "Failed to init wiringPi (BCM mode).\n";
        return 1;
    }

    Ultrasonic sonarFront(TRIG_FRONT, ECHO_FRONT);
    Ultrasonic sonarBack(TRIG_BACK, ECHO_BACK);
    Ultrasonic sonarLeft(TRIG_LEFT, ECHO_LEFT);
    Ultrasonic sonarRight(TRIG_RIGHT, ECHO_RIGHT);

    sonarFront.begin();
    sonarBack.begin();
    sonarLeft.begin();
    sonarRight.begin();

    PanelMap map(MAP_W, MAP_H);
    RobotLogic robot(MAP_W / 2, MAP_H / 2);

    motorsInit();

    std::cout << "Robot edge-mapping demo start.\n";

    int stepCounter = 0;

    while (true) {
        CliffSensors cs = readCliffSensors(sonarFront, sonarBack, sonarLeft, sonarRight);

        updateEdgeMapFromCliff(map, robot, cs);

        robot.markCurrentAsPath(map);

        if (cs.frontCliff) {
            std::cout << "Front cliff detected! Turning right.\n";
            robot.turnRight();
            turnRight90();
        } else {
            std::cout << "Step forward.\n";
            robot.stepForward();
            moveForwardOneCell();
        }

        std::cout << "Robot at (" << robot.x << ", " << robot.y << "), dir=";
        switch (robot.dir) {
        case Direction::UP:    std::cout << "UP"; break;
        case Direction::RIGHT: std::cout << "RIGHT"; break;
        case Direction::DOWN:  std::cout << "DOWN"; break;
        case Direction::LEFT:  std::cout << "LEFT"; break;
        }
        std::cout << std::endl;

        ++stepCounter;
        if (stepCounter % 10 == 0) {
            map.printAscii(robot.x, robot.y);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
