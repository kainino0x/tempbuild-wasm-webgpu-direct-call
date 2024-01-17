#!/bin/bash
set -euo pipefail

FLAGS=()
FLAGS+=(--shell-file=shell.html)
#FLAGS+=(-O0 -g)
FLAGS+=(-O3 -g0)
#FLAGS+=(-flto) # FIXME: -flto not working with reference types: "invalid relocation data index"
#FLAGS+=(--closure=1) # probably doesn't affect benchmark, hard to debug
FLAGS+=(-sUSE_WEBGPU)
FLAGS+=(-sREFERENCE_TYPES) # new experimental flag that sets -mreference-types

set -x

emcc --clear-cache
emcc main.cpp -o     noop-loop.html -DBENCH_MODE_NOOP=1     "${FLAGS[@]}"
emcc main.cpp -o     draw-loop.html -DBENCH_MODE_DRAW=1     "${FLAGS[@]}"
emcc main.cpp -o set-draw-loop.html -DBENCH_MODE_SET_DRAW=1 "${FLAGS[@]}"
