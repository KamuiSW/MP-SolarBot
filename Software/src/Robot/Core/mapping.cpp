#ifndef MOCK_HARDWARE
#include <wiringPi.h>
#endif

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <thread>
#include <vector>

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

  void enable(bool on) { digitalWrite(enPin, on ? LOW : HIGH); }

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
    for (int i = 0; i < STEPS_PER_CELL; i++) {
      left.stepOnce();
      right.stepOnce();
      delayMicroseconds(STEP_DELAY_US);
    }
  }

  void turnRight90() {
    left.setForward(true);
    right.setForward(false);
    for (int i = 0; i < STEPS_PER_90_TURN; i++) {
      left.stepOnce();
      right.stepOnce();
      delayMicroseconds(STEP_DELAY_US);
    }
  }

  void turnLeft90() {
    left.setForward(false);
    right.setForward(true);
    for (int i = 0; i < STEPS_PER_90_TURN; i++) {
      left.stepOnce();
      right.stepOnce();
      delayMicroseconds(STEP_DELAY_US);
    }
  }
};

enum class Direction { UP = 0, RIGHT = 1, DOWN = 2, LEFT = 3 };

static const char *dirName(Direction d) {
  switch (d) {
  case Direction::UP:
    return "UP";
  case Direction::RIGHT:
    return "RIGHT";
  case Direction::DOWN:
    return "DOWN";
  case Direction::LEFT:
    return "LEFT";
  }
  return "?";
}

struct Pose {
  int x = 0, y = 0;
  Direction dir = Direction::UP;
};

static void stepForward(Pose &p) {
  switch (p.dir) {
  case Direction::UP:
    p.y += 1;
    break;
  case Direction::DOWN:
    p.y -= 1;
    break;
  case Direction::LEFT:
    p.x -= 1;
    break;
  case Direction::RIGHT:
    p.x += 1;
    break;
  }
}

static void turnRight(Pose &p) {
  p.dir = static_cast<Direction>((static_cast<int>(p.dir) + 1) % 4);
}

static void turnLeft(Pose &p) {
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
      if (elapsed.count() > timeoutSeconds)
        return 999.0f;
    }

    auto pulseStart = std::chrono::high_resolution_clock::now();
    while (digitalRead(echoPin) == HIGH) {
      auto now = std::chrono::high_resolution_clock::now();
      std::chrono::duration<float> elapsed = now - pulseStart;
      if (elapsed.count() > timeoutSeconds)
        return 999.0f;
    }

    auto pulseEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> pulseDuration = pulseEnd - pulseStart;
    float seconds = pulseDuration.count();
    return (seconds * 34300.0f) / 2.0f;
  }

  bool isCliffRaw() {
    float d = readDistanceCm();
    if (d <= 0 || d > 900.0f)
      return true;
    return d > CLIFF_THRESHOLD_CM;
  }
};

struct CliffSensors {
  bool frontCliff = false;
  bool leftCliff = false;
};

// Majority vote filter: reduces one-bad-reading perimeter leaks
static bool filteredIsCliff(Ultrasonic &s, int samples = 5,
                            int votesRequired = 3) {
  int votes = 0;
  for (int i = 0; i < samples; i++) {
    if (s.isCliffRaw())
      votes++;
    delay(8);
  }
  return votes >= votesRequired;
}

static CliffSensors readCliffSensorsFiltered(Ultrasonic &front,
                                             Ultrasonic &left) {
  CliffSensors cs;
  cs.frontCliff = filteredIsCliff(front, 5, 3);
  cs.leftCliff = filteredIsCliff(left, 5, 3);
  return cs;
}

static inline long long keyll(int x, int y) {
  return ((long long)x << 32) ^ (unsigned int)y;
}

static void addManhattanLine(std::vector<std::pair<int, int>> &out, int x0,
                             int y0, int x1, int y1) {
  int x = x0, y = y0;
  out.push_back({x, y});
  while (x != x1) {
    x += (x1 > x) ? 1 : -1;
    out.push_back({x, y});
  }
  while (y != y1) {
    y += (y1 > y) ? 1 : -1;
    out.push_back({x, y});
  }
}

static void
exportMapJsonV2(const std::vector<std::pair<int, int>> &perimeterTrace,
                const std::set<std::pair<int, int>> &perimeterSet,
                const std::vector<std::pair<int, int>> &corners) {
  std::ofstream f(MAP_JSON_PATH);
  if (!f.is_open())
    return;

  // Densify trace to guarantee watertight boundary
  std::vector<std::pair<int, int>> denseTrace;
  denseTrace.reserve(perimeterTrace.size() * 2);

  if (perimeterTrace.size() >= 2) {
    for (size_t i = 0; i + 1 < perimeterTrace.size(); i++) {
      addManhattanLine(denseTrace, perimeterTrace[i].first,
                       perimeterTrace[i].second, perimeterTrace[i + 1].first,
                       perimeterTrace[i + 1].second);
    }
    addManhattanLine(denseTrace, perimeterTrace.back().first,
                     perimeterTrace.back().second, perimeterTrace.front().first,
                     perimeterTrace.front().second);
  }

  std::set<std::pair<int, int>> denseSet = perimeterSet;
  for (auto &p : denseTrace)
    denseSet.insert(p);

  f << "{\n";
  f << "  \"cell_size_cm\": " << CELL_SIZE_CM << ",\n";

  // unique perimeter (sorted)
  f << "  \"perimeter\": [\n";
  bool first = true;
  for (auto &p : denseSet) {
    if (!first)
      f << ",\n";
    first = false;
    f << "    {\"x\": " << p.first << ", \"y\": " << p.second << "}";
  }
  f << "\n  ],\n";

  // ordered trace (densified)
  f << "  \"perimeter_trace\": [\n";
  for (size_t i = 0; i < denseTrace.size(); i++) {
    f << "    {\"x\": " << denseTrace[i].first
      << ", \"y\": " << denseTrace[i].second << "}";
    if (i + 1 < denseTrace.size())
      f << ",";
    f << "\n";
  }
  f << "  ],\n";

  f << "  \"corners\": [\n";
  for (size_t i = 0; i < corners.size(); i++) {
    f << "    {\"x\": " << corners[i].first << ", \"y\": " << corners[i].second
      << "}";
    if (i + 1 < corners.size())
      f << ",";
    f << "\n";
  }
  f << "  ]\n";
  f << "}\n";
}

