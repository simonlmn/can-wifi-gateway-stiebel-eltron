#!/bin/sh

if [ "$1" != "-f" -a "$(git status --porcelain)" ]; then
    echo "Won't make a release, there are uncommitted changes."
    exit 1
fi

git_description=$(git describe --tags 2> /dev/null)
if [ $? -ne 0 ]; then
    git_description='v0.0.0'
fi

version=${git_description#v}
version=${version//./ }
version=( ${version//-/ } )
major=${version[0]}
minor=${version[1]}
patch=${version[2]}
pre_release_label=${version[3]}
last_version=$major.$minor.$patch

new_target_version='0.0.0'
new_pre_release_label=''

echo "Last version was '${last_version}'."

if [[ "$pre_release_label" =~ ^[a-z].*$ ]]; then
    echo "This was labeled as a pre-release '${pre_release_label}'."
    
    make_prerelease=_
    while [ "$make_prerelease" != "y" -a "$make_prerelease" != "n" ]
    do
        read -p "  Make another pre-release? (y/n) " make_prerelease
    done
    if [ "$make_prerelease" == "y" ]; then
        while [[ ! "$new_pre_release_label" =~ ^[a-z].*$ ]]
        do
            read -p "  New pre-release label: " new_pre_release_label
        done
        new_target_version=$last_version
        tag_message="pre-release $new_pre_release_label for"
    else
        make_release_from_prerelease=_
        while [ "$make_release_from_prerelease" != "y" -a "$make_release_from_prerelease" != "n" ]
        do
            read -p "  Make release from pre-release? (y/n) " make_release_from_prerelease
        done
        if [ "$make_release_from_prerelease" == "y" ]; then
            new_target_version=$last_version
            tag_message="final release from $pre_release_label for"
        fi
    fi
fi

if [ -z "$new_tag" ]; then
    bump_patch=_
    while [ "$bump_patch" != "y" -a "$bump_patch" != "n" -a -n "$bump_patch" ]
    do
        read -p "  Bump patch version? (y/N) " bump_patch
    done
    if [ "$bump_patch" == "y" ]; then
        patch=$(($patch + 1))
        tag_message="patch release"
    else
        bump_minor=_
        while [ "$bump_minor" != "y" -a "$bump_minor" != "n" -a -n "$bump_minor" ]
        do
            read -p "  Bump minor version? (y/N) " bump_minor
        done
        if [ "$bump_minor" == "y" ]; then
            minor=$(($minor + 1))
            patch=0
            tag_message="minor release"
        else
            bump_major=_
            while [ "$bump_major" != "y" -a "$bump_major" != "n" -a -n "$bump_major" ]
            do
                read -p "  Bump major version? (y/N) " bump_major
            done
            if [ "$bump_major" == "y" ]; then
                major=$(($major + 1))
                minor=0
                patch=0
                tag_message="major release"
            else
                echo "Must bump some part of the version, aborting..."
                exit 3
            fi
        fi
    fi

    new_target_version=$major.$minor.$patch

    make_prerelease=_
    while [ "$make_prerelease" != "y" -a "$make_prerelease" != "n" ]
    do
        read -p "  Make a pre-release? (y/n) " make_prerelease
    done
    if [ "$make_prerelease" == "y" ]; then
        while [[ ! "$new_pre_release_label" =~ ^[a-z].*$ ]]
        do
            read -p "  Pre-release label: " new_pre_release_label
        done
        tag_message="pre-release $new_pre_release_label for $tag_message"
    fi
fi

if [ -z "$new_pre_release_label" ]; then
    new_version="$new_target_version"
else
    new_version="$new_target_version-$new_pre_release_label"
fi

new_tag="v$new_version"
tag_message="$tag_message $new_target_version"

echo "Creating new release version of '$new_version' with tag '$new_tag' and message '$tag_message'..."

library_properties="library.properties"
if [ -f "$library_properties" ]; then
    temporary_library_properties=$(mktemp)
    sed "s/^version=.*$/version=$new_version/" "$library_properties" > "$temporary_library_properties"
    cat $temporary_library_properties > "$library_properties"
    rm "$temporary_library_properties"
    git add "$library_properties"
fi

if [ -f src/*/Version.h ]; then
    version_header=$(realpath src/*/Version.h)
    library_name=$(basename "$(dirname "$version_header")" | tr '[:lower:]' '[:upper:]')

    echo "Updating version header '$version_header' of '$library_name' with tag '$new_version'..."

    temporary_version_header=$(mktemp)

    sed "s/^#define ${library_name}_VERSION .*$/#define ${library_name}_VERSION $new_version/" "$version_header" > "$temporary_version_header"
    cat $temporary_version_header > "$version_header"
    rm "$temporary_version_header"
    git add "$version_header"
fi

if [ "$(git status --porcelain)" ]; then
    git commit -m "$tag_message"
fi

git tag -a "$new_tag" -m "$tag_message"

if command -v './post-release.sh' &> /dev/null; then
    ./post-release.sh
fi
