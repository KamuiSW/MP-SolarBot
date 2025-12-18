#include <wiringPi.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#include "config.h"
#include "executor.h"
#include "stains.h"
#include "stain_planner.h"

static const char* STAINS_JSONL_PATH = "stains.jsonl";
static const char* STAIN_PATH_JSON   = "stain_path.json";

struct Waypoint { int x=0, y=0; };

static bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path);
    if(!f.is_open()) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static bool extractWaypoints(const std::string& s, std::vector<Waypoint>& out) {
    size_t k = s.find("\"waypoints\"");
    if(k == std::string::npos) return false;
    size_t lb = s.find('[', k);
    if(lb == std::string::npos) return false;
    size_t rb = s.rfind(']');
    if(rb == std::string::npos || rb <= lb) return false;

    std::string arr = s.substr(lb+1, rb - (lb+1));
    size_t pos = 0;

    while(true) {
        size_t ox = arr.find("\"x\"", pos);
        if(ox == std::string::npos) break;
        size_t cx = arr.find(':', ox);
        if(cx == std::string::npos) break;
        size_t sx = cx + 1;
        while(sx < arr.size() && (arr[sx] == ' ')) sx++;
        size_t ex = sx;
        while(ex < arr.size() && ((arr[ex]>='0'&&arr[ex]<='9') || arr[ex]=='-')) ex++;

        size_t oy = arr.find("\"y\"", ex);
        if(oy == std::string::npos) break;
        size_t cy = arr.find(':', oy);
        if(cy == std::string::npos) break;
        size_t sy = cy + 1;
        while(sy < arr.size() && (arr[sy] == ' ')) sy++;
        size_t ey = sy;
        while(ey < arr.size() && ((arr[ey]>='0'&&arr[ey]<='9') || arr[ey]=='-')) ey++;

        Waypoint w;
        w.x = std::stoi(arr.substr(sx, ex - sx));
        w.y = std::stoi(arr.substr(sy, ey - sy));
        out.push_back(w);
        pos = ey;
    }
    return !out.empty();
}

static bool parseVisionResult(const std::string& s, bool& dirty, double& score) {
    auto kd = s.find("\"dirty\"");
    auto ks = s.find("\"score\"");
    if(kd == std::string::npos || ks == std::string::npos) return false;

    auto cd = s.find(':', kd);
    auto cs = s.find(':', ks);
    if(cd == std::string::npos || cs == std::string::npos) return false;

    auto vd = s.substr(cd+1);
    dirty = (vd.find("true") != std::string::npos);

    size_t i = cs+1;
    while(i < s.size() && (s[i]==' ')) i++;
    size_t j = i;
    while(j < s.size() && ((s[j]>='0'&&s[j]<='9') || s[j]=='-' || s[j]=='.')) j++;
    score = std::stod(s.substr(i, j-i));
    return true;
}

static bool runVisionOnce(bool& dirty, double& score) {
    FILE* pipe = popen("python3 vision_pi.py", "r");
    if(!pipe) return false;

    char buffer[4096];
    std::string out;
    while(fgets(buffer, sizeof(buffer), pipe)) out += buffer;
    pclose(pipe);

    if(out.empty()) return false;
    return parseVisionResult(out, dirty, score);
}

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

static Direction turnTo(Direction cur, Direction target, DifferentialDrive& d) {
    int c = (int)cur;
    int t = (int)target;
    int diff = (t - c + 4) % 4;
    if(diff == 0) return cur;
    if(diff == 1) { d.turnRight90(); return target; }
    if(diff == 3) { d.turnLeft90(); return target; }
    d.turnRight90();
    d.turnRight90();
    return target;
}

static void intensiveCleanAt(int x, int y, int dwell_ms) {
    std::cout << "[Intensive] TODO user code at (" << x << "," << y << ") for " << dwell_ms << "ms\n";
    delay(dwell_ms);
}

