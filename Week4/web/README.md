# NeuralGambit — browser build

This folder adds a static browser interface to the existing Week4 engine. It does not replace or edit the search algorithm. The original `search.cpp`, `search.h`, `see.cpp`, `move_ordering.cpp`, `tt.cpp`, and evaluation files are kept unchanged.

## Included from the uploaded project

- The existing Polyglot opening book: `Week4/books/komodo.bin` (about 8.9 MB).
- The existing C++ search, move-ordering, SEE, transposition-table, and evaluation implementation.
- A browser chessboard, legal move handling, promotion selection, undo, side selection, board flip, and PGN export.

The uploaded project contains tablebase source code but no Syzygy `.rtbw`/`.rtbz` tablebase data. The browser build therefore uses `tb_stub.c`, which reports no tablebases available. This does not remove or change the search algorithm; it only means the web build cannot probe Syzygy positions. The native build can continue using the original tablebase code as before.

## Build on Windows

1. Make sure Git for Windows and Python are installed, and that you have an internet connection.
2. From the repository root run:

   ```bat
   cd Week4\web
   setup-and-build.bat
   ```

   This downloads Emscripten into `Week4/web/.tools/` the first time, activates it, and builds the browser engine. If you already have Emscripten installed and activated, you can run `build.bat` directly instead.

This creates `public\engine\neuralgambit.js`, its `.wasm` file, and a `.data` package containing the opening book. These generated files are not checked into this source ZIP because Emscripten must generate them for the target environment.

## Test locally

After a successful build, from `Week4\web` run:

```bat
python -m http.server 8000 --directory public
```

Open `http://localhost:8000`. Do not open `index.html` directly from File Explorer: browsers block the WebAssembly worker/file loading when opened as `file://`.

## Deploy free

### Vercel

Import the repository into Vercel, set **Root Directory** to `Week4/web`, select **Other**, set the output directory to `public`, and leave the build command empty. Commit the generated `Week4/web/public/engine/` files first; Vercel's static deployment will then serve them.

### GitHub Pages

GitHub Pages can serve the static `public` folder using a workflow or a branch configured to publish that folder. Because the engine files are generated build artifacts, make sure the `.js`, `.wasm`, and `.data` files are included in the published output.

## Important browser behaviour

The browser build defines `NG_NO_THREADS`: the engine search runs synchronously inside a Web Worker. This keeps the page UI responsive, but a running search cannot be interrupted by a `stop` message until that search returns. Use the provided bounded depth/time levels. This setting applies only to the browser build. The native UCI build still uses its normal search thread.

The engine's search and evaluation source files are not modified by this web adapter. WebAssembly will still run slower than a native executable on the same machine, and browser performance varies by device.
