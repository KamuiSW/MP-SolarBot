#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <queue>
#include <unordered_set>
#include <algorithm>
#include <cmath>

#include "config.h"
#include "navigation.h"

struct Pt { int x=0, y=0; };
struct Waypoint { int x=0, y=0; };

static inline long long key(int x, int y) {
    return ( (long long)x << 32 ) ^ (unsigned int)y;
}

static bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path);
    if(!f.is_open()) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static bool extractNumberAfterKey(const std::string& s, const std::string& k, double& outVal) {
    size_t pos = s.find("\"" + k + "\"");
    if(pos == std::string::npos) return false;
    size_t colon = s.find(':', pos);
    if(colon == std::string::npos) return false;

    size_t start = colon + 1;
    while(start < s.size() && (s[start]==' '||s[start]=='\n'||s[start]=='\r'||s[start]=='\t')) start++;

    size_t end = start;
    while(end < s.size() && ((s[end]>='0'&&s[end]<='9') || s[end]=='.' || s[end]=='-')) end++;

    try { outVal = std::stod(s.substr(start, end - start)); return true; }
    catch(...) { return false; }
}

static bool extractPointsArray(const std::string& s, const std::string& arrayKey, std::vector<Pt>& outPts) {
    size_t k = s.find("\"" + arrayKey + "\"");
    if(k == std::string::npos) return false;
    size_t lb = s.find('[', k);
    if(lb == std::string::npos) return false;

    // Find matching closing bracket (simple version: first ']' after lb)
    size_t rb = s.find(']', lb);
    if(rb == std::string::npos) return false;

    std::string arr = s.substr(lb+1, rb - (lb+1));
    size_t pos = 0;

    while(true) {
        size_t ox = arr.find("\"x\"", pos);
        if(ox == std::string::npos) break;
        size_t cx = arr.find(':', ox);
        if(cx == std::string::npos) break;
        size_t sx = cx + 1;
        while(sx < arr.size() && arr[sx] == ' ') sx++;
        size_t ex = sx;
        while(ex < arr.size() && ((arr[ex]>='0'&&arr[ex]<='9') || arr[ex]=='-')) ex++;

        size_t oy = arr.find("\"y\"", ex);
        if(oy == std::string::npos) break;
        size_t cy = arr.find(':', oy);
        if(cy == std::string::npos) break;
        size_t sy = cy + 1;
        while(sy < arr.size() && arr[sy] == ' ') sy++;
        size_t ey = sy;
        while(ey < arr.size() && ((arr[ey]>='0'&&arr[ey]<='9') || arr[ey]=='-')) ey++;

        Pt p;
        try {
            p.x = std::stoi(arr.substr(sx, ex - sx));
            p.y = std::stoi(arr.substr(sy, ey - sy));
        } catch(...) { return false; }

        outPts.push_back(p);
        pos = ey;
    }

    return !outPts.empty();
}

struct Grid {
    int minX=0, maxX=0, minY=0, maxY=0;
    int W=0, H=0;
    int toGX(int x) const { return x - minX; }
    int toGY(int y) const { return y - minY; }
    int toWX(int gx) const { return gx + minX; }
    int toWY(int gy) const { return gy + minY; }
    bool inG(int gx, int gy) const { return gx>=0 && gx<W && gy>=0 && gy<H; }
};

static Grid makeGridBounds(const std::vector<Pt>& perimeter, int margin) {
    Grid g;
    g.minX = g.maxX = perimeter[0].x;
    g.minY = g.maxY = perimeter[0].y;
    for(const auto& p : perimeter){
        g.minX = std::min(g.minX, p.x);
        g.maxX = std::max(g.maxX, p.x);
        g.minY = std::min(g.minY, p.y);
        g.maxY = std::max(g.maxY, p.y);
    }
    g.minX -= margin; g.maxX += margin;
    g.minY -= margin; g.maxY += margin;
    g.W = g.maxX - g.minX + 1;
    g.H = g.maxY - g.minY + 1;
    return g;
}

static void addManhattanToSet(std::unordered_set<long long>& edgeSet, int x0,int y0,int x1,int y1) {
    int x=x0,y=y0;
    edgeSet.insert(key(x,y));
    while(x!=x1){ x += (x1>x)?1:-1; edgeSet.insert(key(x,y)); }
    while(y!=y1){ y += (y1>y)?1:-1; edgeSet.insert(key(x,y)); }
}

