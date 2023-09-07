#ifndef IOT_CORE_VERSIONINFO_H_
#define IOT_CORE_VERSIONINFO_H_

namespace iot_core {

struct VersionInfo final {
  const char* const commit_hash;
  const char* const version_string;

  VersionInfo(const char* commit_hash, const char* version_string) : commit_hash(commit_hash), version_string(version_string) {}
};

}

#endif
