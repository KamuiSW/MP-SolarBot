#pragma once

#ifdef MOCK_HARDWARE

#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <map>
#include <random>
#include <thread>
#include <vector>

// Mock wiringPi constants
#define INPUT 0
#define OUTPUT 1
#define LOW 0
#define HIGH 1

// Pins from config.h
// We redefine them here to avoid circular dep issues, but they match config.h
constexpr int MOCK_TRIG_FRONT = 2;
constexpr int MOCK_ECHO_FRONT = 3;
constexpr int MOCK_TRIG_LEFT = 27;
constexpr int MOCK_ECHO_LEFT = 22;

constexpr int MOCK_L_STEP = 21;
constexpr int MOCK_L_DIR = 20;
constexpr int MOCK_R_STEP = 13;
constexpr int MOCK_R_DIR = 19;

// Robot Specs
constexpr double M_ROBOT_WIDTH = 28.0;

class MockEnvironment {
public:
  static MockEnvironment &instance() {
    static MockEnvironment inst;
    return inst;
  }

  int panelWidthCm = 200;
  int panelHeightCm = 150;
  int robotSizeCm = 30;
  double stepsPerCm = 56.4;

  std::atomic<double> posX{16.0};
  std::atomic<double> posY{16.0};
  std::atomic<double> angle{0.0};

  // Pin State
  std::map<int, int> pinStates;
  bool lDir = false;
  bool rDir = false;

  struct SimStain {
    double x, y;
    double radius = 15.0; // cm
  };
  std::vector<SimStain> trueStains;

  void setStains(const std::vector<SimStain> &stains) {
    trueStains = stains;
    std::cout << "[Mock] Updated " << trueStains.size() << " sim stains.\n";
  }

  double checkForStain() {
    for (const auto &s : trueStains) {
      double dx = posX - s.x;
      double dy = posY - s.y;
      double dist = std::sqrt(dx * dx + dy * dy);
      if (dist < (s.radius + 10.0)) {
        return 85.0 + (15.0 * (1.0 - dist / (s.radius + 10.0)));
      }
    }
    return 0.0;
  }

  void setConfig(int w, int h, int size) {
    panelWidthCm = w;
    panelHeightCm = h;
    robotSizeCm = size;
    reset();
    std::cout << "[Mock] Config: " << w << "x" << h << " Robot:" << size
              << "\n";
  }

  void reset() {
    double start = robotSizeCm / 2.0 + 2.0;
    posX.store(start);
    posY.store(start);
    angle.store(0.0);
    std::cout << "[Mock] Reset Pose to (" << start << "," << start << ")\n";
  }

  void handleWrite(int pin, int value) {
    pinStates[pin] = value;

    if (pin == MOCK_L_DIR)
      lDir = (value == HIGH);
    if (pin == MOCK_R_DIR)
      rDir = (value == HIGH);

    if (value == HIGH && (pin == MOCK_L_STEP || pin == MOCK_R_STEP)) {
      step(pin);
    }
  }

  void step(int pin) {
    double dLeft = 0;
    double dRight = 0;
    const double distPerStep = 1.0 / stepsPerCm;

    double lSign = lDir ? 1.0 : -1.0;
    double rSign = rDir ? 1.0 : -1.0;

    if (pin == MOCK_L_STEP)
      dLeft = lSign * distPerStep;
    if (pin == MOCK_R_STEP)
      dRight = rSign * distPerStep;

    double dS = (dLeft + dRight) / 2.0;
    double dTheta = (dLeft - dRight) / M_ROBOT_WIDTH;

    double curAngle = angle.load();
    double curX = posX.load();
    double curY = posY.load();

    curAngle += dTheta;
    curX += dS * sin(curAngle);
    curY += dS * cos(curAngle);

    angle.store(curAngle);
    posX.store(curX);
    posY.store(curY);
  }

  bool checkPointCliff(double wx, double wy) {
    if (wx < 0 || wx > panelWidthCm || wy < 0 || wy > panelHeightCm) {
      return true;
    }
    return false;
  }

  bool isCliff(int echoPin) {
    double sensX = 0, sensY = 0;

    if (echoPin == MOCK_ECHO_FRONT) {
      sensX = 0;
      sensY = robotSizeCm / 2.0 + 5.0;
    } else if (echoPin == MOCK_ECHO_LEFT) {
      sensX = -(robotSizeCm / 2.0) - 5.0;
      sensY = 0;
    } else {
      return false;
    }

    double rwX = sensX * cos(angle) + sensY * sin(angle);
    double rwY = -sensX * sin(angle) + sensY * cos(angle);

    double checkX = posX + rwX;
    double checkY = posY + rwY;

    return checkPointCliff(checkX, checkY);
  }
};

inline int wiringPiSetupGpio() {
  std::cout << "[Mock] wiringPiSetupGpio success\n";
  return 0;
}

inline void pinMode(int pin, int mode) {}

inline void digitalWrite(int pin, int value) {
  MockEnvironment::instance().handleWrite(pin, value);
}

inline int digitalRead(int pin) { return LOW; }

inline void delay(unsigned int howLong) {
  if (howLong > 10)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  else
    std::this_thread::sleep_for(std::chrono::milliseconds(howLong));
}

inline void delayMicroseconds(unsigned int howLong) {}

inline std::mt19937 &getRNG() {
  static std::mt19937 rng(12345);
  return rng;
}

#endif // MOCK_HARDWARE
