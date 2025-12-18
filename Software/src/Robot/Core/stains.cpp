#include "stains.h"
#include <fstream>
#include <sstream>
#include <cmath>

static bool parseIntField(const std::string& s, const std::string& key, int& out) {
    auto k = s.find("\"" + key + "\"");
    if(k == std::string::npos) return false;
    auto c = s.find(':', k);
    if(c == std::string::npos) return false;
    size_t i = c + 1;
    while(i < s.size() && (s[i]==' ')) i++;
    size_t j = i;
    while(j < s.size() && ((s[j]>='0' && s[j]<='9') || s[j]=='-')) j++;
    out = std::stoi(s.substr(i, j-i));
    return true;
}

static bool parseDoubleField(const std::string& s, const std::string& key, double& out) {
    auto k = s.find("\"" + key + "\"");
    if(k == std::string::npos) return false;
    auto c = s.find(':', k);
    if(c == std::string::npos) return false;
    size_t i = c + 1;
    while(i < s.size() && (s[i]==' ')) i++;
    size_t j = i;
    while(j < s.size() && ((s[j]>='0' && s[j]<='9') || s[j]=='-' || s[j]=='.')) j++;
    out = std::stod(s.substr(i, j-i));
    return true;
}

void appendStainJsonl(const std::string& path, int x, int y, double cellSizeCm, double score) {
    std::ofstream f(path, std::ios::app);
    if(!f.is_open()) return;
    double cx = x * cellSizeCm;
    double cy = y * cellSizeCm;
    f << "{\"x\":" << x << ",\"y\":" << y
      << ",\"cm_x\":" << cx << ",\"cm_y\":" << cy
      << ",\"score\":" << score
      << "}\n";
}

std::vector<Stain> loadStainsJsonl(const std::string& path) {
    std::vector<Stain> out;
    std::ifstream f(path);
    if(!f.is_open()) return out;

    std::string line;
    while(std::getline(f, line)) {
        Stain s;
        if(!parseIntField(line, "x", s.x)) continue;
        if(!parseIntField(line, "y", s.y)) continue;
        parseDoubleField(line, "score", s.score);
        out.push_back(s);
    }
    return out;
}

bool stainNearExisting(const std::vector<Stain>& stains, int x, int y, int radiusCells) {
    for(const auto& s : stains) {
        int dx = s.x - x;
        int dy = s.y - y;
        if(dx*dx + dy*dy <= radiusCells*radiusCells) return true;
    }
    return false;
}
