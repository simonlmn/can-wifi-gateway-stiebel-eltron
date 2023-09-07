#ifndef VERSION_H_
#define VERSION_H_

#if __has_include("Version.generated.h")
#  include "Version.generated.h"
#else
#include "src/iot-core/VersionInfo.h"
static const iot_core::VersionInfo VERSION {"unknown", "v0.0.0-unknown"};
#endif

#endif