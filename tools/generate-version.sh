#!/bin/sh

pushd $1

export git_description=$(git describe --tags --always --dirty --broken)
export git_commit_hash=$(git describe --always --exclude '*' --dirty --broken)

# generate header from template
<"Version.h.in" awk '
    {
    gsub(/\/\*COMMIT_HASH\*\//, ENVIRON["git_commit_hash"]);
    gsub(/\/\*VERSION_STRING\*\//, ENVIRON["git_description"]);
    print;
    }
' > "Version.generated.h"

popd