bool runMapping() {
  if (wiringPiSetupGpio() == -1)
    return false;

  Ultrasonic sonarFront(TRIG_FRONT, ECHO_FRONT);
  Ultrasonic sonarLeft(TRIG_LEFT, ECHO_LEFT);
  sonarFront.begin();
  sonarLeft.begin();

  Stepper leftMotor(L_STEP, L_DIR, L_EN, L_DIR_FORWARD_LEVEL);
  Stepper rightMotor(R_STEP, R_DIR, R_EN, R_DIR_FORWARD_LEVEL);
  DifferentialDrive drive(leftMotor, rightMotor);
  drive.begin();

  Pose pose;

  std::set<std::pair<int, int>> perimeterCells;
  std::vector<std::pair<int, int>> perimeterTrace;
  perimeterTrace.reserve(4096);

  std::vector<std::pair<int, int>> corners;

  auto traceAdd = [&](int x, int y) {
    if (perimeterTrace.empty() || perimeterTrace.back().first != x ||
        perimeterTrace.back().second != y) {
      perimeterTrace.push_back({x, y});
      // New: Emit JSON map update
      std::cout << "{\"type\":\"map_point\", \"x\":" << x << ", \"y\":" << y
                << "}" << std::endl;
    }
    perimeterCells.insert({x, y});
  };

  enum class State {
    FIND_INITIAL_EDGE,
    FIND_EDGE_LEFT,
    FIND_CORNER,
    TRACE_PERIMETER,
    DONE,
    FAIL
  };
  State state = State::FIND_INITIAL_EDGE;

  const int MAX_STEPS = 2000;
  int steps = 0;
  int rightTurns = 0;
  bool originSet = false;

  while (steps++ < MAX_STEPS && state != State::DONE && state != State::FAIL) {
    CliffSensors cs = readCliffSensorsFiltered(sonarFront, sonarLeft);

    // JSON Status Update
    std::cout << "{\"type\":\"pose\", \"x\":" << pose.x << ", \"y\":" << pose.y
              << ", \"dir\":\"" << dirName(pose.dir) << "\""
              << ", \"cliff_l\":" << cs.leftCliff
              << ", \"cliff_f\":" << cs.frontCliff
              << ", \"state\":" << (int)state << "}" << std::endl;

    if (state == State::FIND_INITIAL_EDGE) {
      if (cs.frontCliff) {
        // Found edge in front. Turn Right to align left side to it?
        // If we turn right, the edge is on our Left.
        drive.turnRight90();
        turnRight(pose);
        state = State::FIND_EDGE_LEFT; // Now ensure we are adjacent
      } else if (cs.leftCliff) {
        // Found edge on left
        state = State::FIND_CORNER; // or FIND_EDGE_LEFT?
      } else {
        // No cliff, keep finding
        drive.forwardCell();
        stepForward(pose);
      }
    } else if (state == State::FIND_EDGE_LEFT) {
      if (cs.leftCliff) {
        state = State::FIND_CORNER;
      } else {
        // If we just turned right from Initial Edge, we EXPECT leftCliff.
        // If not, maybe we need to move?
        // But original logic was "Turn Right until Left Cliff".
        // If we are at edge, turned right. Left sensor should see it.
        // If not, maybe we are too far?
        // Let's stick to original logic here for now, but adding Move could
        // help. But main fix is FIND_INITIAL_EDGE.
        drive.turnRight90();
        turnRight(pose);
      }
    } else if (state == State::FIND_CORNER) {
      if (!cs.leftCliff) {
        state = State::FIND_EDGE_LEFT;
        continue;
      }

      if (cs.leftCliff && cs.frontCliff) {
        pose.x = 0;
        pose.y = 0;
        pose.dir = Direction::UP;
        originSet = true;

        corners.push_back({0, 0});
        traceAdd(0, 0);
        rightTurns = 0;

        drive.turnRight90();
        turnRight(pose);
        rightTurns++;

        state = State::TRACE_PERIMETER;
      } else {
        drive.forwardCell();
        stepForward(pose);
        if (originSet)
          traceAdd(pose.x, pose.y);
      }
    } else if (state == State::TRACE_PERIMETER) {
      if (!cs.leftCliff) {
        drive.turnLeft90();
        turnLeft(pose);
        continue;
      }

      if (cs.frontCliff) {
        corners.push_back({pose.x, pose.y});
        traceAdd(pose.x, pose.y);

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
        traceAdd(pose.x, pose.y);
      }
    }
  }

  drive.stop();
  if (state == State::FAIL)
    return false;

  exportMapJsonV2(perimeterTrace, perimeterCells, corners);
  return true;
}
