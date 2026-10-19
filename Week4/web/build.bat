@echo off
rem Build the engine to WebAssembly on Windows (cmd).  Run from Week4\web.
rem First, in the same terminal:  path\to\emsdk\emsdk_env.bat
setlocal
cd /d "%~dp0"
set SRC=..
set OUT=public\engine
if not exist %OUT% mkdir %OUT%

call emcc -O3 -I%SRC%\tablebase -c tb_stub.c -o tb_stub.o || exit /b 1

call em++ -O3 -std=c++17 -fwasm-exceptions -DNG_NO_THREADS -I%SRC%\tablebase ^
  %SRC%\search_engine\uci.cpp %SRC%\search_engine\search.cpp %SRC%\search_engine\see.cpp ^
  %SRC%\search_engine\move_ordering.cpp %SRC%\search_engine\tt.cpp ^
  %SRC%\book\book.cpp %SRC%\arun_eval\eval.cpp tb_stub.o ^
  -o %OUT%\neuralgambit.js ^
  -sMODULARIZE=1 -sEXPORT_NAME=createEngine -sENVIRONMENT=worker ^
  -sEXPORTED_FUNCTIONS=_main,_ng_command -sEXPORTED_RUNTIME_METHODS=ccall ^
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864 -sSTACK_SIZE=4194304 -sEXIT_RUNTIME=0 ^
  --preload-file %SRC%\books\komodo.bin@/books/komodo.bin || exit /b 1

del tb_stub.o
dir %OUT%
echo Done. Preview:  python -m http.server -d public 8000
