#include "library/Version.h"

#ifndef PICAZOO_VERSION_STRING
#define PICAZOO_VERSION_STRING "0.1.0-dev"
#endif

namespace picazoo {
const char* version() { return PICAZOO_VERSION_STRING; }
} // namespace picazoo
