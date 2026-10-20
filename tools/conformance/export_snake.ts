/*
 * Exports golden conformance fixtures for the C port of Snake from the TypeScript reference
 * (grayhex/ApolloBrickGames, library game `snake`, main profile = E88 no. 5 / E-23 no. 93 of the ROM review plus the
 * variant 02 "Frame" reconstructed from the wiki). The reference is executed, never re-implemented.
 *
 * Output: tests/fixtures/snake/snake.conformance.json (format "brick-conformance/1", see docs/CONFORMANCE.md).
 *
 * Usage (from tools/conformance):
 *   APOLLO_BRICK_GAMES=/path/to/ApolloBrickGames npm run export:snake -- [--out FILE] [--dump NAME [--tick N]]
 */
import { e88SnakeTicks } from '@apollo/brick/e88';
import { SnakeGame } from '@apollo/brick/games/library/arcade';
import { Rng } from '@apollo/brick/rng';
import { makeBrickInput, type BrickAction } from '@apollo/brick/types';

import { CMD, Recorder as BaseRecorder, runExporter, type Op, type Options, type Scenario } from './lib';

// ---------------------------------------------------------------- setup operations (test only)

/** The reference keeps these members private or protected; the scenarios reach in the way its own tests do. */
interface SnakeInternals {
  body: number[];
  directionNow: number;
  food: number;
  eaten: number;
  timer: number;
  setLives(count: number): void;
  setScore(value: number): void;
}
const internals = (g: SnakeGame): SnakeInternals => g as unknown as SnakeInternals;

function applyOp(g: SnakeGame, op: Op): void {
  const s = internals(g);
  switch (op[0]) {
    case 'snake': // ['snake', direction, ...cells], head first
      s.directionNow = op[1] as number;
      s.body = op.slice(2) as number[];
      break;
    case 'food': s.food = op[1] as number; break;
    case 'eaten': s.eaten = op[1] as number; break;
    case 'timer': s.timer = op[1] as number; break;
    case 'lives': s.setLives(op[1] as number); break;
    case 'poke': s.setScore(op[1] as number); break;
    default: throw new Error(`unknown op ${String(op[0])}`);
  }
}

const makeLab = (o: Options & { mode?: number }): SnakeGame => new SnakeGame({ seed: o.seed, speed: o.speed, level: o.level, hiScore: o.hiScore, mode: o.mode ?? 0 });
type Recorder = BaseRecorder<SnakeGame>;
const makeRecorder = (s: Scenario<SnakeGame>, options: Options): Recorder => new BaseRecorder<SnakeGame>(s.name, s.description, options, makeLab, applyOp);

// Directions of the reference: 0 up, 1 right, 2 down, 3 left. Cell = y * 10 + x, y counted from the top.
const UP: BrickAction = 'up';
const RIGHT: BrickAction = 'right';
const DOWN: BrickAction = 'down';
const LEFT: BrickAction = 'left';
const cell = (x: number, y: number): number => y * 10 + x;
const isBusy = (g: SnakeGame): boolean => (g as unknown as { busy: boolean }).busy;
/** A path through the whole field, row by row, turning at the ends (consecutive cells are neighbours). */
const snakePath = (rows: number): number[] => Array.from({ length: rows * 10 }, (_, i) => cell(Math.floor(i / 10) % 2 === 0 ? i % 10 : 9 - (i % 10), Math.floor(i / 10)));

// ---------------------------------------------------------------- scenarios

const base: Options & { mode?: number } = { seed: 5, speed: 1, level: 1, hiScore: 0 };
const scenarios: Scenario<SnakeGame>[] = [];
const add = (name: string, description: string, body: (r: Recorder) => void, options: Partial<Options & { mode: number }> = {}): void => {
  scenarios.push({ name, description, options, body });
};

/** A worm of `n` cells in a row, head at (hx, y), heading right (the body trails to the left). */
const rowWorm = (hx: number, y: number, n: number): number[] => Array.from({ length: n }, (_, i) => cell(hx - i, y));

// -- movement cadence per speed: the fractional step period of the ROM (332.358 ... 37.038 ms)
for (const speed of [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]) {
  add(`cadence-speed-${speed}`, `straight run at speed ${speed}: wall hits, clear scenes and fractional timer carry`, (r) => {
    r.idle(speed <= 2 || speed === 5 || speed === 10 ? 1100 : 420);
  }, { speed });
}

