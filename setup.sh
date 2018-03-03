#!/bin/bash

mkdir os
cd os
git clone https://github.com/ReturnInfinity/Pure64.git
git clone https://github.com/ReturnInfinity/BareMetal-kernel.git
sed -i 's/call STAGE3/jmp 0x100000/g' Pure64/src/pure64.asm
cd ..
./build.sh
