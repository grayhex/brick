/*
 * Exports golden conformance fixtures for the C port of Tetris from the TypeScript reference
 * (grayhex/ApolloBrickGames). The reference is executed, never re-implemented: every number in the output
 * is produced by the reference code at the commit recorded in the fixture.
 *
 * Output: tests/fixtures/tetris/tetris.conformance.json (format "brick-conformance/1", see docs/CONFORMANCE.md).
 *
 * Usage (from tools/conformance):
 *   APOLLO_BRICK_GAMES=/path/to/ApolloBrickGames npm run export:tetris -- [--out FILE] [--dump NAME [--tick N]]
 * --dump prints the reference state per tick of one scenario (debug aid for mismatches).
 */
import { execFileSync } from 'node:child_process';
import { mkdirSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';

import { figureName, makePiece, rotatePiece, TETRIS_POOL } from '@apollo/brick/games/tetris/shapes';
import { Rng } from '@apollo/brick/rng';
import { makeBrickInput, type BrickAction, type BrickEvent, type BrickInput } from '@apollo/brick/types';
import { Lab, bestPlacement } from '@apollo-tests/brick/games/tetrisLab';

const APOLLO = process.env.APOLLO_BRICK_GAMES as string;

// ---------------------------------------------------------------- encoding shared with the C runner

/** Key bits; identical to bk_key_t in firmware/include/brick/input.h. */
const KEY_BITS: Record<BrickAction, number> = { left: 1, right: 2, up: 4, down: 8, rotate: 16, fire: 32 };
const ACTIONS = Object.keys(KEY_BITS) as BrickAction[];
/** A tick code is: held keys (bits 0-5) | pressed keys << 6 | command << 12. */
const CMD = { none: 0, start: 1, pause: 2, resume: 3, restart: 4 } as const;
const heldBits = (code: number): number => code & 63;
const pressedBits = (code: number): number => (code >> 6) & 63;

const actionsOf = (bits: number): BrickAction[] => ACTIONS.filter((a) => (bits & KEY_BITS[a]) !== 0);
const bitsOf = (actions: Iterable<BrickAction>): number => {
  let bits = 0;
  for (const a of actions) bits |= KEY_BITS[a];
  return bits;
};
const codeOf = (input: BrickInput, cmd = 0): number => bitsOf(input.held) | (bitsOf(input.pressed) << 6) | (cmd << 12);
const inputOf = (code: number): BrickInput => makeBrickInput(actionsOf(pressedBits(code)), actionsOf(heldBits(code)));

/** Event ids; identical to bk_event_type_t in firmware/include/brick/event.h. */
function encodeEvent(e: BrickEvent): [number, number, number, number] {
  switch (e.type) {
    case 'started': return [1, 0, 0, 0];
    case 'move': return [2, 0, 0, 0];
    case 'rotate': return [3, 0, 0, 0];
    case 'lock': return [4, 0, 0, 0];
    case 'drop': return [5, 0, 0, 0];
    case 'shot': return [6, 0, 0, 0];
    case 'hit': return [7, 0, 0, 0];
    case 'explode': return [8, 0, 0, 0];
    case 'crash': return [9, 0, 0, 0];
    case 'lifeLost': return [10, 0, 0, 0];
    case 'bonus': return [11, 0, 0, 0];
    case 'win': return [12, 0, 0, 0];
    case 'blocked': return [13, 0, 0, 0];
    case 'tone': return [14, e.pad, e.ticks, 0];
    case 'lines': {
      let mask = 0;
      for (const row of e.rows ?? []) mask |= 1 << row;
      return [15, e.count, 0, mask >>> 0];
    }
    case 'levelUp': return [16, e.speed, 0, 0];
    case 'gameOver': return [17, 0, 0, 0];
    case 'paused': return [18, 0, 0, 0];
    case 'resumed': return [19, 0, 0, 0];
  }
}

const PHASE_ID = { ready: 0, playing: 1, paused: 2, over: 3 } as const;

/** FNV-1a 32 over bytes; the digest layout is specified in docs/CONFORMANCE.md. */
class Fnv {
  h = 0x811c9dc5;
  u8(v: number): void {
    this.h = Math.imul(this.h ^ (v & 255), 0x01000193) >>> 0;
  }
  u16(v: number): void {
    this.u8(v);
    this.u8(v >>> 8);
  }
  u32(v: number): void {
    this.u16(v);
    this.u16(v >>> 16);
  }
}

function digest(lab: Lab, events: readonly BrickEvent[]): number {
  const f = new Fnv();
  f.u32(lab.tick);
  f.u8(PHASE_ID[lab.phase]);
  f.u32(lab.score);
  f.u32(lab.hiScore);
  f.u8(lab.speed);
  f.u8(lab.level);
  f.u8(lab.lives === null ? 255 : lab.lives);
  f.u8(lab.overFill);
  for (const v of lab.cells()) f.u8(v);
  const preview = lab.preview();
  f.u8(preview ? 1 : 0);
  if (preview) for (const v of preview) f.u8(v);
  f.u8(events.length);
  for (const e of events) {
    const [type, a, b, c] = encodeEvent(e);
    f.u8(type);
    f.u8(a);
    f.u16(b);
    f.u32(c);
  }
  return f.h;
}

const hex = (v: number, width: number): string => (v >>> 0).toString(16).padStart(width, '0');
const fold16 = (h: number): number => (h ^ (h >>> 16)) & 0xffff;

// ---------------------------------------------------------------- recorder

const SHAPE_INDEX: Record<string, number> = {};
for (const shape of TETRIS_POOL) SHAPE_INDEX[figureName(shape)] = shape;

type Op = (string | number)[];

function applyOp(lab: Lab, op: Op): void {
  const shape = (name: string | number): number => {
    const index = SHAPE_INDEX[String(name)];
    if (index === undefined) throw new Error(`unknown shape ${name}`);
    return index;
  };
  switch (op[0]) {
    case 'wipe': lab.wipe(); break;
    case 'fill': lab.fill(op[1] as number, op[2] as string); break;
    case 'put': lab.put(shape(op[1]!), op[2] as number, op[3] as number, op[4] as number); break;
    case 'setNext': lab.setNext(shape(op[1]!), op[2] as number); break;
    case 'respawn': lab.respawn(shape(op[1]!), op[2] as number); break;
    case 'dueFall': lab.dueFall(); break;
    case 'poke': lab.poke(op[1] as number); break;
    default: throw new Error(`unknown op ${String(op[0])}`);
  }
}

interface Options {
  seed: number;
  speed: number;
  level: number;
  hiScore: number;
}

const CHECKPOINT_EVERY = 250;
const rowsOf = (cells: Uint8Array, width: number): string[] => {
  const out: string[] = [];
  for (let r = 0; r < cells.length / width; r++) out.push([...cells.slice(r * width, (r + 1) * width)].map((v) => '.#*'[v]).join(''));
  return out;
};

/** Executes a scenario against the reference while recording inputs, ops and per-tick digests. */
class Recorder {
  readonly lab: Lab;
  readonly trace: (number[] | Op)[] = [];
  readonly digests: number[] = [];
  readonly checkpoints: Record<string, number>[] = [];
  readonly counts: Record<string, number> = {};
  private run: [number, number] | null = null;
  readonly dump: string[] = [];

  constructor(
    readonly name: string,
    readonly description: string,
    readonly options: Options,
  ) {
    this.lab = new Lab('tetris', { ...options, mode: 0 });
    this.lab.start(); // events of the very first start are not part of the contract (as plainLab in the reference)
  }

  private flushRun(): void {
    if (this.run) this.trace.push([this.run[0], this.run[1]]);
    this.run = null;
  }

  /** Test-setup operation (reference Lab method). */
  op(...op: Op): void {
    this.flushRun();
    applyOp(this.lab, op);
    this.trace.push(op);
  }

  /** One harness tick: optional command, then one game step. */
  tick(code: number): BrickEvent[] {
    const lab = this.lab;
    const cmd = code >> 12;
    const pending = ((lab as unknown as { queue: unknown[] }).queue ?? []).length;
    if (pending > 0 && (cmd !== 0 || lab.phase !== 'playing')) {
      throw new Error(`${this.name}: pending setup events need a plain step in the PLAYING phase`);
    }
    const events: BrickEvent[] = [];
    if (cmd === CMD.start) events.push(...lab.start());
    else if (cmd === CMD.pause) events.push(...lab.pause());
    else if (cmd === CMD.resume) events.push(...lab.resume());
    else if (cmd === CMD.restart) events.push(...lab.restart());
    events.push(...lab.step(inputOf(code)));

    const h = digest(lab, events);
    const index = this.digests.length;
    this.digests.push(fold16(h));
    if ((index + 1) % CHECKPOINT_EVERY === 0) {
      this.checkpoints.push({ i: index, h: h >>> 0, tick: lab.tick, phase: PHASE_ID[lab.phase], score: lab.score, speed: lab.speed });
    }
    for (const e of events) this.counts[e.type] = (this.counts[e.type] ?? 0) + 1;
    this.dump.push(`#${index} code=${code} tick=${lab.tick} ${lab.phase} score=${lab.score} speed=${lab.speed} fill=${lab.overFill} ev=[${events.map((e) => JSON.stringify(e)).join(',')}] h=${hex(fold16(h), 4)}`);

    if (this.run && this.run[0] === code) this.run[1]++;
    else {
      this.flushRun();
      this.run = [code, 1];
    }
    return events;
  }

  // ---- helpers mirroring the reference test helpers ----
  idle(n: number): void {
    for (let i = 0; i < n; i++) this.tick(0);
  }
  /** Pressed on one tick, released on the next. */
  tap(...keys: BrickAction[]): void {
    const bits = bitsOf(keys);
    this.tick(bits | (bits << 6));
    this.tick(0);
  }
  /** Pressed on one tick, not released. */
  press(...keys: BrickAction[]): void {
    const bits = bitsOf(keys);
    this.tick(bits | (bits << 6));
  }
  /** Held for `n` ticks: pressed on the first one. */
  hold(key: BrickAction, n: number): void {
    const bit = KEY_BITS[key];
    for (let i = 0; i < n; i++) this.tick(bit | (i === 0 ? bit << 6 : 0));
  }
  input(input: BrickInput): void {
    this.tick(codeOf(input));
  }
  cmd(c: number): void {
    this.tick(c << 12);
  }
  until(test: () => boolean, limit = 4000): number {
    let n = 0;
    while (!test() && n < limit) {
      this.tick(0);
      n++;
    }
    return n;
  }

  finish(): Record<string, unknown> {
    this.flushRun();
    const lab = this.lab;
    return {
      name: this.name,
      description: this.description,
      options: this.options,
      ticks: this.digests.length,
      trace: this.trace,
      digests: this.digests.map((d) => hex(d, 4)).join(''),
      checkpoints: this.checkpoints,
      final: {
        phase: PHASE_ID[lab.phase],
        tick: lab.tick,
        score: lab.score,
        hiScore: lab.hiScore,
        speed: lab.speed,
        level: lab.level,
        overFill: lab.overFill,
        rows: rowsOf(lab.cells(), 10),
        preview: lab.preview() ? rowsOf(lab.preview()!, 4) : null,
        events: this.counts,
      },
    };
  }
}

// ---------------------------------------------------------------- players

/** The reference Bot picks a placement and uses the hard drop (FIRE). */
class HardBot {
  private target: { angle: number; x: number } | null = null;
  private released = true;
  private attempts = 0;
  next(lab: Lab): BrickInput {
    const piece = lab.current;
    if (!piece || lab.landed || lab.phase !== 'playing') {
      this.target = null;
      this.attempts = 0;
      this.released = true;
      return makeBrickInput();
    }
    if (!this.released) {
      this.released = true;
      return makeBrickInput();
    }
    this.target ??= bestPlacement(lab) ?? { angle: piece.angle, x: lab.px };
    this.attempts++;
    this.released = false;
    if (this.attempts > 24) return makeBrickInput(['fire']);
    if (rotatePiece(piece) !== piece && piece.angle !== this.target.angle) return makeBrickInput(['rotate']);
    if (lab.px < this.target.x) return makeBrickInput(['right']);
    if (lab.px > this.target.x) return makeBrickInput(['left']);
    return makeBrickInput(['fire']);
  }
}

/** Same placement logic, but lands the piece with the soft drop only (the only way on the real console). */
class SoftBot {
  private target: { angle: number; x: number } | null = null;
  private released = true;
  private downHeld = false;
  next(lab: Lab): BrickInput {
    const piece = lab.current;
    if (!piece || lab.landed || lab.phase !== 'playing') {
      this.target = null;
      this.released = true;
      this.downHeld = false;
      return makeBrickInput();
    }
    if (!this.released) {
      this.released = true;
      return makeBrickInput();
    }
    this.target ??= bestPlacement(lab) ?? { angle: piece.angle, x: lab.px };
    if (rotatePiece(piece) !== piece && piece.angle !== this.target.angle) {
      this.released = false;
      return makeBrickInput(['rotate']);
    }
    if (lab.px < this.target.x) {
      this.released = false;
      return makeBrickInput(['right']);
    }
    if (lab.px > this.target.x) {
      this.released = false;
      return makeBrickInput(['left']);
    }
    const first = !this.downHeld;
    this.downHeld = true;
    return makeBrickInput(first ? ['down'] : [], ['down']);
  }
}

/** Random key toggling (the reference "fuzzPlayer" idea) plus occasional pause and restart commands. */
function fuzzRun(r: Recorder, seed: number, ticks: number, bias: number): void {
  let state = seed >>> 0;
  const rnd = (): number => {
    state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
    return state / 4294967296;
  };
  const held = new Set<BrickAction>();
  let overFor = 0;
  for (let i = 0; i < ticks; i++) {
    const phase = r.lab.phase;
    overFor = phase === 'over' ? overFor + 1 : 0;
    if (phase === 'over' && overFor > 20) {
      held.clear();
      r.cmd(CMD.start);
      overFor = 0;
      continue;
    }
    if (phase === 'paused') {
      if (rnd() < 0.05) r.cmd(CMD.resume);
      else r.idle(1);
      continue;
    }
    if (phase === 'playing' && rnd() < 0.002) {
      r.cmd(CMD.pause);
      continue;
    }
    const pressed: BrickAction[] = [];
    for (const a of ACTIONS) {
      const p = a === 'fire' ? bias * 0.15 : a === 'down' ? bias * 0.8 : bias;
      if (rnd() < p) {
        if (held.has(a)) held.delete(a);
        else {
          held.add(a);
          pressed.push(a);
        }
      }
    }
    r.input(makeBrickInput(pressed, held));
  }
}

// ---------------------------------------------------------------- scenarios

interface Scenario {
  name: string;
  description: string;
  options?: Partial<Options>;
  body: (r: Recorder) => void;
}

const base: Options = { seed: 5, speed: 1, level: 1, hiScore: 0 };
const scenarios: Scenario[] = [];
const add = (name: string, description: string, body: (r: Recorder) => void, options: Partial<Options> = {}): void => {
  scenarios.push({ name, description, options, body });
};

const wellOf = (r: Recorder, rows: number, x = 0, pattern = '.#########', shape = 'Line'): void => {
  r.op('wipe');
  for (let y = 0; y < rows; y++) r.op('fill', y, pattern);
  r.op('put', shape, 0, x, 10);
};

// -- gravity, spawn, landing
add('fall-speed-1', 'first gravity step at tick 27, then every ~26.7 ticks', (r) => {
  r.op('wipe');
  r.op('put', 'Square', 0, 4, 12);
  r.idle(26 + 1 + 27);
});
add('fall-speed-10', 'speed 10: fractional gravity period 4.45 ticks', (r) => {
  r.op('wipe');
  r.op('put', 'Square', 0, 4, 12);
  r.idle(60);
}, { speed: 10 });
add('landing-and-spawn', 'piece lands on the floor, the next piece spawns on the next gravity step', (r) => {
  r.op('wipe');
  r.op('put', 'Square', 0, 4, 3);
  r.until(() => r.lab.landed, 200);
  r.idle(60);
});
add('landing-on-ledge', 'piece stops on a ledge', (r) => {
  r.op('wipe');
  r.op('fill', 0, '....#.....');
  r.op('put', 'Square', 0, 4, 8);
  r.until(() => r.lab.landed, 400);
  r.idle(40);
});
for (const shape of ['Z', 'S', 'Line', 'T', 'Square', 'L', 'MirroredL']) {
  add(`spawn-${shape}`, `spawn position and NEXT preview of ${shape}`, (r) => {
    r.op('wipe');
    r.op('setNext', shape, 0);
    r.idle(2);
    r.op('respawn', shape, 0);
    r.idle(30);
    r.tap('rotate');
    r.tap('rotate');
  });
}

// -- keys
add('left-right-and-walls', 'moves, wall contact without sound', (r) => {
  r.op('wipe');
  r.op('put', 'Square', 0, 4, 12);
  r.tap('left');
  r.tap('right');
  r.op('put', 'Square', 0, 0, 12);
  r.tap('left');
  r.op('put', 'Square', 0, 8, 12);
  r.tap('right');
});
add('left-autorepeat', 'auto-repeat after ~130 ms, every tick', (r) => {
  for (const ticks of [1, 8, 9, 10, 11, 14]) {
    r.op('wipe');
    r.op('put', 'Square', 0, 8, 12);
    r.hold('left', ticks);
    r.tap('right');
  }
});
add('blocked-by-cells', 'cannot move into settled cells', (r) => {
  r.op('wipe');
  r.op('fill', 5, '###.......');
  r.op('put', 'Square', 0, 3, 5);
  r.tap('left');
  r.tap('right');
});
add('rotate-rotate-up', 'ROTATE and UP rotate, the square does not', (r) => {
  r.op('wipe');
  r.op('put', 'T', 0, 4, 10);
  r.tap('rotate');
  r.idle(20);
  r.tap('up');
  r.op('put', 'Square', 0, 4, 10);
  r.tap('rotate');
});
add('rotate-at-wall', 'rotation pushes a piece away from the wall; blocked rotation does nothing', (r) => {
  r.op('wipe');
  r.op('put', 'Line', 0, 0, 10);
  r.tap('rotate');
  r.op('put', 'Line', 1, 9, 10);
  r.tap('rotate');
  r.op('wipe');
  r.op('put', 'Line', 0, 3, 10);
  r.op('fill', 11, '.....#....');
  r.tap('rotate');
  r.idle(10);
});
for (const shape of ['Z', 'S', 'Line', 'T', 'L', 'MirroredL']) {
  add(`rotate-all-angles-${shape}`, `four rotations of ${shape} in the open and next to both walls`, (r) => {
    for (const x of [4, 1, 8]) {
      r.op('wipe');
      r.op('put', shape, 0, x, 12);
      for (let i = 0; i < 5; i++) {
        r.tap('rotate');
        r.tap('left');
      }
      for (let i = 0; i < 5; i++) {
        r.tap('rotate');
        r.tap('right');
      }
    }
  });
}
add('soft-drop', 'DOWN drops one cell per tick and resets gravity', (r) => {
  r.op('wipe');
  r.op('put', 'Square', 0, 4, 14);
  r.hold('down', 7);
  r.idle(30);
});
add('hard-drop', 'FIRE: drop then lock in the same tick (engine only)', (r) => {
  r.op('wipe');
  r.op('put', 'Square', 0, 4, 12);
  r.tap('fire');
  r.tap('left');
  r.tap('rotate');
  r.tap('fire');
  r.idle(60);
});
add('pause-resume', 'pause freezes gravity and keys', (r) => {
  r.op('wipe');
  r.op('put', 'Square', 0, 4, 12);
  r.cmd(CMD.pause);
  r.idle(200);
  r.press('left');
  r.cmd(CMD.resume);
  r.idle(30);
});

// -- lines and score
for (const rows of [1, 2, 3, 4]) {
  add(`lines-${rows}`, `${rows} row(s) with one landing: points 1/3/7/15`, (r) => {
    wellOf(r, rows);
    r.tap('fire');
    r.idle(200);
  });
}
add('lines-anchor-right-edge', 'erase animation anchored at the right wall', (r) => {
  wellOf(r, 4, 9, '#########.');
  r.tap('fire');
  r.idle(200);
});
add('lines-anchor-middle', 'erase animation anchored in the middle (T-piece stem)', (r) => {
  r.op('wipe');
  r.op('fill', 0, '#####.####');
  r.op('fill', 1, '####...###');
  r.op('put', 'Line', 1, 5, 10);
  r.tap('fire');
  r.idle(200);
});
add('lines-with-gaps', 'full rows are not contiguous', (r) => {
  r.op('wipe');
  r.op('fill', 0, '.#########');
  r.op('fill', 1, '.#.#.#.#.#');
  r.op('fill', 2, '.#########');
  r.op('fill', 3, '..#.#.#.#.');
  r.op('put', 'Line', 0, 0, 10);
  r.tap('fire');
  r.idle(300);
});
add('row-collapse', 'a cleared row drops everything above', (r) => {
  r.op('wipe');
  r.op('fill', 0, '####..####');
  r.op('fill', 1, '#.#.....#.');
  r.op('put', 'Square', 0, 4, 10);
  r.tap('fire');
  r.idle(200);
});
add('no-points-without-row', 'incomplete row scores nothing', (r) => {
  r.op('wipe');
  r.op('fill', 0, '##.......#');
  r.op('put', 'Square', 0, 4, 10);
  r.tap('fire');
  r.idle(60);
});
add('top-row-not-checked', 'a full top row is not removed', (r) => {
  r.op('wipe');
  r.op('fill', 19, '##########');
  r.op('put', 'Square', 0, 4, 3);
  r.tap('fire');
  r.idle(60);
});
add('keys-during-erase', 'keys are ignored while the erase animation runs; gravity time is skipped, not replayed', (r) => {
  wellOf(r, 2);
  r.tap('fire');
  for (let i = 0; i < 4; i++) {
    r.press('left');
    r.hold('down', 3);
  }
  r.idle(120);
});
add('pause-during-erase', 'pause and resume in the middle of an erase animation', (r) => {
  wellOf(r, 3);
  r.tap('fire');
  r.idle(5);
  r.cmd(CMD.pause);
  r.idle(50);
  r.cmd(CMD.resume);
  r.idle(120);
});
add('speed-up-1000', 'crossing 1000 points raises the speed', (r) => {
  wellOf(r, 1);
  r.op('poke', 999);
  r.tap('fire');
  r.idle(120);
});
add('speed-up-across-2000', 'a four-row landing carries the score over 2000', (r) => {
  wellOf(r, 4);
  r.op('poke', 1990);
  r.tap('fire');
  r.idle(200);
});
add('max-speed-holds', 'speed 10 stays; level and rows do not change', (r) => {
  wellOf(r, 1);
  r.op('poke', 999);
  r.tap('fire');
  r.idle(120);
}, { speed: 10 });

// -- start level, game over
for (const level of [2, 5, 10]) {
  add(`start-level-${level}`, `level ${level}: ${level - 1} random garbage rows`, (r) => {
    r.idle(200);
  }, { level, seed: 11 });
}
add('game-over-by-spawn', 'the new piece does not fit: flood animation, OVER, restart keeps the record', (r) => {
  r.op('wipe');
  r.op('fill', 19, '#.#.#.#.#.');
  r.op('poke', 42);
  r.op('respawn', 'Square', 0);
  r.idle(260);
  r.cmd(CMD.start);
  r.idle(120);
}, { hiScore: 5 });
add('game-over-by-overhang', 'a piece landing above the field ends the game', (r) => {
  r.op('wipe');
  for (let y = 0; y < 18; y++) r.op('fill', y, '#.#.#.#.#.');
  r.op('put', 'Line', 0, 0, 19);
  r.tap('fire');
  r.idle(260);
  r.cmd(CMD.restart);
  r.idle(5);
  r.cmd(CMD.start);
  r.idle(60);
});
add('keys-during-game-over', 'input is ignored during the flood animation', (r) => {
  r.op('wipe');
  r.op('fill', 19, '#.#.#.#.#.');
  r.op('respawn', 'T', 0);
  for (let i = 0; i < 30; i++) r.tap('left', 'rotate');
  r.idle(220);
});

// -- long-running traces
for (const speed of [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]) {
  add(`idle-speed-${speed}`, `no input at speed ${speed}: gravity timing over a long run`, (r) => {
    r.idle(speed <= 2 ? 3300 : 700);
  }, { speed, seed: 100 + speed });
}
add('bot-hard-drop', 'placement bot with hard drop, 6000 ticks', (r) => {
  const bot = new HardBot();
  for (let i = 0; i < 6000 && r.lab.phase === 'playing'; i++) r.input(bot.next(r.lab));
}, { seed: 3 });
add('bot-hard-drop-fast', 'placement bot, start speed 6, level 4', (r) => {
  const bot = new HardBot();
  for (let i = 0; i < 4000 && r.lab.phase === 'playing'; i++) r.input(bot.next(r.lab));
}, { seed: 9, speed: 6, level: 4 });
add('bot-soft-drop', 'placement bot landing with the soft drop only (real console controls), 6000 ticks', (r) => {
  const bot = new SoftBot();
  for (let i = 0; i < 6000 && r.lab.phase === 'playing'; i++) r.input(bot.next(r.lab));
}, { seed: 21, speed: 3 });
add('bot-soft-drop-high-score', 'soft-drop bot until the first thousand points raise the speed', (r) => {
  const bot = new SoftBot();
  r.op('poke', 880);
  for (let i = 0; i < 5000 && r.lab.phase === 'playing'; i++) r.input(bot.next(r.lab));
}, { seed: 33, speed: 7 });
for (let i = 0; i < 8; i++) {
  const seed = 1000 + i * 37;
  add(`fuzz-${i}`, `random keys, pauses and restarts (seed ${seed})`, (r) => fuzzRun(r, seed, 2500, 0.1 + 0.03 * (i % 4)), {
    seed: 50 + i,
    speed: 1 + ((i * 3) % 10),
    level: 1 + ((i * 5) % 10),
  });
}

// ---------------------------------------------------------------- static sections

function rngSection(): unknown[] {
  const out: unknown[] = [];
  for (const seed of [0, 1, 5, 12345, 0xdeadbeef]) {
    const rng = new Rng(seed);
    const ops: unknown[] = [];
    // the same call pattern the game uses: shape, angle, garbage-row coin flips
    const plan: [string, number?][] = [['next'], ['next'], ['int', 7], ['int', 4], ['int', 1], ['int', 10], ['chance', 0.5], ['chance', 0.5], ['int', 7], ['next']];
    for (const [kind, arg] of plan) {
      if (kind === 'next') ops.push(['next', Math.round(rng.next() * 4294967296)]);
      else if (kind === 'int') ops.push(['int', arg, rng.int(arg!)]);
      else ops.push(['chance', arg, rng.chance(arg!) ? 1 : 0]);
    }
    for (let i = 0; i < 20; i++) ops.push(['int', 7, rng.int(7)]);
    out.push({ seed, ops });
  }
  return out;
}

function catalogSection(): unknown[] {
  const out: unknown[] = [];
  for (const shape of TETRIS_POOL) {
    for (let angle = 0; angle < 4; angle++) {
      const p = makePiece(shape, angle);
      out.push({ shape: figureName(shape), angle, cells: p.cells, minX: p.minX, maxX: p.maxX, minY: p.minY, maxY: p.maxY });
    }
  }
  return out;
}

function fallTicksSection(): number[] {
  const out: number[] = [];
  for (let speed = 1; speed <= 10; speed++) {
    const lab = new Lab('tetris', { seed: 5, speed, mode: 0 });
    out.push(lab.fallPeriod);
  }
  return out;
}

// ---------------------------------------------------------------- main

function main(): void {
  const args = process.argv.slice(2);
  const arg = (flag: string): string | undefined => {
    const i = args.indexOf(flag);
    return i >= 0 ? args[i + 1] : undefined;
  };
  const out = resolve(arg('--out') ?? '../../tests/fixtures/tetris/tetris.conformance.json');
  const dumpName = arg('--dump');

  let commit = 'unknown';
  try {
    commit = execFileSync('git', ['-C', APOLLO, 'rev-parse', 'HEAD'], { encoding: 'utf8' }).trim();
  } catch {
    console.warn('warning: reference checkout is not a git repository, commit id is unknown');
  }

  const records: Record<string, unknown>[] = [];
  let totalTicks = 0;
  for (const s of scenarios) {
    const recorder = new Recorder(s.name, s.description, { ...base, ...s.options });
    s.body(recorder);
    if (dumpName === s.name) {
      const from = Number(arg('--tick') ?? 0);
      console.log(recorder.dump.slice(from, from + 40).join('\n'));
    }
    const record = recorder.finish();
    totalTicks += record.ticks as number;
    records.push(record);
  }
  if (dumpName) return;

  const lines: string[] = [];
  lines.push('{');
  lines.push(` "format": "brick-conformance/1",`);
  lines.push(` "game": "tetris",`);
  lines.push(` "reference": ${JSON.stringify({ repo: 'grayhex/ApolloBrickGames', commit, files: ['src/brick/base.ts', 'src/brick/games/tetris.ts', 'src/brick/games/tetris/core.ts', 'src/brick/games/tetris/shapes.ts', 'src/core/rng.ts'] })},`);
  lines.push(` "note": "Generated by tools/conformance/export_tetris.ts. Do not edit by hand. Tick codes, event ids and the digest layout are specified in docs/CONFORMANCE.md.",`);
  lines.push(` "rng": ${JSON.stringify(rngSection())},`);
  lines.push(` "catalog": ${JSON.stringify(catalogSection())},`);
  lines.push(` "fallTicks": ${JSON.stringify(fallTicksSection())},`);
  lines.push(` "scenarios": [`);
  records.forEach((rec, i) => lines.push(`  ${JSON.stringify(rec)}${i + 1 < records.length ? ',' : ''}`));
  lines.push(' ]');
  lines.push('}');
  mkdirSync(dirname(out), { recursive: true });
  writeFileSync(out, lines.join('\n') + '\n');
  console.log(`wrote ${out}: ${records.length} scenarios, ${totalTicks} ticks, reference ${commit}`);
}

main();
