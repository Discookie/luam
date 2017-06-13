@echo off
echo Compiling...
g++ luam.cpp -o luam.exe -static -std=c++11
echo Finished compiling.
pause