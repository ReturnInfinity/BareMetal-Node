#!/bin/bash

cp interrupt.asm os/BareMetal-kernel/src/x86-64/
cd os
cd Pure64
./build.sh
cp src/bootsectors/pxestart.sys ../..
cp src/pure64.sys ../..
cd ..
cd BareMetal-kernel
./build_x86-64.sh
cp kernel.sys ../..
cd ../..
cat pxestart.sys pure64.sys kernel.sys > pxeboot.bin
gcc mcp.c -o mcp
strip mcp
