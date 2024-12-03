#pragma once


#include <random>
#include <utility>

namespace sdk {

int GetRandomInt(int start, int end);

int GetRandomDouble(int start, int end);

std::pair<double, double> GetMinMax(double first, double second);

} //namespace sdk

