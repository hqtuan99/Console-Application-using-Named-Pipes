#include "common.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

std::string serializeTimePoint(const TimePoint &time,
                               const std::string &format) {
  std::time_t tt = std::chrono::system_clock::to_time_t(time);
  std::tm tm = *std::gmtime(&tt); // GMT (UTC)
  std::stringstream ss;
  ss << std::put_time(&tm, format.c_str());
  return ss.str();
}
