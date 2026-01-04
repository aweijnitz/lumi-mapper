#include "ShutdownUtils.h"

#include <iostream>

#include <ofMain.h>

namespace projection::renderer {

bool isWindowAvailable(ofAppBaseWindow* window, std::ostream& log, const char* tag) {
  if (window) {
    return true;
  }
  log << "[" << tag << "] window not initialized; exiting immediately" << std::endl;
  return false;
}

bool requestWindowClose(ofAppBaseWindow* window, std::ostream& log, const char* tag) {
  if (!isWindowAvailable(window, log, tag)) {
    return false;
  }
  window->setWindowShouldClose();
  return true;
}

}  // namespace projection::renderer
