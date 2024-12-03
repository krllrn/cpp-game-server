#include "sdk.h"

namespace sdk {

int GetRandomInt(int start, int end) {
    std::random_device random_device; // Источник энтропии.
    std::mt19937 generator(random_device()); // Генератор случайных чисел.

    std::uniform_int_distribution<> distribution(start, end);
    int r = distribution(generator);

    return r;
}

int GetRandomDouble(int start, int end) {
    std::random_device random_device; // Источник энтропии.
    std::mt19937 generator(random_device()); // Генератор случайных чисел.

    std::uniform_real_distribution<double> distribution(start, end);
    double r = std::round(distribution(generator) * 10) / 10;

    return r;
}

std::pair<double, double> GetMinMax(double first, double second) {
    return std::make_pair(std::min(first, second), std::max(first, second));
}

} // namespace sdk