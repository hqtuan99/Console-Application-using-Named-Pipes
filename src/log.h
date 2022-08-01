#ifndef LOG_H_
#define LOG_H_

#include <iostream>
#include <sstream>

using namespace std;

enum typelog { kDEBUG, kINFO, kERRORS };

class LOG {
public:
  LOG() { msglevel = kDEBUG; }
  LOG(typelog type) {
    msglevel = type;
    operator<<("[" + getLabel(type) + "]");
  }

  ~LOG() {
    if (opened) {
      cerr << endl;
    }
    opened = false;
  }

  template <class T> LOG &operator<<(const T &msg) {
    cerr << msg;
    opened = true;
    return *this;
  }

private:
  bool opened = false;
  typelog msglevel = kDEBUG;
  inline string getLabel(typelog type) {
    string label;
    switch (type) {
    case kDEBUG:
      label = "DEBUG:";
      break;
    case kINFO:
      label = "SYSTEM ANNOUNCEMENT:";
      break;
    case kERRORS:
      label = "ERROR:";
      break;
    }
    return label;
  }
};

#endif // LOG_H_