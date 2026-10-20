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
let suppressSquareClickUntil = 0;
let premove = null; // { from, to, promotion? } queued while the engine is thinking
let drag = null; // active pointer drag: { from, id, x, y, started, ghost, over }

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
const FILES = 'abcdefgh';

function buildBoard() {
  boardEl.textContent = '';
  squares.clear();
  for (let r = 0; r < 8; r++) {
    for (let f = 0; f < 8; f++) {
      const file = flipped ? 7 - f : f;
      const rank = flipped ? r : 7 - r; // 0-based rank index, 7 == rank 8
      const name = FILES[file] + (rank + 1);
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'sq ' + ((file + rank) % 2 === 1 ? 'light' : 'dark');
      b.dataset.sq = name;
      if (r === 7) {
        const c = document.createElement('span');
        c.className = 'coord file';
        c.textContent = FILES[file];
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
  if (premove && gameResult()) premove = null;

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
    el.classList.toggle('premove', !!premove && (name === premove.from || name === premove.to));

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

  boardEl.classList.toggle('locked', !canInteract());
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
      premove = null;
      cancelDrag();
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
    el.textContent = premove
      ? `${ENGINE_NAME} is thinking… (premove ${premove.from}-${premove.to} queued)`
      : `${ENGINE_NAME} is thinking…`;
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
  $('engine-info').textContent = parts.join(' · ') || ' ';
}

// ---------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------

function canHumanMove() {
  return engineReady && !engineDead && !thinking && !pendingPromotion && !gameResult() && chess.turn() === human;
}

// While it is the engine's turn the user may queue one premove.
function canPremove() {
  return engineReady && !engineDead && !pendingPromotion && !gameResult() && chess.turn() !== human;
}

function canInteract() {
  return canHumanMove() || canPremove();
}

// Legal moves on the human's turn; "pseudo-legal for the human" while the engine is on move.
function movesFor(square) {
  if (chess.turn() === human) return chess.moves({ square, verbose: true });
  try {
    const parts = chess.fen().split(' ');
    parts[1] = human; // pretend it is the human's turn
    parts[3] = '-'; // no en passant for a premove
    return new Chess(parts.join(' ')).moves({ square, verbose: true });
  } catch {
    return [];
  }
}

function selectSquare(name) {
  selected = name;
  legal = movesFor(name);
}

// ---- pointer geometry -----------------------------------------------------

function squareFromPoint(x, y) {
  const r = boardEl.getBoundingClientRect();
  if (x < r.left || x >= r.right || y < r.top || y >= r.bottom) return null;
  const f = Math.floor((x - r.left) / (r.width / 8));
  const row = Math.floor((y - r.top) / (r.height / 8));
  const file = flipped ? 7 - f : f;
  const rank = flipped ? row : 7 - row;
  return FILES[file] + (rank + 1);
}

function clearDragClasses() {
  boardEl.querySelectorAll('.dragging, .drop-target').forEach((el) => el.classList.remove('dragging', 'drop-target'));
}

function cancelDrag() {
  if (!drag) return;
  drag.ghost?.remove();
  drag = null;
  clearDragClasses();
}

function startGhost(from) {
  const piece = chess.get(from);
  if (!piece) return null;
  const ghost = document.createElement('span');
  ghost.className = 'drag-ghost ' + piece.color;
  ghost.textContent = GLYPH[piece.type];
  ghost.style.fontSize = (boardEl.clientWidth / 8) * 0.95 + 'px';
  document.body.append(ghost);
  squares.get(from)?.classList.add('dragging');
  return ghost;
}

function moveGhost(e) {
  if (!drag?.ghost) return;
  // Lift the piece above the fingertip on touch screens so it stays visible.
  const lift = e.pointerType === 'touch' ? boardEl.clientWidth / 8 * 0.7 : 0;
  drag.ghost.style.left = e.clientX + 'px';
  drag.ghost.style.top = e.clientY - lift + 'px';
  const over = squareFromPoint(e.clientX, e.clientY - lift);
  if (over !== drag.over) {
    if (drag.over) squares.get(drag.over)?.classList.remove('drop-target');
    if (over) squares.get(over)?.classList.add('drop-target');
    drag.over = over;
  }
}

function endDrag(e, cancelled) {
  if (!drag || e.pointerId !== drag.id) return;
  const d = drag;
  drag = null;
  d.ghost?.remove();
  clearDragClasses();
  try { boardEl.releasePointerCapture(e.pointerId); } catch { /* already released */ }
  if (!d.started) return; // it was just a tap: the piece stays selected
  suppressSquareClickUntil = performance.now() + 350;
  if (cancelled) { render(); return; }
  const lift = e.pointerType === 'touch' ? boardEl.clientWidth / 8 * 0.7 : 0;
  const to = squareFromPoint(e.clientX, e.clientY - lift);
  if (to && to !== d.from) attemptMove(d.from, to);
  else render();
}

boardEl.addEventListener('pointerdown', (e) => {
  if (e.pointerType === 'mouse' && e.button !== 0) return;
  if (moveMode !== 'drag' || drag || !canInteract()) return;
  const name = squareFromPoint(e.clientX, e.clientY);
  if (!name) return;
  const piece = chess.get(name);
  if (!piece || piece.color !== human) return;
  e.preventDefault();
  try { boardEl.setPointerCapture(e.pointerId); } catch { /* ignore */ }
  selectSquare(name);
  render();
  drag = { from: name, id: e.pointerId, x: e.clientX, y: e.clientY, started: false, ghost: null, over: null };
});

boardEl.addEventListener('pointermove', (e) => {
  if (drag) {
    if (e.pointerId !== drag.id) return;
    if (!drag.started) {
      if (Math.hypot(e.clientX - drag.x, e.clientY - drag.y) < 6) return;
      drag.ghost = startGhost(drag.from);
      if (!drag.ghost) { cancelDrag(); return; }
      drag.started = true;
    }
    e.preventDefault();
    moveGhost(e);
    return;
  }
  // Mouse hover previews the legal moves of your own pieces.
  if (e.pointerType === 'mouse' && moveMode === 'drag' && canInteract()) {
    const name = squareFromPoint(e.clientX, e.clientY);
    const piece = name && chess.get(name);
    if (piece && piece.color === human && name !== selected) {
      selectSquare(name);
      render();
    }
  }
});

boardEl.addEventListener('pointerup', (e) => endDrag(e, false));
boardEl.addEventListener('pointercancel', (e) => endDrag(e, true));
boardEl.addEventListener('lostpointercapture', (e) => { if (drag && e.pointerId === drag.id && !drag.started) drag = null; });

boardEl.addEventListener('click', (event) => {
  if (performance.now() < suppressSquareClickUntil) { event.preventDefault(); return; }
  // detail === 0 means keyboard activation: use the focused square instead of the pointer position.
  const sqEl = event.target.closest?.('.sq');
  const name = event.detail === 0 && sqEl ? sqEl.dataset.sq : squareFromPoint(event.clientX, event.clientY);
  if (!name) return;
  if (moveMode === 'click') {
    onSquare(name);
  } else if (canInteract()) {
    // In drag mode, a tap/click only selects a friendly piece to preview its moves.
    const piece = chess.get(name);
    if (piece && piece.color === human) {
      selectSquare(name);
    } else {
      selected = null;
      legal = [];
      premove = null; // tapping elsewhere cancels a queued premove
    }
    render();
  }
});

// Right-click (or long-press) cancels the premove instead of opening the browser menu.
boardEl.addEventListener('contextmenu', (e) => {
  e.preventDefault();
  if (premove || selected) {
    premove = null;
    selected = null;
    legal = [];
    render();
  }
});

function onSquare(name) {
  if (!canInteract()) return;

  const target = legal.find((m) => m.to === name);
  if (selected && target) {
    attemptMove(selected, name);
    return;
  }

  const piece = chess.get(name);
  if (piece && piece.color === human && name !== selected) {
    selectSquare(name);
  } else {
    selected = null;
    legal = [];
    premove = null;
  }
  render();
}

function attemptMove(from, to) {
  if (!canInteract()) return;
  const piece = chess.get(from);
  if (!piece || piece.color !== human) return;
  selectSquare(from);
  const target = legal.find((m) => m.to === to);
  if (target) {
    const promos = legal.filter((m) => m.to === to && m.promotion);
    if (promos.length) askPromotion(from, to);
    else submitMove({ from, to });
    return;
  }
  const destinationPiece = chess.get(to);
  if (destinationPiece && destinationPiece.color === human) {
    selectSquare(to);
  } else {
    selected = null;
    legal = [];
  }
  render();
}

// Plays the move now on the human's turn, otherwise queues it as the premove.
function submitMove(move) {
  if (chess.turn() === human) {
    playHuman(move);
  } else {
    premove = { from: move.from, to: move.to, promotion: move.promotion };
    selected = null;
    legal = [];
    render();
  }
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
      submitMove({ ...pr, promotion: p });
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
  let m;
  try {
    m = chess.move(move);
  } catch {
    // The position changed under us (e.g. the engine moved while the promotion dialog was open).
    selected = null;
    legal = [];
    render();
    return;
  }
  gameStarted = true;
  clockLastTick = performance.now();
  addIncrement(mover);
  lastMove = { from: m.from, to: m.to };
  selected = null;
  legal = [];
  render();
  engineTurn();
}

function playPremove(pm) {
  if (!pm || gameResult() || chess.turn() !== human) return;
  const ok = chess
    .moves({ verbose: true })
    .some((m) => m.from === pm.from && m.to === pm.to && (m.promotion || '') === (pm.promotion || ''));
  if (ok) playHuman(pm);
  else toast('Premove cancelled');
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
  $('engine-info').textContent = ' ';
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

  // Grab the queued premove before rendering, then try to play it.
  const pm = premove;
  premove = null;
  // Keep a half-made selection alive, but refresh its targets for the new position.
  if (selected && chess.get(selected)?.color === human) legal = movesFor(selected);
  else { selected = null; legal = []; }
  render();
  playPremove(pm);
}

function newGame(color) {
  gameToken++;
  cancelDrag();
  resetClocks();
  thinking = false;
  pendingPromotion = null;
  premove = null;
  chess = customFen ? new Chess(customFen) : new Chess();
  startFen = customFen ? chess.fen() : null;
  chess.setHeader('White', color === 'w' ? 'Player' : ENGINE_NAME);
  chess.setHeader('Black', color === 'w' ? ENGINE_NAME : 'Player');
  human = color;
  flipped = color === 'b';
  selected = null;
  legal = [];
  lastMove = null;
  $('engine-info').textContent = ' ';
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
  cancelDrag();
  thinking = false;
  pendingPromotion = null;
  premove = null;
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
    cancelDrag();
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