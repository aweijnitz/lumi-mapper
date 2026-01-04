#pragma once

#include <iosfwd>

class ofAppBaseWindow;

namespace projection::renderer {

bool isWindowAvailable(ofAppBaseWindow* window, std::ostream& log, const char* tag);
bool requestWindowClose(ofAppBaseWindow* window, std::ostream& log, const char* tag);

}  // namespace projection::renderer