// -- turning
add('turn-up-steps-at-once', 'a direction key moves the head on the same tick and restarts the timer', (r) => {
  r.idle(10);
  r.tap(UP);
  r.idle(25);
});
add('turn-down-steps-at-once', 'turning down from the heading right', (r) => {
  r.op('snake', 1, ...rowWorm(4, 10, 3));
  r.idle(5);
  r.tap(DOWN);
  r.idle(60);
});
add('turn-down-from-the-bottom-row-loses', 'the start row is the bottom row: turning down leaves the field', (r) => {
  r.idle(5);
  r.tap(DOWN);
  r.idle(70);
});
add('same-direction-tap-steps', 'tapping the current direction is a step, not a no-op', (r) => {
  for (let i = 0; i < 3; i++) r.tap(RIGHT);
  r.idle(40);
});
add('tap-spam', 'fast alternating turns', (r) => {
  r.op('snake', 1, ...rowWorm(2, 10, 3));
  for (const k of [UP, RIGHT, DOWN, RIGHT, UP, RIGHT, DOWN, RIGHT]) r.tap(k);
  r.idle(30);
});
add('rotate-is-up', 'the big action key (ROTATE) is the fourth direction: up', (r) => {
  r.idle(8);
  r.tap('rotate');
  r.idle(50);
});
add('fire-is-up', 'the engine alias FIRE also means up', (r) => {
  r.idle(8);
  r.tap('fire');
  r.idle(50);
});
add('rotate-held-no-turbo', 'holding ROTATE does not repeat or speed the snake up', (r) => {
  r.hold('rotate', 90);
  r.idle(30);
});
add('held-direction-without-press', 'a held key without a rising edge does nothing', (r) => {
  r.op('snake', 1, ...rowWorm(3, 10, 3));
  for (let i = 0; i < 60; i++) r.tick(8); // DOWN (bit 3) held, never pressed
});
add('key-priority-up-over-right', 'two keys on one tick: the first in order up, right, down, left wins', (r) => {
  r.op('snake', 1, ...rowWorm(3, 10, 3));
  r.press(RIGHT, UP);
  r.idle(1);
  r.press(DOWN, LEFT);
  r.idle(40);
});
add('rotate-and-direction-together', 'a direction key beats ROTATE on the same tick', (r) => {
  r.op('snake', 1, ...rowWorm(3, 10, 3));
  r.press('rotate', DOWN);
  r.idle(30);
});

// -- reversing: head and tail swap
add('reverse-straight', 'LEFT while heading right swaps head and tail, no step', (r) => {
  r.idle(7);
  r.tap(LEFT);
  r.idle(70);
});
add('reverse-bent', 'reversing a bent worm: the direction comes from the former tail', (r) => {
  r.op('snake', 0, cell(5, 8), cell(5, 9), cell(5, 10), cell(4, 10), cell(3, 10));
  r.idle(3);
  r.tap(DOWN);
  r.idle(40);
});
add('reverse-bent-all-corners', 'reversals of worms bent in all four ways', (r) => {
  r.op('snake', 1, cell(6, 5), cell(5, 5), cell(5, 6), cell(5, 7));
  r.tap(LEFT);
  r.op('snake', 2, cell(4, 6), cell(4, 5), cell(3, 5), cell(2, 5));
  r.tap(UP);
  r.op('snake', 3, cell(3, 12), cell(4, 12), cell(4, 11), cell(4, 10));
  r.tap(RIGHT);
  r.op('snake', 0, cell(8, 4), cell(8, 5), cell(7, 5), cell(7, 6));
  r.tap(DOWN);
  r.idle(30);
});
add('reverse-keeps-timer', 'a reversal does not restart the step timer', (r) => {
  r.idle(14);
  r.tap(LEFT);
  r.idle(40);
});
add('reverse-twice', 'two reversals in a row', (r) => {
  r.idle(4);
  r.tap(LEFT);
  r.tap(RIGHT);
  r.idle(50);
});

