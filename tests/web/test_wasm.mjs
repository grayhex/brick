// The simulator core as WebAssembly: no imports, the recorded sessions of tests/fixtures/sim must give exactly the
// native results (ticks, pictures shown, pictures waiting, hash of the last picture), and the JavaScript-facing API
// behaves. Usage: node tests/web/test_wasm.mjs build-wasm/brick-sim.wasm tests/fixtures/sim
import { readFileSync } from 'node:fs';

const [wasmPath, fixtureDir] = process.argv.slice(2);
if (!wasmPath || !fixtureDir) {
  console.error('usage: node test_wasm.mjs <brick-sim.wasm> <fixtures/sim>');
  process.exit(2);
}

let checks = 0;
let failures = 0;
function check(cond, what) {
  checks++;
  if (!cond) {
    failures++;
    console.error(`FAIL ${what}`);
  }
}
function eq(actual, expected, what) {
  check(actual === expected, `${what}: actual ${actual}, expected ${expected}`);
}

const module = await WebAssembly.compile(readFileSync(wasmPath));
eq(WebAssembly.Module.imports(module).length, 0, 'the module needs no imports');
const fresh = async () => (await WebAssembly.instantiate(module, {})).exports;

function loadTrace(w, bytes) {
  new Uint8Array(w.memory.buffer, w.bk_w_io_ptr(), bytes.length).set(bytes);
  return w.bk_w_trace_load(bytes.length) === 1;
}

// ---- recorded sessions ----
const expected = readFileSync(`${fixtureDir}/expected.txt`, 'utf8')
  .split('\n')
  .filter((l) => l && !l.startsWith('#'));
check(expected.length >= 3, 'fixtures listed');
for (const line of expected) {
  const [name, ticks, presents, dropped, hash] = line.split(/\s+/);
  const w = await fresh();
  check(w.bk_w_init(8, 0, 0, 0, 1) === 1, 'init');
  check(loadTrace(w, readFileSync(`${fixtureDir}/${name}`)), `${name}: trace loads`);
  let guard = 0;
  while (w.bk_w_replay_done() === 0 && guard++ < 100000) w.bk_w_tick();
  eq(w.bk_w_ticks(), Number(ticks), `${name} ticks`);
  eq(w.bk_w_presents(), Number(presents), `${name} presents`);
  eq(w.bk_w_dropped(), Number(dropped), `${name} dropped`);
  eq((w.bk_w_frame_hash() >>> 0).toString(16).padStart(8, '0'), hash, `${name} last picture hash`);
}

