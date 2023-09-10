#!/bin/sh

pushd $1

export version_template_file=$(ls [Vv]ersion.*.in 2> /dev/null)
if [ ! -f "$version_template_file" ]; then
    echo "No version template available in $1, not generating version file"
    exit
fi

version_file_prefix=${version_template_file:0:7}
version_generated_file=$version_file_prefix.generated${version_template_file#$version_file_prefix}
version_generated_file=${version_generated_file%.in}

export git_description=$(git describe --tags --always --dirty --broken)
export git_commit_hash=$(git describe --always --exclude '*' --dirty --broken)

echo "Version template ${version_template_file} available, generating ${version_generated_file}..."

# generate file from template
<"$version_template_file" awk '
    {
    gsub(/\/\*COMMIT_HASH\*\//, ENVIRON["git_commit_hash"]);
    gsub(/\/\*VERSION_STRING\*\//, ENVIRON["git_description"]);
    print;
    }
' > "$version_generated_file"

popd
