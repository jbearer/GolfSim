#!/usr/bin/env bash
set -e

RUN_CMAKE=true
BUILD_PROJECT=false
RUN_PROJECT=false
BUILD_DEBUG=false

# Process options
while test $# -gt 0
do
    case "$1" in
        -b) BUILD_PROJECT=true
            ;;
        -r) BUILD_PROJECT=true
            RUN_PROJECT=true
            ;;
        -g) BUILD_DEBUG=true
            ;;
        -*) echo "unknown option $1"
            exit 1
            ;;
        *)  echo "unexpected positional argument $1"
            exit 1
            ;;
    esac
    shift
done

if $RUN_CMAKE; then
    CMAKE_FLAGS=""
    if $BUILD_DEBUG; then
        CMAKE_FLAGS+=" -DCMAKE_BUILD_TYPE=Debug"
    fi

    pushd build
    cmake ${CMAKE_FLAGS} ..
    popd
fi

if $BUILD_PROJECT; then
    make -C build
fi

if $RUN_PROJECT; then
    build/bin/golf
fi
