#include <iostream>
#include <thread>
#include <chrono>
#include "mapping.cpp"

using namespace std;

class Simulator {
public:
    PanelMap map;
    RobotLogic robot;

    Simulator(int w, int h) : map(w, h) {}

    void draw() {
        system("cls");
        for (int y = 0; y < map.h; y++) {
            for (int x = 0; x < map.w; x++) {
                if (x == robot.x && y == robot.y) cout << "R";
                else if (map.grid[y][x].isEdge) cout << "#";
                else if (map.grid[y][x].onPath) cout << "*";
                else cout << ".";
            }
            cout << "\n";
        }
        this_thread::sleep_for(chrono::milliseconds(120));
    }

    void simulateMapping() {
        for (robot.x = 0, robot.y = 0; robot.x < map.w; robot.x++) {
            map.markEdge(robot.x, robot.y);
            draw();
        }

        for (robot.y = 0; robot.y < map.h; robot.y++) {
            robot.x = map.w - 1;
            map.markEdge(robot.x, robot.y);
            draw();
        }

        for (robot.x = map.w - 1; robot.x >= 0; robot.x--) {
            robot.y = map.h - 1;
            map.markEdge(robot.x, robot.y);
            draw();
        }

        for (robot.y = map.h - 1; robot.y >= 0; robot.y--) {
            robot.x = 0;
            map.markEdge(robot.x, robot.y);
            draw();
        }
    }

    void simulateSpiral() {
        int left = 1, right = map.w - 2;
        int top = 1, bottom = map.h - 2;

        while (left <= right && top <= bottom) {
            for (int i = left; i <= right; i++) {
                robot.x = i; robot.y = top;
                map.markPath(robot.x, robot.y);
                draw();
            }
            top++;

            for (int i = top; i <= bottom; i++) {
                robot.x = right; robot.y = i;
                map.markPath(robot.x, robot.y);
                draw();
            }
            right--;

            if (top <= bottom)
                for (int i = right; i >= left; i--) {
                    robot.x = i; robot.y = bottom;
                    map.markPath(robot.x, robot.y);
                    draw();
                }
            bottom--;

            if (left <= right)
                for (int i = bottom; i >= top; i--) {
                    robot.x = left; robot.y = i;
                    map.markPath(robot.x, robot.y);
                    draw();
                }
            left++;
        }
    }
};

int main() {
    Simulator sim(30, 12);

    sim.simulateMapping();
    sim.simulateSpiral();

    cout << "done\n";
    return 0;
}
