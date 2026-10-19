// Runs the WebAssembly engine off the main thread so the page stays responsive
// while it searches.  Protocol (plain strings in, objects out):
//   page -> worker : a UCI command, e.g. "position startpos moves e2e4"
//   worker -> page : {type:'ready'} | {type:'line', line:'<one line of UCI output>'}
//                    | {type:'log', line:'<stderr>'}
importScripts('engine/neuralgambit.js');

if (typeof createEngine !== 'function') {
  throw new Error('NeuralGambit WebAssembly factory was not loaded');
}

let engine = null;
const pending = [];

function run(cmd) {
  engine.ccall('ng_command', null, ['string'], [cmd]);
}

self.onmessage = (e) => {
  if (engine) run(e.data);
  else pending.push(e.data); // arrived before the .wasm / book finished loading
};

createEngine({
  print: (line) => postMessage({ type: 'line', line }),
  printErr: (line) => postMessage({ type: 'log', line }),
  // .wasm and .data live next to neuralgambit.js, not next to this worker file
  locateFile: (path) => new URL('engine/' + path, self.location.href).href,
}).then((module) => {
  engine = module;
  postMessage({ type: 'ready' });
  pending.splice(0).forEach(run);
}).catch((error) => {
  postMessage({ type: 'fatal', message: error?.stack || error?.message || String(error) });
});

self.addEventListener('error', (event) => {
  postMessage({ type: 'fatal', message: event.message || 'Uncaught worker error' });
});
