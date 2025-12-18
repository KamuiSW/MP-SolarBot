#pragma once
#include <string>
#include <vector>

struct Stain {
    int x=0;
    int y=0;
    double score=0.0;
};

void appendStainJsonl(const std::string& path, int x, int y, double cellSizeCm, double score);
std::vector<Stain> loadStainsJsonl(const std::string& path);
bool stainNearExisting(const std::vector<Stain>& stains, int x, int y, int radiusCells);
