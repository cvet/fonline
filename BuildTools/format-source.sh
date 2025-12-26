#!/bin/bash -e

CUR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "$CUR_DIR/../Source" && clang-format -i \
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
