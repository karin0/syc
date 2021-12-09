#!/bin/bash
set -e
if [ -f syc.zip ]; then
    rm syc.zip
fi
cd src
zip ../syc.zip -r . # CMakeLists.txt tests/src/*.py
cd ..
zip -u syc.zip CMakeLists.txt