static void floodFillOutside(const Grid& g,
                             const std::unordered_set<long long>& edgeSet,
                             std::vector<uint8_t>& outside)
{
    outside.assign(g.W * g.H, 0);
    std::queue<std::pair<int,int>> q;

    auto blocked = [&](int gx,int gy)->bool{
        int wx=g.toWX(gx), wy=g.toWY(gy);
        return edgeSet.count(key(wx,wy))>0;
    };

    auto pushIf = [&](int gx,int gy){
        if(!g.inG(gx,gy)) return;
        int idx=gx+gy*g.W;
        if(outside[idx]) return;
        if(blocked(gx,gy)) return;
        outside[idx]=1;
        q.push({gx,gy});
    };

    for(int x=0;x<g.W;x++){
        pushIf(x,0);
        pushIf(x,g.H-1);
    }
    for(int y=0;y<g.H;y++){
        pushIf(0,y);
        pushIf(g.W-1,y);
    }

    const int dx[4]={1,-1,0,0};
    const int dy[4]={0,0,1,-1};

    while(!q.empty()){
        auto [cx,cy]=q.front(); q.pop();
        for(int k=0;k<4;k++){
            int nx=cx+dx[k], ny=cy+dy[k];
            if(!g.inG(nx,ny)) continue;
            int nidx=nx+ny*g.W;
            if(outside[nidx]) continue;
            if(blocked(nx,ny)) continue;
            outside[nidx]=1;
            q.push({nx,ny});
        }
    }
}

static void computeInside(const Grid& g,
                          const std::unordered_set<long long>& edgeSet,
                          const std::vector<uint8_t>& outside,
                          std::vector<uint8_t>& inside)
{
    inside.assign(g.W * g.H, 0);
    for(int gy=0;gy<g.H;gy++){
        for(int gx=0;gx<g.W;gx++){
            int idx=gx+gy*g.W;
            int wx=g.toWX(gx), wy=g.toWY(gy);
            bool isEdge = edgeSet.count(key(wx,wy))>0;
            if(!outside[idx] && !isEdge) inside[idx]=1;
        }
    }
}

static void erodeByChebyshev(const Grid& g,
                             const std::vector<uint8_t>& src,
                             int r,
                             std::vector<uint8_t>& dst)
{
    dst.assign(g.W * g.H, 0);
    if(r <= 0){ dst = src; return; }

    std::vector<int> integral((g.W+1)*(g.H+1), 0);
    auto I = [&](int x,int y)->int& { return integral[x + y*(g.W+1)]; };

    for(int y=1;y<=g.H;y++){
        int rowSum=0;
        for(int x=1;x<=g.W;x++){
            rowSum += (src[(x-1) + (y-1)*g.W] ? 1 : 0);
            I(x,y) = I(x, y-1) + rowSum;
        }
    }

    auto rectSum = [&](int x0,int y0,int x1,int y1)->int{
        x0 = std::max(0, x0); y0 = std::max(0, y0);
        x1 = std::min(g.W-1, x1); y1 = std::min(g.H-1, y1);
        int A = integral[(x0) + (y0)*(g.W+1)];
        int B = integral[(x1+1) + (y0)*(g.W+1)];
        int C = integral[(x0) + (y1+1)*(g.W+1)];
        int D = integral[(x1+1) + (y1+1)*(g.W+1)];
        return D - B - C + A;
    };

    for(int gy=0;gy<g.H;gy++){
        for(int gx=0;gx<g.W;gx++){
            if(!src[gx + gy*g.W]) continue;
            int x0=gx-r, y0=gy-r, x1=gx+r, y1=gy+r;
            int area = (std::min(g.W-1,x1)-std::max(0,x0)+1) * (std::min(g.H-1,y1)-std::max(0,y0)+1);
            int sum = rectSum(x0,y0,x1,y1);
            if(sum == area) dst[gx + gy*g.W] = 1;
        }
    }
}

static bool findInsideBounds(const Grid& g,
                             const std::vector<uint8_t>& mask,
                             int& minGX, int& maxGX, int& minGY, int& maxGY)
{
    bool found=false;
    minGX = 1e9; minGY = 1e9;
    maxGX = -1e9; maxGY = -1e9;
    for(int gy=0;gy<g.H;gy++){
        for(int gx=0;gx<g.W;gx++){
            if(mask[gx + gy*g.W]){
                found=true;
                minGX = std::min(minGX, gx);
                maxGX = std::max(maxGX, gx);
                minGY = std::min(minGY, gy);
                maxGY = std::max(maxGY, gy);
            }
        }
    }
    return found;
}

static void addIfValid(const Grid& g,
                       const std::vector<uint8_t>& mask,
                       int gx,int gy,
                       std::vector<Waypoint>& path,
                       std::unordered_set<long long>& used)
{
    if(!g.inG(gx,gy)) return;
    if(!mask[gx + gy*g.W]) return;
    int wx=g.toWX(gx), wy=g.toWY(gy);
    long long k=key(wx,wy);
    if(used.count(k)) return;
    used.insert(k);
    path.push_back({wx,wy});
}

