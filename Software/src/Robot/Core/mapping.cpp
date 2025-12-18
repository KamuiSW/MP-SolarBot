#include <wiringPi.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <set>
#include <algorithm>
#include <fstream>

#include "config.h"
#include "mapping.h"

class Stepper {
public:
    int stepPin, dirPin, enPin;
    bool forwardLevel;

    Stepper(int step, int dir, int en, bool fwdLevel)
        : stepPin(step), dirPin(dir), enPin(en), forwardLevel(fwdLevel) {}

    void begin() {
        pinMode(stepPin, OUTPUT);
        pinMode(dirPin, OUTPUT);
        pinMode(enPin, OUTPUT);
        digitalWrite(enPin, LOW);
        digitalWrite(stepPin, LOW);
        digitalWrite(dirPin, forwardLevel ? HIGH : LOW);
    }

    void enable(bool on) {
        digitalWrite(enPin, on ? LOW : HIGH);
    }

    void setForward(bool forward) {
        bool lvl = forward ? forwardLevel : !forwardLevel;
        digitalWrite(dirPin, lvl ? HIGH : LOW);
    }

    void stepOnce() {
        digitalWrite(stepPin, HIGH);
        delayMicroseconds(STEP_PULSE_US);
        digitalWrite(stepPin, LOW);
    }
};

class DifferentialDrive {
public:
    Stepper left;
    Stepper right;

    DifferentialDrive(Stepper l, Stepper r) : left(l), right(r) {}

    void begin() {
        left.begin();
        right.begin();
        left.enable(true);
        right.enable(true);
    }

    void stop() {
        left.enable(false);
        right.enable(false);
    }

    void forwardCell() {
        left.setForward(true);
        right.setForward(true);
        for(int i=0;i<STEPS_PER_CELL;i++){
            left.stepOnce();
            right.stepOnce();
            delayMicroseconds(STEP_DELAY_US);
        }
    }

    void turnRight90() {
        left.setForward(true);
        right.setForward(false);
        for(int i=0;i<STEPS_PER_90_TURN;i++){
            left.stepOnce();
            right.stepOnce();
            delayMicroseconds(STEP_DELAY_US);
        }
    }

    void turnLeft90() {
        left.setForward(false);
        right.setForward(true);
        for(int i=0;i<STEPS_PER_90_TURN;i++){
            left.stepOnce();
            right.stepOnce();
            delayMicroseconds(STEP_DELAY_US);
        }
    }
};

enum class Direction { UP=0, RIGHT=1, DOWN=2, LEFT=3 };

static const char* dirName(Direction d) {
    switch(d){
        case Direction::UP: return "UP";
        case Direction::RIGHT: return "RIGHT";
        case Direction::DOWN: return "DOWN";
        case Direction::LEFT: return "LEFT";
    }
    return "?";
}

struct Pose {
    int x=0, y=0;
    Direction dir=Direction::UP;
};

static void stepForward(Pose& p) {
    switch(p.dir){
        case Direction::UP:    p.y += 1; break;
        case Direction::DOWN:  p.y -= 1; break;
        case Direction::LEFT:  p.x -= 1; break;
        case Direction::RIGHT: p.x += 1; break;
    }
}

static void turnRight(Pose& p) {
    p.dir = static_cast<Direction>((static_cast<int>(p.dir) + 1) % 4);
}

static void turnLeft(Pose& p) {
    p.dir = static_cast<Direction>((static_cast<int>(p.dir) + 3) % 4);
}

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
            if (elapsed.count() > timeoutSeconds) return 999.0f;
        }

        auto pulseStart = std::chrono::high_resolution_clock::now();
        while (digitalRead(echoPin) == HIGH) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = now - pulseStart;
            if (elapsed.count() > timeoutSeconds) return 999.0f;
        }

        auto pulseEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> pulseDuration = pulseEnd - pulseStart;
        float seconds = pulseDuration.count();
        return (seconds * 34300.0f) / 2.0f;
    }

    bool isCliff() {
        float d = readDistanceCm();
        if (d <= 0 || d > 900.0f) return true;
        return d > CLIFF_THRESHOLD_CM;
    }
};

struct CliffSensors {
    bool frontCliff=false;
    bool leftCliff=false;
};

