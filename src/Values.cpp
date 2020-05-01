#include "Values.h"

std::atomic<uint16_t> Values::rpm;
std::atomic<float> Values::temperature;
std::atomic<float> Values::duty;
std::atomic<unsigned long> Values::temperatureReadout;