// ---- API behaviour ----
{
  const w = await fresh();
  eq(w.bk_w_init(9, 0, 0, 0, 1), 0, 'odd cell is refused');
  eq(w.bk_w_init(8, 4000, 4000, 0, 1), 0, 'a panel larger than the frame buffer is refused');
  check(w.bk_w_max_frame_w() >= 336 && w.bk_w_max_frame_h() >= 536, 'frame buffer holds the 336x536 candidate');
  check(w.bk_w_init(0, 336, 536, 10, 5) === 1, 'init with a fitted panel');
  w.bk_w_tick();
  eq(w.bk_w_frame_width(), 336, 'frame width');
  eq(w.bk_w_frame_height(), 536, 'frame height');
  eq(w.bk_w_frame_stride(), 42, 'frame stride');
  const bits = new Uint8Array(w.memory.buffer, w.bk_w_frame_bits(), 42 * 536);
  check(bits.some((b) => b !== 0), 'the splash has ink');
  check(w.bk_w_frame_seq() >= 1, 'a picture was shown');
  eq(w.bk_w_screen(), 0, 'splash screen');
  for (let i = 0; i < 70; i++) w.bk_w_tick();
  eq(w.bk_w_screen(), 1, 'home screen after the splash');

  // buttons reach the console: START starts the game
  w.bk_w_set_buttons(128);
  for (let i = 0; i < 3; i++) w.bk_w_tick();
  w.bk_w_set_buttons(0);
  for (let i = 0; i < 5; i++) w.bk_w_tick();
  eq(w.bk_w_screen(), 5, 'game screen after START');

  // the panel limit applies at run time
  const p0 = w.bk_w_presents();
  w.bk_w_set_buttons(2);
  for (let i = 0; i < 60; i++) w.bk_w_tick();
  check(w.bk_w_presents() - p0 <= 11, 'at 10 Hz at most 11 pictures in 60 ticks');
  check(w.bk_w_dropped() > 0, 'pictures waited');

  // the profile is stored when a setting changes, and survives a reboot
  const w2 = await fresh();
  w2.bk_w_init(8, 0, 0, 0, 1);
  for (let i = 0; i < 70; i++) w2.bk_w_tick();
  const before = w2.bk_w_storage_writes();
  w2.bk_w_set_buttons(32); // MODE: settings
  for (let i = 0; i < 3; i++) w2.bk_w_tick();
  w2.bk_w_set_buttons(0);
  for (let i = 0; i < 3; i++) w2.bk_w_tick();
  w2.bk_w_set_buttons(16); // ACTION: toggle SOUND
  for (let i = 0; i < 3; i++) w2.bk_w_tick();
  check(w2.bk_w_storage_writes() > before, 'a setting change wrote the profile');
  const saved = [0, 1].map((s) => ({ len: w2.bk_w_storage_len(s), data: new Uint8Array(w2.memory.buffer, w2.bk_w_storage_ptr(s), w2.bk_w_storage_len(s)).slice() }));
  check(saved[0].len > 0 || saved[1].len > 0, 'a slot holds the profile');

  // restore into a new device the way the page does: init, put the slots back, reboot without wiping
  const w3 = await fresh();
  w3.bk_w_init(8, 0, 0, 0, 1);
  saved.forEach((s, i) => {
    new Uint8Array(w3.memory.buffer, w3.bk_w_storage_ptr(i), s.len).set(s.data);
    w3.bk_w_storage_set_len(i, s.len);
  });
  check(w3.bk_w_reset(0) === 1, 'reboot keeps the profile');
  eq(w3.bk_w_storage_len(0), saved[0].len, 'slot 0 survived');
  eq(w3.bk_w_storage_len(1), saved[1].len, 'slot 1 survived');
  check(w3.bk_w_reset(1) === 1, 'wipe');
  eq(w3.bk_w_storage_len(0) + w3.bk_w_storage_len(1), 0, 'storage wiped');
}

// ---- record, save, load, replay ----
{
  const a = await fresh();
  a.bk_w_init(8, 0, 0, 0, 99);
  check(a.bk_w_record_start() === 1, 'recording starts');
  for (let i = 0; i < 70; i++) a.bk_w_tick();
  a.bk_w_set_buttons(128);
  for (let i = 0; i < 3; i++) a.bk_w_tick();
  a.bk_w_set_buttons(0);
  for (let i = 0; i < 200; i++) a.bk_w_tick();
  a.bk_w_record_stop();
  const n = a.bk_w_trace_save();
  eq(n, 18 + a.bk_w_trace_len(), 'trace file size');
  const file = new Uint8Array(a.memory.buffer, a.bk_w_io_ptr(), n).slice();
  eq(String.fromCharCode(...file.slice(0, 4)), 'BKT1', 'trace magic');

  const b = await fresh();
  b.bk_w_init(8, 0, 0, 0, 1);
  check(loadTrace(b, file), 'trace loads');
  eq(b.bk_w_cfg_seed(), 99, 'the trace brings its seed');
  while (b.bk_w_replay_done() === 0) b.bk_w_tick();
  eq(b.bk_w_frame_hash(), a.bk_w_frame_hash(), 'replay shows the recorded last picture');
  b.bk_w_replay_stop();
  eq(b.bk_w_replaying(), 0, 'control returns to the player');
  const bad = file.slice();
  bad[0] = 0x58;
  check(!loadTrace(b, bad), 'a damaged trace is refused');
}

console.log(`wasm: ${checks} checks, ${failures} failures`);
process.exit(failures === 0 ? 0 : 1);
