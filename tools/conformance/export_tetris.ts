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
import { figureName, makePiece, rotatePiece, TETRIS_POOL } from '@apollo/brick/games/tetris/shapes';
import { Rng } from '@apollo/brick/rng';
import { makeBrickInput, type BrickAction, type BrickEvent, type BrickInput } from '@apollo/brick/types';
import { Lab, bestPlacement } from '@apollo-tests/brick/games/tetrisLab';

import { ACTIONS, CMD, Recorder as BaseRecorder, runExporter, type Op, type Options, type Scenario } from './lib';
// ---------------------------------------------------------------- recorder

const SHAPE_INDEX: Record<string, number> = {};
for (const shape of TETRIS_POOL) SHAPE_INDEX[figureName(shape)] = shape;


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

type Recorder = BaseRecorder<Lab>;
const makeRecorder = (s: Scenario<Lab>, options: Options): Recorder =>
  new BaseRecorder<Lab>(s.name, s.description, options, (o) => new Lab('tetris', { ...o, mode: 0 }), applyOp);

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

const base: Options = { seed: 5, speed: 1, level: 1, hiScore: 0 };
const scenarios: Scenario<Lab>[] = [];
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

runExporter(
  {
    game: 'tetris',
    files: ['src/brick/base.ts', 'src/brick/games/tetris.ts', 'src/brick/games/tetris/core.ts', 'src/brick/games/tetris/shapes.ts', 'src/core/rng.ts'],
    sections: [
      ['rng', rngSection()],
      ['catalog', catalogSection()],
      ['fallTicks', fallTicksSection()],
    ],
    defaultOut: '../../tests/fixtures/tetris/tetris.conformance.json',
  },
  scenarios,
  base,
  makeRecorder,
);
