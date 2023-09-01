#ifndef VERSION_H_
#define VERSION_H_

#if __has_include("Version.generated.h")
#  include "Version.generated.h"
#else
namespace version {
  static const char* COMMIT_HASH = "unknown";
  static const char* VERSION_STRING = "v0.0.0-unknown";
};
#endif

#endif