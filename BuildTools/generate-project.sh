#!/bin/bash -e

if [ "$1" = "" ]; then
    echo "Provide project name"
    exit 1
fi

if [ ! -z "$(ls -A)" ]; then
   echo "Directory for project must be an empty"
   exit 1
fi

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
FO_ROOT="$(cd $CUR_DIR/../ && pwd)"

echo "Copy project template"
cp -r "$FO_ROOT/BuildTools/project-template/." .

echo "Generate .gitignore"
echo "/Workspace" > .gitignore

echo "Fix PROJECT_NAME"
# Todo: fix PROJECT_NAME -> $1

echo "Fix FONLINE_PATH"
# Todo: fix FONLINE_PATH -> FO_ROOT

echo "Initialize git repository"
# Todo: Initialize git repository (optional)

echo "Generate workspace"
# Todo: generate workspace (optional)
