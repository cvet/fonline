#!/bin/bash -e

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"

cd "$CUR_DIR/../Source" && clang-format -i \
Applications/*.cpp \
Client/*.cpp Client/*.h \
Common/*.cpp Common/*.h \
Common/AngelScriptExt/*.cpp Common/AngelScriptExt/*.h \
Common/ImGuiExt/*.cpp Common/ImGuiExt/*.h \
Server/*.cpp Server/*.h \
Tools/*.cpp Tools/*.h \
Scripting/*.cpp Scripting/*.h \
Scripting/AngelScript/*.fos \
Frontend/*.cpp Frontend/*.h \
Singleplayer/*.cpp Singleplayer/*.h \
Tests/*.cpp
