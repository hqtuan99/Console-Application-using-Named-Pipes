#include "common.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

std::string SerializeTimePoint(const TimePoint &time,
                               const std::string &format) {
  std::time_t tt = std::chrono::system_clock::to_time_t(time);
  std::tm tm = *std::gmtime(&tt); // GMT (UTC)
  std::stringstream ss;
  ss << std::put_time(&tm, format.c_str());
  return ss.str();
}

int RandomNumber() {
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<int> uni(1, 9999);
  auto random_integer = uni(rng);
  return random_integer;
}
