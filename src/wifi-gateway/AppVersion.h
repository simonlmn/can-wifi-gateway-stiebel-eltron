#ifndef APP_VERSION_H_
#define APP_VERSION_H_

#if __has_include("Version.generated.h")
#  include "Version.generated.h"
#else
#include <iot_core/VersionInfo.h>
static const iot_core::VersionInfo APP_VERSION {"unknown", "v0.0.0-unknown"};
#endif

#endif