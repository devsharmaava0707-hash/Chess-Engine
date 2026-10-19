import { Chess } from './vendor/chess.js';

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

const ENGINE_NAME = 'Darinda Dev';

const TIME_CONTROLS = { bullet1: [60, 0], bullet2: [120, 1], blitz3: [180, 2], blitz5: [300, 0], rapid10: [600, 0], unlimited: [0, 0] };

// Filled glyphs for both colours; CSS colours them. U+FE0E forces text (not emoji) style.
const GLYPH = { k: '♚', q: '♛', r: '♜', b: '♝', n: '♞', p: '♟︎' };
const NAMES = { k: 'king', q: 'queen', r: 'rook', b: 'bishop', n: 'knight', p: 'pawn' };

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

const $ = (id) => document.getElementById(id);

function store(key, value) {
  try {
    if (value === undefined) return localStorage.getItem(key);
    localStorage.setItem(key, value);
  } catch {
    /* private mode etc. – the page works without it */
  }
  return null;
}

function toast(message) {
  const el = $('toast');
  el.textContent = message;
  el.classList.add('show');
  clearTimeout(toast.t);
  toast.t = setTimeout(() => el.classList.remove('show'), 1800);
}

// ---------------------------------------------------------------------------
// Engine client: talks UCI to the Web Worker that hosts the WebAssembly engine
// ---------------------------------------------------------------------------

function parseInfo(line) {
  // "info depth 12 seldepth 20 score cp 34 nodes 123456 nps 600000 ... pv e2e4 e7e5"
  const t = line.split(/\s+/);
  const info = {};
  for (let i = 1; i < t.length; i++) {
    switch (t[i]) {
      case 'depth': info.depth = +t[++i]; break;
      case 'nodes': info.nodes = +t[++i]; break;
      case 'nps': info.nps = +t[++i]; break;
      case 'score':
        info.scoreKind = t[++i];
        info.score = +t[++i];
        break;
      case 'pv': info.pv = t.slice(i + 1); i = t.length; break;
      default: break;
    }
  }
  return info;
}

class EngineClient {
  constructor(workerUrl) {
    this.listeners = new Set();
    this.chain = Promise.resolve(); // searches run strictly one after another
    this.worker = new Worker(workerUrl);
    this.ready = new Promise((resolve, reject) => {
      this.worker.addEventListener('message', (e) => {
        const m = e.data;
        if (m.type === 'ready') resolve();
        else if (m.type === 'line') this.listeners.forEach((fn) => fn(m.line));
      });
      this.worker.addEventListener('error', (e) => {
        const err = new Error(e.message || 'The engine crashed or failed to load');
        reject(err);
        this.onCrash?.(err);
      });
    });
  }

  send(cmd) {
    this.worker.postMessage(cmd);
  }

  // Resolves with the engine's bestmove (e.g. "e2e4"), or null if it had none.
  search(positionCmd, goCmd, onInfo) {
    const run = () =>
      new Promise((resolve) => {
        const fn = (line) => {
          if (line.startsWith('info ')) {
            onInfo?.(parseInfo(line));
          } else if (line.startsWith('bestmove')) {
            this.listeners.delete(fn);
            const mv = line.split(/\s+/)[1];
            resolve(!mv || mv === '(none)' || mv === '0000' ? null : mv);
          }
        };
        this.listeners.add(fn);
        this.send(positionCmd);
        this.send(goCmd);
      });
    this.chain = this.chain.then(run, run);
    return this.chain;
  }
}

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------

const params = new URLSearchParams(location.search);
const customFen = params.get('fen'); // e.g. ?fen=8/P7/8/8/8/8/8/k6K w - - 0 1 (handy for testing)

