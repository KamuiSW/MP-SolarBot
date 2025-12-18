#include "stain_planner.h"
#include "stains.h"
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>

static int manhattan(int ax,int ay,int bx,int by){
    return std::abs(ax-bx) + std::abs(ay-by);
}

bool buildStainVisitPlan(const std::string& stainsJsonl,
                         int startX, int startY,
                         const std::string& outJson,
                         double cellSizeCm)
{
    auto stains = loadStainsJsonl(stainsJsonl);
    if(stains.empty()) {
        std::ofstream f(outJson);
        if(!f.is_open()) return false;
        f << "{\n  \"cell_size_cm\": " << cellSizeCm << ",\n  \"targets\": [],\n  \"path\": []\n}\n";
        return true;
    }

    std::vector<int> used(stains.size(), 0);
    std::vector<Stain> order;
    order.reserve(stains.size());

    int cx=startX, cy=startY;

    for(size_t k=0;k<stains.size();k++){
        int best=-1, bestD=1e9;
        for(size_t i=0;i<stains.size();i++){
            if(used[i]) continue;
            int d=manhattan(cx,cy,stains[i].x,stains[i].y);
            if(d<bestD){ bestD=d; best=(int)i; }
        }
        used[best]=1;
        order.push_back(stains[best]);
        cx=stains[best].x; cy=stains[best].y;
    }

    std::vector<std::pair<int,int>> path;
    cx=startX; cy=startY;

    auto pushCell=[&](int x,int y){
        if(path.empty() || path.back().first!=x || path.back().second!=y)
            path.push_back({x,y});
    };

    pushCell(cx,cy);
    for(const auto& t : order){
        while(cx != t.x){
            cx += (t.x > cx ? 1 : -1);
            pushCell(cx,cy);
        }
        while(cy != t.y){
            cy += (t.y > cy ? 1 : -1);
            pushCell(cx,cy);
        }
    }

    std::ofstream f(outJson);
    if(!f.is_open()) return false;

    f << "{\n";
    f << "  \"cell_size_cm\": " << cellSizeCm << ",\n";
    f << "  \"start\": {\"x\": " << startX << ", \"y\": " << startY << "},\n";

    f << "  \"targets\": [\n";
    for(size_t i=0;i<order.size();i++){
        f << "    {\"x\": " << order[i].x << ", \"y\": " << order[i].y
          << ", \"score\": " << order[i].score << "}";
        if(i+1<order.size()) f << ",";
        f << "\n";
    }
    f << "  ],\n";

    f << "  \"path\": [\n";
    for(size_t i=0;i<path.size();i++){
        double cmx = path[i].first * cellSizeCm;
        double cmy = path[i].second * cellSizeCm;
        f << "    {\"x\": " << path[i].first << ", \"y\": " << path[i].second
          << ", \"cm_x\": " << cmx << ", \"cm_y\": " << cmy << "}";
        if(i+1<path.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n";
    f << "}\n";

    return true;
}
