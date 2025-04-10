#!/bin/sh
CWD=$(dirname $(dirname $(cd $(dirname $0) && pwd)))
cd ${CWD}
if [ "$1" = "Debug" ];then
    BuildType=Debug
else
    BuildType=Release
fi

DEST=Outputs/${BuildType}

cmake -S . -G Ninja -B ${DEST} -DCMAKE_BUILD_TYPE=${BuildType}
cmake --build ${DEST}