// -- food
add('eat-food-ahead', 'eating: +10, bonus event, longer worm, new food', (r) => {
  r.op('snake', 1, ...rowWorm(4, 10, 3));
  r.op('food', cell(5, 10));
  r.tap(RIGHT);
  r.idle(60);
});
add('eat-many', 'several meals in a row on a straight line', (r) => {
  r.op('snake', 1, ...rowWorm(1, 12, 3));
  for (let x = 2; x <= 8; x++) {
    r.op('food', cell(x, 12));
    r.tap(RIGHT);
  }
  r.idle(30);
});
add('food-never-on-body', 'long worm: the next food is picked among the 59 free cells (rng.pick)', (r) => {
  const path = snakePath(14); // rows 0..13, the head is the last cell (0, 13)
  r.op('snake', 2, ...path.slice().reverse());
  r.op('food', cell(0, 14));
  r.tap(DOWN);
  r.idle(130);
});
add('one-free-cell', 'eating with a single free cell left: the food goes there without consuming the generator', (r) => {
  const q = snakePath(20); // q[0] .. q[199]
  r.op('snake', 3, ...q.slice(1, 199)); // head q[1] .. tail q[198]: 198 cells, heading to q[0] (left)
  r.op('food', q[0]!);
  r.tap(LEFT);
  r.idle(20);
});
add('win-after-13', 'the 13th meal wins the level: +100, clear scene, next level, new round', (r) => {
  r.op('snake', 1, ...rowWorm(4, 10, 3));
  r.op('eaten', 12);
  r.op('food', cell(5, 10));
  r.tap(RIGHT);
  r.idle(130);
});
add('win-wraps-level-and-speed', 'level 10 wraps to 1 and the speed goes up', (r) => {
  r.op('snake', 1, ...rowWorm(4, 10, 3));
  r.op('eaten', 12);
  r.op('food', cell(5, 10));
  r.tap(RIGHT);
  r.idle(130);
}, { level: 10, speed: 4 });
add('win-wraps-speed-ten', 'speed 10 + 1 wraps to 1', (r) => {
  r.op('snake', 1, ...rowWorm(4, 10, 3));
  r.op('eaten', 12);
  r.op('food', cell(5, 10));
  r.tap(RIGHT);
  r.idle(130);
}, { level: 10, speed: 10 });
add('score-and-record', 'score counts meals, the record follows it', (r) => {
  r.op('snake', 1, ...rowWorm(1, 12, 3));
  for (let x = 2; x <= 5; x++) {
    r.op('food', cell(x, 12));
    r.tap(RIGHT);
  }
  r.idle(10);
}, { hiScore: 25 });

// -- collisions and lives
for (const [name, worm, dir, key] of [
  ['hit-wall-top', [cell(5, 0), cell(5, 1), cell(5, 2)], 0, UP],
  ['hit-wall-bottom', [cell(5, 19), cell(5, 18), cell(5, 17)], 2, DOWN],
  ['hit-wall-left', [cell(0, 9), cell(1, 9), cell(2, 9)], 3, LEFT],
  ['hit-wall-right', [cell(9, 9), cell(8, 9), cell(7, 9)], 1, RIGHT],
] as [string, number[], number, BrickAction][]) {
  add(name, 'leaving the field costs a life; the clear scene runs; the round restarts', (r) => {
    r.op('snake', dir, ...worm);
    r.tap(key);
    r.idle(80);
  });
}
add('hit-self', 'running into the own body costs a life', (r) => {
  r.op('snake', 1, cell(4, 5), cell(4, 6), cell(5, 6), cell(6, 6), cell(6, 5), cell(6, 4), cell(5, 4), cell(4, 4));
  r.tap(RIGHT);
  r.idle(80);
});
add('chase-the-tail', 'the cell the tail leaves is free: moving into it is legal when not eating', (r) => {
  r.op('snake', 2, cell(4, 5), cell(5, 5), cell(5, 4), cell(4, 4));
  r.tap(DOWN);
  r.idle(10);
  r.op('snake', 3, cell(4, 5), cell(5, 5), cell(5, 6), cell(4, 6));
  r.tap(LEFT);
  r.idle(20);
});
add('lose-all-lives', 'four losses end the game: flood animation, then the over phase; keys are ignored meanwhile', (r) => {
  for (let i = 0; i < 4; i++) {
    r.op('snake', 0, cell(5, 0), cell(5, 1), cell(5, 2));
    r.tap(UP);
    r.tap(UP);
    r.until(() => !isBusy(r.lab), 200);
  }
  r.idle(30);
  r.tap(UP);
  r.idle(260);
});
add('lives-indicator', 'the NEXT preview shows one pair of cells per life', (r) => {
  for (const n of [4, 3, 2, 1]) {
    r.op('lives', n);
    r.idle(1);
  }
  r.idle(3);
});
add('start-after-over', 'a new party after game over resets score, lives and field', (r) => {
  r.op('lives', 1);
  r.op('snake', 0, cell(5, 0), cell(5, 1), cell(5, 2));
  r.tap(UP);
  r.idle(260);
  r.cmd(CMD.start);
  r.idle(90);
}, { hiScore: 40 });
add('restart-midgame', 'restart returns to READY with a fresh field', (r) => {
  r.idle(30);
  r.cmd(CMD.restart);
  r.idle(3);
  r.cmd(CMD.start);
  r.idle(50);
});
add('pause-resume', 'paused ticks do not advance the game', (r) => {
  r.idle(20);
  r.cmd(CMD.pause);
  r.idle(40);
  r.cmd(CMD.resume);
  r.idle(40);
});
add('pause-during-clear-scene', 'pausing while the clear scene runs', (r) => {
  r.op('snake', 0, cell(5, 0), cell(5, 1), cell(5, 2));
  r.tap(UP);
  r.idle(10);
  r.cmd(CMD.pause);
  r.idle(10);
  r.cmd(CMD.resume);
  r.idle(70);
});

