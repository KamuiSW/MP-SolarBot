#include <vector>

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

    void markEdge(int x, int y) { grid[y][x].isEdge = true; }
    void markPath(int x, int y) { grid[y][x].onPath = true; }
};

class RobotLogic {
public:
    int x = 0, y = 0;

    void moveUp() { y--; }
    void moveDown() { y++; }
    void moveLeft() { x--; }
    void moveRight() { x++; }

    void mapRectangle(PanelMap& map) {
        //Top edge
        for (int i = 0; i < map.w; i++) { x = i; y = 0; map.markEdge(x, y); }

        //Right edge
        for (int i = 0; i < map.h; i++) { x = map.w - 1; y = i; map.markEdge(x, y); }

        //Bottom edge
        for (int i = map.w - 1; i >= 0; i--) { x = i; y = map.h - 1; map.markEdge(x, y); }

        //Left edge
        for (int i = map.h - 1; i >= 0; i--) { x = 0; y = i; map.markEdge(x, y); }
    }

    void planSpiral(PanelMap& map) {
        int left = 1, right = map.w - 2;
        int top = 1, bottom = map.h - 2;

        while (left <= right && top <= bottom) {
            //ight
            for (int i = left; i <= right; i++) map.markPath(i, top);
            top++;

            //Down
            for (int i = top; i <= bottom; i++) map.markPath(right, i);
            right--;

            //Left
            if (top <= bottom)
                for (int i = right; i >= left; i--) map.markPath(i, bottom);
            bottom--;

            //Up
            if (left <= right)
                for (int i = bottom; i >= top; i--) map.markPath(left, i);
            left++;
        }
    }
};
