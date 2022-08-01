#ifndef COMMON_H_
#define COMMON_H_

#include <chrono>
#include <iostream>

#include "windows.h"

using Clock = std::chrono::system_clock;
using TimePoint = std::chrono::time_point<Clock>;

std::string SerializeTimePoint(const TimePoint &time,
                               const std::string &format);

int RandomNumber();

inline void WriteLastErrorMessage(const char *proc_name = nullptr,
                                  const char *object = nullptr) {
  LPTSTR buffer{};
  auto error_code = GetLastError();
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                     FORMAT_MESSAGE_ALLOCATE_BUFFER,
                 nullptr, error_code, 0, (LPTSTR)&buffer, 0, nullptr);
  if (object)
    std::cerr << object << ": ";
  if (proc_name)
    std::cerr << proc_name << ": ";
  std::cerr << "Error (code " << error_code << "):\n";
  std::cerr << '\t' << buffer;
  LocalFree(buffer);
}

#endif // COMMON_H_
