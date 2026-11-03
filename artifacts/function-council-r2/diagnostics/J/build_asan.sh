#!/bin/bash
CPP=/home/user/Forgeshape/app/src/main/cpp
src=$1; obj=/tmp/claude-0/fc2-scratch/J/asan_obj/$(basename $src .cpp).o
[ -f $obj ] && exit 0
g++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -I$CPP -I$CPP/third_party/manifold/include -DMANIFOLD_PAR=-1 -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM -w -c $src -o $obj || echo "FAILED $src"
