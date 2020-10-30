@echo off
call setup_cl_x64.bat

if not exist build\debug mkdir build\debug

pushd build\debug
cmake ..\.. -G "Ninja" -DCMAKE_BUILD_TYPE:STRING="Debug"
ninja
popd