bool runExecuteNavigation() {
    if (wiringPiSetupGpio() == -1) return false;

    std::string js;
    if(!readFile(NAVIGATION_JSON_PATH, js)) return false;

    std::vector<Waypoint> path;
    if(!extractWaypoints(js, path)) return false;

    Stepper leftMotor(L_STEP, L_DIR, L_EN, L_DIR_FORWARD_LEVEL);
    Stepper rightMotor(R_STEP, R_DIR, R_EN, R_DIR_FORWARD_LEVEL);
    DifferentialDrive drive(leftMotor, rightMotor);
    drive.begin();

    int curX = path.front().x;
    int curY = path.front().y;
    Direction curDir = Direction::UP;

    std::vector<Stain> stainCache = loadStainsJsonl(STAINS_JSONL_PATH);

    const int CHECK_EVERY_N_CELLS = 3;
    const int DEDUPE_RADIUS_CELLS = 2;

    int cellCounter = 0;

    for(size_t i=1;i<path.size();i++){
        int tx = path[i].x;
        int ty = path[i].y;

        while(curX != tx){
            Direction needed = (tx > curX) ? Direction::RIGHT : Direction::LEFT;
            curDir = turnTo(curDir, needed, drive);
            drive.forwardCell();
            curX += (tx > curX ? 1 : -1);

            cellCounter++;
            if(cellCounter % CHECK_EVERY_N_CELLS == 0){
                bool dirty=false;
                double score=0.0;
                if(runVisionOnce(dirty, score) && dirty){
                    if(!stainNearExisting(stainCache, curX, curY, DEDUPE_RADIUS_CELLS)){
                        appendStainJsonl(STAINS_JSONL_PATH, curX, curY, CELL_SIZE_CM, score);
                        stainCache.push_back({curX, curY, score});
                        std::cout << "[Stain] logged at (" << curX << "," << curY << ") score=" << score << "\n";
                    }
                }
            }
        }

        while(curY != ty){
            Direction needed = (ty > curY) ? Direction::UP : Direction::DOWN;
            curDir = turnTo(curDir, needed, drive);
            drive.forwardCell();
            curY += (ty > curY ? 1 : -1);

            cellCounter++;
            if(cellCounter % CHECK_EVERY_N_CELLS == 0){
                bool dirty=false;
                double score=0.0;
                if(runVisionOnce(dirty, score) && dirty){
                    if(!stainNearExisting(stainCache, curX, curY, DEDUPE_RADIUS_CELLS)){
                        appendStainJsonl(STAINS_JSONL_PATH, curX, curY, CELL_SIZE_CM, score);
                        stainCache.push_back({curX, curY, score});
                        std::cout << "[Stain] logged at (" << curX << "," << curY << ") score=" << score << "\n";
                    }
                }
            }
        }
    }

    drive.stop();

    buildStainVisitPlan(STAINS_JSONL_PATH, curX, curY, STAIN_PATH_JSON, CELL_SIZE_CM);
    std::cout << "[StainPlan] saved " << STAIN_PATH_JSON << "\n";

    std::string sp;
    if(readFile(STAIN_PATH_JSON, sp)){
        std::vector<Waypoint> stainPath;
        if(extractWaypoints(sp, stainPath) && stainPath.size() >= 2){
            drive.begin();
            int sx = stainPath.front().x;
            int sy = stainPath.front().y;
            Direction d = Direction::UP;

            for(size_t i=1;i<stainPath.size();i++){
                int tx = stainPath[i].x;
                int ty = stainPath[i].y;

                while(sx != tx){
                    Direction needed = (tx > sx) ? Direction::RIGHT : Direction::LEFT;
                    d = turnTo(d, needed, drive);
                    drive.forwardCell();
                    sx += (tx > sx ? 1 : -1);
                }
                while(sy != ty){
                    Direction needed = (ty > sy) ? Direction::UP : Direction::DOWN;
                    d = turnTo(d, needed, drive);
                    drive.forwardCell();
                    sy += (ty > sy ? 1 : -1);
                }
            }

            drive.stop();

            auto stains = loadStainsJsonl(STAINS_JSONL_PATH);
            for(const auto& t : stains){
                intensiveCleanAt(t.x, t.y, 5000);
            }
        }
    }

    return true;
}
