#include "Values.h"

std::atomic<uint16_t> Values::rpm;
std::atomic<float> Values::temperature;
std::atomic<uint8_t> Values::duty;
std::atomic<unsigned long> Values::temperatureReadout;
