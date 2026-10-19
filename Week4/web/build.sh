#!/usr/bin/env bash
# Build the engine to WebAssembly.  Run from anywhere:  ./web/build.sh
#
# Needs the Emscripten SDK (https://emscripten.org/docs/getting_started/downloads.html):
#   git clone https://github.com/emscripten-core/emsdk.git
#   cd emsdk && ./emsdk install latest && ./emsdk activate latest
#   source ./emsdk_env.sh        # (Windows: emsdk_env.bat) -- once per terminal
set -euo pipefail

cd "$(dirname "$0")"            # -> Week4/web
SRC=..                          # -> Week4
OUT=public/engine
mkdir -p "$OUT"

# 1) tablebase stub (plain C, so it is compiled separately from the C++ files)
emcc -O3 -I"$SRC/tablebase" -c tb_stub.c -o tb_stub.o

# 2) engine + book  ->  neuralgambit.js / .wasm / .data
#    -DNG_NO_THREADS      search runs inline in a Web Worker (no std::thread)
#    -fwasm-exceptions    chess.hpp / std::stoll may throw; native wasm EH is fast
#    --preload-file       packs the Polyglot book into the virtual filesystem as
#                         /books/komodo.bin, the path uci.cpp loads at start-up
emcc -O3 -std=c++17 -fwasm-exceptions -DNG_NO_THREADS \
  -I"$SRC/tablebase" \
  "$SRC/search_engine/uci.cpp" \
  "$SRC/search_engine/search.cpp" \
  "$SRC/search_engine/see.cpp" \
  "$SRC/search_engine/move_ordering.cpp" \
  "$SRC/search_engine/tt.cpp" \
  "$SRC/book/book.cpp" \
  "$SRC/arun_eval/eval.cpp" \
  tb_stub.o \
  -o "$OUT/neuralgambit.js" \
  -sMODULARIZE=1 -sEXPORT_NAME=createEngine -sENVIRONMENT=worker \
  -sEXPORTED_FUNCTIONS=_main,_ng_command -sEXPORTED_RUNTIME_METHODS=ccall \
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864 -sSTACK_SIZE=4194304 \
  -sEXIT_RUNTIME=0 \
  --preload-file "$SRC/books/komodo.bin@/books/komodo.bin"

rm -f tb_stub.o
ls -lh "$OUT"
echo "Done. Preview locally:  python3 -m http.server -d public 8000   (then open http://localhost:8000)"