let chess = new Chess();
let startFen = null; // null == standard start position
let human = 'w';
let flipped = false;
let selected = null;
let legal = []; // verbose legal moves from the selected square
let lastMove = null;
let thinking = false;
let gameToken = 0; // bumped on every new game / undo so stale engine replies are ignored
let engineReady = false;
let engineDead = false;
let pendingPromotion = null;
let gameStarted = false;
let clockMs = { w: 180000, b: 180000 };
let clockIncrementMs = 2000;
let clockLastTick = 0;
let clockTimer = null;
let flagged = null;
let moveMode = 'drag';
let draggedFrom = null;
let suppressSquareClickUntil = 0;

const engine = new EngineClient('engine-worker.js');

function positionCommand() {
  const moves = chess
    .history({ verbose: true })
    .map((m) => m.from + m.to + (m.promotion || ''))
    .join(' ');
  const base = startFen ? `position fen ${startFen}` : 'position startpos';
  return moves ? `${base} moves ${moves}` : base;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

const boardEl = $('board');
const squares = new Map(); // "e4" -> button element

function buildBoard() {
  boardEl.textContent = '';
  squares.clear();
  const files = 'abcdefgh';
  for (let r = 0; r < 8; r++) {
    for (let f = 0; f < 8; f++) {
      const file = flipped ? 7 - f : f;
      const rank = flipped ? r : 7 - r; // 0-based rank index, 7 == rank 8
      const name = files[file] + (rank + 1);
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'sq ' + ((file + rank) % 2 === 1 ? 'light' : 'dark');
      b.dataset.sq = name;
      
      // Preview legal moves immediately when hovering over your piece.
      // This does not move the piece or interfere with drag-and-drop.
      b.addEventListener('pointerenter', () => {
        if (moveMode !== 'drag' || !canHumanMove()) return;

        const piece = chess.get(name);

        if (piece && piece.color === human) {
          selected = name;
          legal = chess.moves({ square: name, verbose: true });
          render();
        }
      });

      b.draggable = true;
      b.addEventListener('click', (event) => {
        if (performance.now() < suppressSquareClickUntil) { event.preventDefault(); return; }
        if (moveMode === 'click') {
          onSquare(name);
        } else if (canHumanMove()) {
          // In drag mode, clicks only select a friendly piece to preview its legal moves.
          // The move itself still has to be completed by dragging and dropping.
          const piece = chess.get(name);
          if (piece && piece.color === human) {
            selected = name;
            legal = chess.moves({ square: name, verbose: true });
            render();
          }
        }
      });
      b.addEventListener('dragstart', (event) => {
        const piece = chess.get(name);
        if (moveMode !== 'drag' || !canHumanMove() || !piece || piece.color !== human) {
          event.preventDefault();
          return;
        }
        draggedFrom = name;
        event.dataTransfer.effectAllowed = 'move';
        event.dataTransfer.setData('text/plain', name);
        b.classList.add('dragging');
      });
      b.addEventListener('dragend', () => {
        draggedFrom = null;
        boardEl.querySelectorAll('.dragging, .drop-target').forEach((el) => el.classList.remove('dragging', 'drop-target'));
      });
      b.addEventListener('dragover', (event) => {
        if (moveMode === 'drag' && draggedFrom && canHumanMove()) {
          event.preventDefault();
          event.dataTransfer.dropEffect = 'move';
          b.classList.add('drop-target');
        }
      });
      b.addEventListener('dragleave', () => b.classList.remove('drop-target'));
      b.addEventListener('drop', (event) => {
        if (moveMode !== 'drag' || !draggedFrom || !canHumanMove()) return;
        event.preventDefault();
        const from = draggedFrom;
        draggedFrom = null;
        suppressSquareClickUntil = performance.now() + 350;
        boardEl.querySelectorAll('.dragging, .drop-target').forEach((el) => el.classList.remove('dragging', 'drop-target'));
        attemptMove(from, name);
      });
      if (r === 7) {
        const c = document.createElement('span');
        c.className = 'coord file';
        c.textContent = files[file];
        b.append(c);
      }
      if (f === 0) {
        const c = document.createElement('span');
        c.className = 'coord rank';
        c.textContent = String(rank + 1);
        b.append(c);
      }
      boardEl.append(b);
      squares.set(name, b);
    }
  }
}

function render() {
  // pieces
  const pieceAt = new Map();
  for (const row of chess.board()) for (const p of row) if (p) pieceAt.set(p.square, p);

  const checkSq = chess.inCheck()
    ? [...pieceAt.values()].find((p) => p.type === 'k' && p.color === chess.turn())?.square
    : null;
  const targets = new Map(legal.map((m) => [m.to, m]));

  for (const [name, el] of squares) {
    el.querySelectorAll('.piece, .hint').forEach((n) => n.remove());
    el.classList.toggle('sel', name === selected);
    el.classList.toggle('last', !!lastMove && (name === lastMove.from || name === lastMove.to));
    el.classList.toggle('check', name === checkSq);

    const p = pieceAt.get(name);
    let label = name;
    if (p) {
      const s = document.createElement('span');
      s.className = 'piece ' + p.color;
      s.textContent = GLYPH[p.type];
      el.append(s);
      label += ', ' + (p.color === 'w' ? 'white ' : 'black ') + NAMES[p.type];
    }
    if (targets.has(name)) {
      const h = document.createElement('span');
      h.className = 'hint' + (p || targets.get(name).flags.includes('e') ? ' cap' : '');
      el.append(h);
    }
    el.setAttribute('aria-label', label);
  }

  boardEl.classList.toggle('locked', !canHumanMove());
  boardEl.classList.toggle('drag-mode', moveMode === 'drag');
  boardEl.classList.toggle('click-mode', moveMode === 'click');

  // player labels (top = side shown at the top of the board)
  const topColor = flipped ? 'w' : 'b';
  const label = (color) => {
    const isHuman = color === human;
    const who = isHuman ? 'You' : ENGINE_NAME;
    const cardId = isHuman ? 'you-clock-card' : 'engine-clock-card';
    const clockId = isHuman ? 'you-clock' : 'engine-clock';
    return `<div class="player-identity"><span class="dot ${color}"></span><span class="player-name">${who}</span><span class="tag">${isHuman ? 'You' : 'Engine'}</span></div><div class="player-clock" id="${cardId}"><span class="clock-name">${who}</span><strong id="${clockId}">${formatClock(clockMs[color])}</strong></div>`;
  };
  const top = $('top-player');
  const bottom = $('bottom-player');
  top.innerHTML = label(topColor);
  bottom.innerHTML = label(topColor === 'w' ? 'b' : 'w');
  top.classList.toggle('thinking', thinking && topColor !== human);
  bottom.classList.toggle('thinking', thinking && topColor === human);

  renderMoves();
  renderStatus();
  updateClocks();
  $('undo').disabled = !engineReady || chess.history().length === 0;
  $('new-white').disabled = !engineReady || engineDead;
  $('new-black').disabled = !engineReady || engineDead;
}

function renderMoves() {
  const hist = chess.history();
  const ol = $('moves');
  ol.textContent = '';
  for (let i = 0; i < hist.length; i += 2) {
    const li = document.createElement('li');
    const n = document.createElement('span');
    n.className = 'n';
    n.textContent = i / 2 + 1 + '.';
    const w = document.createElement('span');
    w.textContent = hist[i];
    const b = document.createElement('span');
    b.textContent = hist[i + 1] || '';
    if (i === hist.length - 1) w.className = 'cur';
    if (i + 1 === hist.length - 1) b.className = 'cur';
    li.append(n, w, b);
    ol.append(li);
  }
  ol.scrollTop = ol.scrollHeight;
  const empty = $('empty-moves');
  if (empty) empty.hidden = hist.length > 0;
}

function gameResult() {
  if (flagged) return flagged === human ? `Time expired — ${ENGINE_NAME} wins.` : 'Time expired — you win!';
  if (chess.isCheckmate()) {
    const loser = chess.turn();
    return loser === human ? `Checkmate – ${ENGINE_NAME} wins.` : 'Checkmate – you win! 🎉';
  }
  if (chess.isStalemate()) return 'Draw by stalemate.';
  if (chess.isInsufficientMaterial()) return 'Draw – insufficient material.';
  if (chess.isThreefoldRepetition()) return 'Draw by threefold repetition.';
  if (chess.isDrawByFiftyMoves?.() ?? chess.isDraw()) return 'Draw by the fifty-move rule.';
  return null;
}

function selectedTimeControl() {
  return TIME_CONTROLS[$('time-control')?.value || 'blitz3'] || TIME_CONTROLS.blitz3;
}

function resetClocks() {
  const [seconds, increment] = selectedTimeControl();
  clockMs = { w: seconds * 1000, b: seconds * 1000 };
  clockIncrementMs = increment * 1000;
  clockLastTick = performance.now();
  gameStarted = false;
  flagged = null;
  updateClocks();
}

function formatClock(ms) {
  if ($('time-control')?.value === 'unlimited') return '∞';
  const total = Math.max(0, Math.ceil(ms / 1000));
  const m = Math.floor(total / 60);
  const s = total % 60;
  return `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
}

function updateClocks() {
  const youClock = $('you-clock');
  const engineClock = $('engine-clock');
  const youCard = $('you-clock-card');
  const engineCard = $('engine-clock-card');
  const timeControl = $('time-control');

  // Clock nodes are created by render(); resetClocks() can run before that.
  if (!youClock || !engineClock || !youCard || !engineCard) return;

  const engineColor = human === 'w' ? 'b' : 'w';
  const unlimited = timeControl?.value === 'unlimited';

  youClock.textContent = formatClock(clockMs[human]);
  engineClock.textContent = formatClock(clockMs[engineColor]);

  youCard.classList.toggle(
    'active',
    gameStarted && !flagged && chess.turn() === human && !gameResult()
  );
  engineCard.classList.toggle(
    'active',
    gameStarted && !flagged && chess.turn() !== human && !gameResult()
  );
  youCard.classList.toggle('low', !unlimited && clockMs[human] < 20000);
  engineCard.classList.toggle('low', !unlimited && clockMs[engineColor] < 20000);
}

function startClockTicker() {
  if (clockTimer) clearInterval(clockTimer);
  clockLastTick = performance.now();
  clockTimer = setInterval(() => {
    const now = performance.now();
    const elapsed = now - clockLastTick;
    clockLastTick = now;
    if (!gameStarted || flagged || gameResult() || $('time-control').value === 'unlimited') { updateClocks(); return; }
    const side = chess.turn();
    clockMs[side] = Math.max(0, clockMs[side] - elapsed);
    if (clockMs[side] <= 0) {
      flagged = side;
      gameToken++;
      thinking = false;
      selected = null;
      legal = [];
      render();
    } else updateClocks();
  }, 100);
}

function addIncrement(side) {
  if ($('time-control').value !== 'unlimited') clockMs[side] += clockIncrementMs;
  updateClocks();
}

function renderStatus() {
  const el = $('status');
  el.className = 'status';
  if (engineDead) {
    el.textContent = 'The engine crashed. Reload the page to continue.';
    el.classList.add('error');
    return;
  }
  if (!engineReady) {
    el.textContent = 'Loading engine…';
    return;
  }
  const result = gameResult();
  if (result) {
    el.textContent = result;
    el.classList.add('over');
  } else if (thinking) {
    el.textContent = `${ENGINE_NAME} is thinking…`;
  } else if (chess.turn() === human) {
    el.textContent = chess.inCheck() ? 'Your move – you are in check.' : 'Your move.';
  } else {
    el.textContent = `${ENGINE_NAME} to move…`;
  }
}

function renderInfo(info, sideToMove) {
  const parts = [];
  if (info.depth != null) parts.push(`depth ${info.depth}`);
  if (info.scoreKind === 'cp') {
    const white = sideToMove === 'w' ? info.score : -info.score; // UCI scores are from the mover's view
    parts.push(`eval ${white > 0 ? '+' : ''}${(white / 100).toFixed(2)}`);
  } else if (info.scoreKind === 'mate') {
    const whiteMates = sideToMove === 'w' ? info.score > 0 : info.score < 0;
    parts.push(`${whiteMates ? 'White' : 'Black'} mates in ${Math.abs(info.score)}`);
  }
  if (info.nodes != null) parts.push(`${Math.round(info.nodes / 1000).toLocaleString()}k nodes`);
  $('engine-info').textContent = parts.join(' · ') || ' ';
}

// ---------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------

function canHumanMove() {
  return engineReady && !engineDead && !thinking && !pendingPromotion && !gameResult() && chess.turn() === human;
}

function onSquare(name) {
  if (!canHumanMove()) return;

  const target = legal.find((m) => m.to === name);
  if (selected && target) {
    attemptMove(selected, name);
    return;
  }

  const piece = chess.get(name);
  if (piece && piece.color === human && name !== selected) {
    selected = name;
    legal = chess.moves({ square: name, verbose: true });
  } else {
    selected = null;
    legal = [];
  }
  render();
}

function attemptMove(from, to) {
  if (!canHumanMove()) return;
  const piece = chess.get(from);
  if (!piece || piece.color !== human) return;
  selected = from;
  legal = chess.moves({ square: from, verbose: true });
  const target = legal.find((m) => m.to === to);
  if (target) {
    const promos = legal.filter((m) => m.to === to && m.promotion);
    if (promos.length) askPromotion(from, to);
    else playHuman({ from, to });
    return;
  }
  const destinationPiece = chess.get(to);
  if (destinationPiece && destinationPiece.color === human) {
    selected = to;
    legal = chess.moves({ square: to, verbose: true });
  } else {
    selected = null;
    legal = [];
  }
  render();
}

function askPromotion(from, to) {
  pendingPromotion = { from, to };
  const box = $('promo-choices');
  box.textContent = '';
  for (const p of ['q', 'r', 'b', 'n']) {
    const b = document.createElement('button');
    b.type = 'button';
    b.className = human;
    b.textContent = GLYPH[p];
    b.setAttribute('aria-label', 'Promote to ' + NAMES[p]);
    b.addEventListener('click', () => {
      $('promo').close();
      const pr = pendingPromotion;
      pendingPromotion = null;
      playHuman({ ...pr, promotion: p });
    });
    box.append(b);
  }
  $('promo').showModal();
}

$('promo').addEventListener('cancel', () => {
  // Esc closes the dialog without choosing – just forget the pending move.
  pendingPromotion = null;
  selected = null;
  legal = [];
  render();
});

function playHuman(move) {
  const mover = chess.turn();
  gameStarted = true;
  clockLastTick = performance.now();
  const m = chess.move(move);
  addIncrement(mover);
  lastMove = { from: m.from, to: m.to };
  selected = null;
  legal = [];
  render();
  engineTurn();
}

async function engineTurn() {
  if (!engineReady || engineDead || gameResult() || chess.turn() === human) {
    render();
    return;
  }
  const token = gameToken;
  const mover = chess.turn();
  gameStarted = true;
  clockLastTick = performance.now();

  thinking = true;
  $('engine-info').textContent = ' ';
  render();

  const tc = $('time-control').value;
  // Timed games use the engine's native UCI clock management. Unlimited mode
  // searches deeply instead of imposing an arbitrary per-move strength cap.
  const goCommand = tc === 'unlimited'
    ? 'go depth 64'
    : `go wtime ${Math.max(1, Math.floor(clockMs.w))} btime ${Math.max(1, Math.floor(clockMs.b))} winc ${clockIncrementMs} binc ${clockIncrementMs}`;
  const best = await engine.search(positionCommand(), goCommand, (info) => {
    if (token === gameToken) renderInfo(info, mover);
  });

  if (token !== gameToken) return; // the game was reset while the engine was thinking
  thinking = false;

  if (!best) {
    render();
    return;
  }
  try {
    const m = chess.move({ from: best.slice(0, 2), to: best.slice(2, 4), promotion: best[4] });
    addIncrement(mover);
    lastMove = { from: m.from, to: m.to };
  } catch {
    $('status').textContent = `The engine returned an illegal move (${best}).`;
    $('status').className = 'status error';
    console.error('Illegal engine move', best, chess.fen());
    return;
  }
  render();
}

function newGame(color) {
  gameToken++;
  resetClocks();
  thinking = false;
  pendingPromotion = null;
  chess = customFen ? new Chess(customFen) : new Chess();
  startFen = customFen ? chess.fen() : null;
  chess.setHeader('White', color === 'w' ? 'Player' : ENGINE_NAME);
  chess.setHeader('Black', color === 'w' ? ENGINE_NAME : 'Player');
  human = color;
  flipped = color === 'b';
  selected = null;
  legal = [];
  lastMove = null;
  $('engine-info').textContent = ' ';
  engine.send('ucinewgame');
  if (color === 'b') gameStarted = true;
  clockLastTick = performance.now();
  buildBoard();
  render();
  engineTurn();
}

function undo() {
  if (!engineReady || chess.history().length === 0) return;
  gameToken++; // discard any search in flight
  thinking = false;
  pendingPromotion = null;
  // Take back until it is the human's turn again (normally two plies).
  do {
    chess.undo();
  } while (chess.history().length > 0 && chess.turn() !== human);
  const hist = chess.history({ verbose: true });
  lastMove = hist.length ? { from: hist.at(-1).from, to: hist.at(-1).to } : null;
  selected = null;
  legal = [];
  render();
  engineTurn(); // only does something if it is the engine's turn (e.g. we undid back to the start as Black)
}

async function copyPgn() {
  const pgn = chess.pgn();
  try {
    await navigator.clipboard.writeText(pgn);
    toast('PGN copied');
  } catch {
    window.prompt('Copy the PGN:', pgn);
  }
}

// ---------------------------------------------------------------------------
// Boot
// ---------------------------------------------------------------------------

function init() {
  const tc = $('time-control');
  const savedTc = store('ng-time-control');
  if (savedTc && TIME_CONTROLS[savedTc]) tc.value = savedTc;
  tc.addEventListener('change', () => { store('ng-time-control', tc.value); resetClocks(); toast('Time control updated for the next game'); render(); });
  const modeSelect = $('move-mode');
  const savedMode = store('ng-move-mode');
  if (savedMode === 'drag' || savedMode === 'click') moveMode = savedMode;
  modeSelect.value = moveMode;
  modeSelect.addEventListener('change', () => {
    moveMode = modeSelect.value === 'click' ? 'click' : 'drag';
    store('ng-move-mode', moveMode);
    selected = null;
    legal = [];
    buildBoard();
    render();
    toast(moveMode === 'drag' ? 'Drag pieces to move' : 'Select a piece, then click its destination');
  });

  $('new-white').addEventListener('click', () => newGame('w'));
  $('new-black').addEventListener('click', () => newGame('b'));
  $('undo').addEventListener('click', undo);
  $('flip').addEventListener('click', () => {
    flipped = !flipped;
    buildBoard();
    render();
  });
  $('copy-pgn').addEventListener('click', copyPgn);

  chess = customFen ? new Chess(customFen) : new Chess();
  startFen = customFen ? chess.fen() : null;
  resetClocks();
  startClockTicker();
  buildBoard();
  render();

  engine.onCrash = (err) => {
    engineDead = true;
    thinking = false;
    render();
    console.error('NeuralGambit worker failed:', err);
    $('status').textContent = `Engine failed to load: ${err?.message || 'unknown worker error'}. Check that public/engine contains neuralgambit.js, neuralgambit.wasm and neuralgambit.data.`;
  };
  engine.ready
    .then(() => {
      engine.send('uci');
      engine.send('isready');
      engineReady = true;
      render();
    })
    .catch(() => {
      engineDead = true;
      render();
    });
}

init();
