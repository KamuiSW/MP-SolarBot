#pragma once
#include <string>

bool buildStainVisitPlan(const std::string& stainsJsonl,
                         int startX, int startY,
                         const std::string& outJson,
                         double cellSizeCm);