static CliffSensors readCliffSensors(Ultrasonic& front, Ultrasonic& left) {
    CliffSensors cs;
    cs.frontCliff = front.isCliff();
    cs.leftCliff  = left.isCliff();
    return cs;
}

static void exportMapJson(const std::set<std::pair<int,int>>& perimeter,
                          const std::vector<std::pair<int,int>>& corners)
{
    std::ofstream f(MAP_JSON_PATH);
    if(!f.is_open()) return;

    f << "{\n";
    f << "  \"cell_size_cm\": " << CELL_SIZE_CM << ",\n";

    f << "  \"perimeter\": [\n";
    bool first = true;
    for (auto &p : perimeter) {
        if(!first) f << ",\n";
        first = false;
        f << "    {\"x\": " << p.first << ", \"y\": " << p.second << "}";
    }
    f << "\n  ],\n";

    f << "  \"corners\": [\n";
    for (size_t i=0;i<corners.size();i++){
        f << "    {\"x\": " << corners[i].first << ", \"y\": " << corners[i].second << "}";
        if(i+1<corners.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n";
    f << "}\n";
}

bool runMapping() {
    if (wiringPiSetupGpio() == -1) return false;

    Ultrasonic sonarFront(TRIG_FRONT, ECHO_FRONT);
    Ultrasonic sonarLeft(TRIG_LEFT, ECHO_LEFT);
    sonarFront.begin();
    sonarLeft.begin();

    Stepper leftMotor(L_STEP, L_DIR, L_EN, L_DIR_FORWARD_LEVEL);
    Stepper rightMotor(R_STEP, R_DIR, R_EN, R_DIR_FORWARD_LEVEL);
    DifferentialDrive drive(leftMotor, rightMotor);
    drive.begin();

    Pose pose;
    std::set<std::pair<int,int>> perimeterCells;
    std::vector<std::pair<int,int>> corners;

    enum class State { FIND_EDGE_LEFT, FIND_CORNER, TRACE_PERIMETER, DONE, FAIL };
    State state = State::FIND_EDGE_LEFT;

    const int MAX_STEPS = 2000;
    int steps = 0;
    int rightTurns = 0;
    bool originSet = false;

    while (steps++ < MAX_STEPS && state != State::DONE && state != State::FAIL) {
        CliffSensors cs = readCliffSensors(sonarFront, sonarLeft);

        std::cout << "[Map] step=" << steps
                  << " state=" << (int)state
                  << " pose=(" << pose.x << "," << pose.y << ") dir=" << dirName(pose.dir)
                  << " L=" << cs.leftCliff << " F=" << cs.frontCliff << "\n";

        if (state == State::FIND_EDGE_LEFT) {
            if (cs.leftCliff) {
                state = State::FIND_CORNER;
            } else {
                drive.turnRight90();
                turnRight(pose);
            }
        }
        else if (state == State::FIND_CORNER) {
            if (!cs.leftCliff) {
                state = State::FIND_EDGE_LEFT;
                continue;
            }

            if (cs.leftCliff && cs.frontCliff) {
                pose.x = 0;
                pose.y = 0;
                pose.dir = Direction::UP;
                originSet = true;

                corners.push_back({0,0});
                perimeterCells.insert({0,0});
                rightTurns = 0;

                drive.turnRight90();
                turnRight(pose);
                rightTurns++;

                state = State::TRACE_PERIMETER;
            } else {
                drive.forwardCell();
                stepForward(pose);
                if (originSet) perimeterCells.insert({pose.x, pose.y});
            }
        }
        else if (state == State::TRACE_PERIMETER) {
            if (!cs.leftCliff) {
                drive.turnLeft90();
                turnLeft(pose);
                continue;
            }

            if (cs.frontCliff) {
                corners.push_back({pose.x, pose.y});
                perimeterCells.insert({pose.x, pose.y});

                drive.turnRight90();
                turnRight(pose);
                rightTurns++;

                if (rightTurns >= 4 && pose.x == 0 && pose.y == 0) {
                    state = State::DONE;
                    break;
                }
            } else {
                drive.forwardCell();
                stepForward(pose);
                perimeterCells.insert({pose.x, pose.y});
            }
        }
    }

    drive.stop();

    if (state == State::FAIL) return false;

    exportMapJson(perimeterCells, corners);
    return true;
}