// -- variant 02 "Frame": a one-cell frame round the field, 8 x 18 playing area
add('frame-start', 'variant 02: the worm starts on row 18, the frame is lit', (r) => {
  r.idle(30);
}, { mode: 1 });
add('frame-hit-wall', 'the frame is a wall', (r) => {
  r.idle(400);
}, { mode: 1, speed: 6 });
add('frame-eat-and-win', 'meals and a win inside the frame', (r) => {
  r.op('snake', 1, ...rowWorm(3, 10, 3));
  for (let x = 4; x <= 8; x++) {
    r.op('food', cell(x, 10));
    r.tap(RIGHT);
  }
  r.op('eaten', 12);
  r.op('snake', 1, ...rowWorm(3, 5, 3));
  r.op('food', cell(4, 5));
  r.tap(RIGHT);
  r.idle(130);
}, { mode: 1 });
add('frame-food-spawn', 'food never lands on the frame', (r) => {
  r.op('snake', 1, ...rowWorm(3, 10, 3));
  for (let i = 0; i < 20; i++) {
    r.op('food', cell(4, 10));
    r.tap(RIGHT);
    r.op('snake', 1, ...rowWorm(3, 10, 3));
  }
}, { mode: 1, seed: 99 });
add('frame-game-over', 'losing every life inside the frame', (r) => {
  r.idle(2400);
}, { mode: 1, speed: 10 });

// -- options
add('start-speed-and-level', 'options set the start speed and level', (r) => {
  r.idle(300);
}, { speed: 7, level: 3, seed: 21 });
add('seeded-food', 'different seeds, different food', (r) => {
  r.op('snake', 1, ...rowWorm(1, 12, 3));
  for (let x = 2; x <= 6; x++) {
    r.op('food', cell(x, 12));
    r.tap(RIGHT);
  }
  r.idle(5);
}, { seed: 987654321 });

