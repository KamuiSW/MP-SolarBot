#pragma once

constexpr double ROBOT_WIDTH_CM  = 28.0;
constexpr double ROBOT_LENGTH_CM = 32.0;
constexpr double CELL_SIZE_CM    = 5.0;

constexpr double CLIFF_THRESHOLD_CM = 30.0;

constexpr int TRIG_FRONT = 2;
constexpr int ECHO_FRONT = 3;

constexpr int TRIG_LEFT  = 27;
constexpr int ECHO_LEFT  = 22;

constexpr int L_STEP = 21;
constexpr int L_DIR  = 20;
constexpr int L_EN   = 16;

constexpr int R_STEP = 13;
constexpr int R_DIR  = 19;
constexpr int R_EN   = 26;

constexpr bool L_DIR_FORWARD_LEVEL = true;
constexpr bool R_DIR_FORWARD_LEVEL = true;

constexpr int STEPS_PER_CELL    = 800;
constexpr int STEPS_PER_90_TURN = 600;

constexpr int STEP_PULSE_US = 3;
constexpr int STEP_DELAY_US = 700;

constexpr const char* MAP_JSON_PATH        = "map.json";
constexpr const char* NAVIGATION_JSON_PATH = "navigation.json";