static std::vector<Waypoint> buildRectSpiral(const Grid& g,
                                             const std::vector<uint8_t>& mask,
                                             int stepCells)
{
    int minGX,maxGX,minGY,maxGY;
    if(!findInsideBounds(g, mask, minGX,maxGX,minGY,maxGY)) return {};

    std::vector<Waypoint> path;
    path.reserve(20000);
    std::unordered_set<long long> used;
    used.reserve(20000);

    int left=minGX, right=maxGX, bottom=minGY, top=maxGY;

    while(left <= right && bottom <= top){
        for(int gx=left; gx<=right; gx++) addIfValid(g, mask, gx, bottom, path, used);
        for(int gy=bottom; gy<=top; gy++) addIfValid(g, mask, right, gy, path, used);
        for(int gx=right; gx>=left; gx--) addIfValid(g, mask, gx, top, path, used);
        for(int gy=top; gy>=bottom; gy--) addIfValid(g, mask, left, gy, path, used);

        left  += stepCells;
        right -= stepCells;
        bottom+= stepCells;
        top   -= stepCells;
    }

    return path;
}

static bool writeNavigationJson(const std::vector<Waypoint>& path, double cellSizeCm) {
    std::ofstream f(NAVIGATION_JSON_PATH);
    if(!f.is_open()) return false;

    f << "{\n";
    f << "  \"cell_size_cm\": " << cellSizeCm << ",\n";
    f << "  \"robot_width_cm\": " << ROBOT_WIDTH_CM << ",\n";
    f << "  \"robot_length_cm\": " << ROBOT_LENGTH_CM << ",\n";
    f << "  \"waypoints\": [\n";

    for(size_t i=0;i<path.size();i++){
        const auto& w = path[i];
        double cx = w.x * cellSizeCm;
        double cy = w.y * cellSizeCm;
        f << "    {\"x\": " << w.x << ", \"y\": " << w.y
          << ", \"cm_x\": " << cx << ", \"cm_y\": " << cy << "}";
        if(i+1<path.size()) f << ",";
        f << "\n";
    }

    f << "  ]\n";
    f << "}\n";
    return true;
}

bool runNavigation() {
    std::string js;
    if(!readFile(MAP_JSON_PATH, js)) return false;

    double cellSizeCm = CELL_SIZE_CM;
    extractNumberAfterKey(js, "cell_size_cm", cellSizeCm);

    // Prefer perimeter_trace if available
    std::vector<Pt> trace;
    bool hasTrace = extractPointsArray(js, "perimeter_trace", trace);

    std::vector<Pt> perimeter;
    if(!hasTrace)
    {
        if(!extractPointsArray(js, "perimeter", perimeter)) return false;
    }
    else
    {
        perimeter = trace;
    }

    std::unordered_set<long long> edgeSet;
    edgeSet.reserve(perimeter.size()*4);

    // Densify boundary segments (watertight)
    if(perimeter.size() >= 2){
        for(size_t i=0;i+1<perimeter.size();i++){
            addManhattanToSet(edgeSet, perimeter[i].x, perimeter[i].y, perimeter[i+1].x, perimeter[i+1].y);
        }
        addManhattanToSet(edgeSet, perimeter.back().x, perimeter.back().y, perimeter.front().x, perimeter.front().y);
    } else {
        return false;
    }

    Grid g = makeGridBounds(perimeter, 4);

    std::vector<uint8_t> outside;
    floodFillOutside(g, edgeSet, outside);

    std::vector<uint8_t> inside;
    computeInside(g, edgeSet, outside, inside);

    int insideCount = 0;
    for(auto v : inside) if(v) insideCount++;
    if(insideCount == 0) return false;

    int stepCells = (int)std::ceil(ROBOT_WIDTH_CM / cellSizeCm);
    stepCells = std::max(1, stepCells);

    // More conservative clearance than before (avoid deleting all inside)
    int clearanceCells = (int)std::ceil((ROBOT_WIDTH_CM * 0.5) / cellSizeCm);
    clearanceCells = std::max(1, clearanceCells);

    std::vector<uint8_t> safeInside;
    erodeByChebyshev(g, inside, clearanceCells, safeInside);

    int safeCount = 0;
    for(auto v : safeInside) if(v) safeCount++;
    if(safeCount == 0) return false;

    auto path = buildRectSpiral(g, safeInside, stepCells);
    if(path.empty()) return false;

    return writeNavigationJson(path, cellSizeCm);
}