// -- a greedy player that actually eats: growth, long bodies, many meals and wins
function botRun(r: Recorder, seed: number, ticks: number): void {
  let state = seed >>> 0;
  const rnd = (): number => {
    state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
    return state / 4294967296;
  };
  const dirs: [BrickAction, number, number][] = [[UP, 0, -1], [RIGHT, 1, 0], [DOWN, 0, 1], [LEFT, -1, 0]];
  const s = internals(r.lab) as SnakeInternals & { walls: Set<number> };
  let wait = 0;
  for (let i = 0; i < ticks; i++) {
    const phase = r.lab.phase;
    if (phase === 'over') {
      r.idle(30);
      r.cmd(CMD.start);
      i += 30;
      continue;
    }
    if (phase !== 'playing' || (r.lab as unknown as { busy: boolean }).busy) {
      r.idle(1);
      continue;
    }
    if (wait-- > 0) {
      r.idle(1);
      continue;
    }
    wait = Math.floor(rnd() * 5);
    const head = s.body[0]!;
    const hx = head % 10;
    const hy = Math.floor(head / 10);
    const fx = s.food % 10;
    const fy = Math.floor(s.food / 10);
    const tail = s.body[s.body.length - 1]!;
    const options = dirs
      .map(([key, dx, dy], d) => ({ key, d, x: hx + dx, y: hy + dy }))
      .filter((o) => o.x >= 0 && o.x < 10 && o.y >= 0 && o.y < 20 && !s.walls.has(o.y * 10 + o.x))
      .filter((o) => o.d !== (s.directionNow + 2) % 4)
      .filter((o) => {
        const to = o.y * 10 + o.x;
        return to === s.food || to === tail || !s.body.includes(to);
      })
      .sort((a, b) => Math.abs(a.x - fx) + Math.abs(a.y - fy) - (Math.abs(b.x - fx) + Math.abs(b.y - fy)) || (rnd() < 0.5 ? -1 : 1));
    if (options.length === 0) {
      r.idle(1);
      continue;
    }
    r.tap(options[0]!.key);
    i += 1;
  }
}
add('bot-normal', 'a greedy player eats and wins levels (variant 01)', (r) => botRun(r, 7, 7000), { seed: 11, speed: 5 });
add('bot-frame', 'the same player inside the frame (variant 02)', (r) => botRun(r, 8, 7000), { seed: 12, speed: 8, mode: 1 });
add('bot-fast-long', 'the greedy player at speed 10, many levels', (r) => botRun(r, 9, 9000), { seed: 13, speed: 10, level: 8 });

// -- long random sessions (the reference "fuzzPlayer" idea): keys toggled at random, pauses, restarts after game over
function fuzzRun(r: Recorder, seed: number, ticks: number, bias: number): void {
  let state = seed >>> 0;
  const rnd = (): number => {
    state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
    return state / 4294967296;
  };
  const keys: BrickAction[] = ['left', 'right', 'up', 'down', 'rotate', 'fire'];
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
    for (const k of keys) {
      const p = k === 'fire' ? bias * 0.1 : bias;
      if (rnd() < p) {
        if (held.has(k)) held.delete(k);
        else {
          held.add(k);
          pressed.push(k);
        }
      }
    }
    r.input(makeBrickInput(pressed, held));
  }
}
for (let i = 0; i < 16; i++) {
  const seed = 1000 + i * 7919;
  add(`fuzz-${i}`, `random keys, pauses and restarts (seed ${seed}, variant ${i % 4 >= 2 ? 2 : 1})`, (r) => fuzzRun(r, seed, 3000, 0.04 + 0.02 * (i % 4)), {
    seed,
    speed: 1 + ((i * 3) % 10),
    level: 1 + ((i * 5) % 10),
    mode: i % 4 >= 2 ? 1 : 0,
  });
}

// ---------------------------------------------------------------- static sections

function rngSection(): unknown[] {
  const out: unknown[] = [];
  for (const seed of [0, 1, 5, 12345, 0xdeadbeef]) {
    const rng = new Rng(seed);
    const ops: unknown[] = [];
    // the call pattern of the game: pick among N free cells (int of N), for N from 197 down to 1
    for (const n of [197, 150, 99, 40, 12, 3, 2, 1, 1, 197]) ops.push(['int', n, rng.int(n)]);
    out.push({ seed, ops });
  }
  return out;
}

/** The step period of every speed, as the exact IEEE double the reference computes. */
function periodsSection(): number[] {
  const out: number[] = [];
  for (let speed = 1; speed <= 10; speed++) out.push(e88SnakeTicks(speed));
  return out;
}

// ---------------------------------------------------------------- main

runExporter(
  {
    game: 'snake',
    files: ['src/brick/base.ts', 'src/brick/e88.ts', 'src/brick/games/library/arcade.ts', 'src/brick/games/library/common.ts', 'src/core/rng.ts'],
    sections: [
      ['rng', rngSection()],
      ['periods', periodsSection()],
    ],
    defaultOut: '../../tests/fixtures/snake/snake.conformance.json',
  },
  scenarios,
  base,
  makeRecorder,
);
