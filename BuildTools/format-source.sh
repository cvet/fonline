#!/bin/bash -e

CUR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if which clang-format-20 >/dev/null 2>&1; then
    CLANG_FORMAT="clang-format-20"
elif which clang-format >/dev/null 2>&1; then
    CLANG_FORMAT="clang-format"
else
    echo "clang-format not found" >&2
    exit 1
fi

cd "$CUR_DIR/../Source" && $CLANG_FORMAT -i \
Applications/*.cpp \
Client/*.cpp Client/*.h \
Common/*.cpp Common/*.h \
Common/ImGuiExt/*.cpp Common/ImGuiExt/*.h \
Essentials/*.cpp Essentials/*.h \
Server/*.cpp Server/*.h \
Tools/*.cpp Tools/*.h \
Scripting/*.cpp \
Scripting/AngelScript/*.cpp Scripting/AngelScript/*.h \
Scripting/AngelScript/CoreScripts/*.fos \
Frontend/*.cpp Frontend/*.h \
Tests/*.cpp
