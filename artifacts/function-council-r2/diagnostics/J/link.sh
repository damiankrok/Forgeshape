#!/bin/bash
# usage: link.sh name   (compiles name.cpp with ASan/UBSan, links against ASan product objects + cached kernel)
CPP=/home/user/Forgeshape/app/src/main/cpp
OBJS=$(ls /tmp/claude-0/fc2-scratch/J/asan_obj/*.o | grep -v selftest)
SELF=$(ls /tmp/claude-0/fc2-scratch/J/asan_obj/*selftest*.o)
g++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -I$CPP -I$CPP/third_party/manifold/include -DMANIFOLD_PAR=-1 -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM $1.cpp $OBJS $SELF /home/user/Forgeshape/build/host-selftests/kernel/libmanifold.a -o $1
