/*
 * Exports golden conformance fixtures for the C port of Racing from the TypeScript reference
 * (grayhex/ApolloBrickGames `racing`, the RacingGame of the donor n0Live/BrickGame, MIT). One variant is ported (ADR-036):
 * two lanes on the normal field. The reference is executed, never re-implemented.
 *
 * Fidelity: the rules are a reconstruction from the donor, NOT checked against the ROM of a real device.
 *
 * Output: tests/fixtures/racing/racing.conformance.json (format "brick-conformance/1", see docs/CONFORMANCE.md).
 *
 * Usage (from tools/conformance):
 *   APOLLO_BRICK_GAMES=/path/to/ApolloBrickGames npm run export:racing -- [--out FILE] [--dump NAME [--tick N]]
 */
import { RacingGame } from '@apollo/brick/games/racing';
import { halfStepTicks } from '@apollo/brick/games/racing/road';
import { Rng } from '@apollo/brick/rng';
import { makeBrickInput, type BrickAction } from '@apollo/brick/types';

import { CMD, Recorder as BaseRecorder, runExporter, type Op, type Options, type Scenario } from './lib';

// ---------------------------------------------------------------- setup operations (test only)

interface Car {
  x: number;
  y: number;
}
/** The reference keeps these members private or protected; the scenarios reach in the way its own tests do. */
interface RacingInternals {
  lane: number;
  carX: number;
  sinceStep: number;
  oddMove: boolean;
  road: { opponents: Car[]; borderPhase: number; tailY: number | null };
  layout: { lanes: number[] };
  setScore(value: number): void;
  setLives(count: number): void;
}
const internals = (g: RacingGame): RacingInternals => g as unknown as RacingInternals;

/** `tail` with this value means "no group on the road" (null in the reference). */
const NO_TAIL = -1000;

function applyOp(g: RacingGame, op: Op): void {
  const s = internals(g);
  switch (op[0]) {
    case 'poke': s.setScore(op[1] as number); break; // the score setter of the game: 50 points speed the road up
    case 'lives': s.setLives(op[1] as number); break;
    case 'opps': { // ['opps', x, y, x, y, ...]
      const cars: Car[] = [];
      for (let i = 1; i + 1 < op.length; i += 2) cars.push({ x: op[i] as number, y: op[i + 1] as number });
      s.road.opponents = cars;
      break;
    }
    case 'tail': s.road.tailY = (op[1] as number) === NO_TAIL ? null : (op[1] as number); break;
    case 'lane': s.lane = op[1] as number; s.carX = s.layout.lanes[s.lane] as number; break;
    case 'since': s.sinceStep = op[1] as number; break;
    case 'odd': s.oddMove = op[1] === 1; break;
    case 'border': s.road.borderPhase = op[1] as number; break;
    default: throw new Error(`unknown op ${String(op[0])}`);
  }
}

const makeLab = (o: Options): RacingGame => new RacingGame({ seed: o.seed, speed: o.speed, level: o.level, hiScore: o.hiScore, mode: 0 });
type Recorder = BaseRecorder<RacingGame>;
const makeRecorder = (s: Scenario<RacingGame>, options: Options): Recorder => new BaseRecorder<RacingGame>(s.name, s.description, options, makeLab, applyOp);

const LEFT: BrickAction = 'left';
const RIGHT: BrickAction = 'right';
const UP: BrickAction = 'up';
const DOWN: BrickAction = 'down';
const ROTATE: BrickAction = 'rotate';
const FIRE: BrickAction = 'fire';
const isBusy = (g: RacingGame): boolean => (g as unknown as { busy: boolean }).busy;

// Geometry: lane 0 is x = 3, lane 1 (the start lane) is x = 6; the player's car is centred at y = 1 and covers rows 0..3.
const L0 = 3;
const L1 = 6;
const INTRO = 90; // the start melody before every round: no input, nothing moves

// ---------------------------------------------------------------- scenarios

const base: Options = { seed: 5, speed: 1, level: 1, hiScore: 0 };
const scenarios: Scenario<RacingGame>[] = [];
const add = (name: string, description: string, body: (r: Recorder) => void, options: Partial<Options> = {}): void => {
  scenarios.push({ name, description, options, body });
};
/** An empty road that stays empty (the last group is far away), after the intro. */
const clearRoad = (r: Recorder): void => {
  r.idle(INTRO);
  r.op('opps');
  r.op('tail', 60);
};

// -- the road on its own: spawn, scroll, crash, kaboom, clear scene, new round, game over, per speed
for (const speed of [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]) {
  add(`idle-speed-${speed}`, `no input at speed ${speed}: traffic, crashes, explosions, clear scenes, new rounds`, (r) => {
    r.idle(speed <= 2 ? 2400 : speed <= 5 ? 1400 : 900);
  }, { speed });
}
for (const level of [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]) {
  add(`idle-level-${level}`, `no input on level ${level}: the gap between groups and the road speed depend on the level`, (r) => {
    r.idle(900);
  }, { level, speed: 5 });
}

// -- the intro
add('intro-ignores-input', 'during the 90 ticks of the start melody keys do nothing and the road stands still', (r) => {
  r.tap(LEFT);
  r.tap(RIGHT);
  r.hold(UP, 20);
  r.idle(INTRO);
  r.idle(30);
});

// -- steering
add('steer-left-right', 'a lane change moves the car and plays the move sound', (r) => {
  clearRoad(r);
  r.tap(LEFT);
  r.idle(4);
  r.tap(RIGHT);
  r.idle(4);
});
add('steer-at-the-edge', 'LEFT in the left lane and RIGHT in the right lane do nothing', (r) => {
  clearRoad(r);
  r.tap(RIGHT);
  r.tap(RIGHT);
  r.tap(LEFT);
  r.tap(LEFT);
  r.tap(LEFT);
  r.idle(4);
});
add('steer-hold-repeats', 'a held lane key repeats after 5 ticks and then every 5', (r) => {
  clearRoad(r);
  r.hold(LEFT, 30);
  r.hold(RIGHT, 30);
  r.idle(4);
});
add('steer-both-keys', 'LEFT and RIGHT together: the left one is handled first', (r) => {
  clearRoad(r);
  r.press(LEFT, RIGHT);
  r.idle(8);
  r.press(LEFT, RIGHT);
  r.idle(8);
});
add('steer-into-a-car', 'changing into a lane taken by a car is a crash', (r) => {
  clearRoad(r);
  r.op('opps', L0, 2);
  r.tap(LEFT);
  r.idle(300);
});
add('steer-past-a-far-car', 'a car further up does not matter', (r) => {
  clearRoad(r);
  r.op('opps', L0, 12);
  r.tap(LEFT);
  r.idle(20);
});
add('steer-next-to-a-car', 'a car in the other lane just behind the nose is still a crash, one further is not', (r) => {
  clearRoad(r);
  r.op('opps', L0, 6);
  r.tap(LEFT);
  r.idle(300);
});
add('steer-near-the-wall-clamp', 'the explosion of a lane-change crash sits on row 18 and is moved inside the field', (r) => {
  clearRoad(r);
  r.op('opps', L0, 1);
  r.tap(LEFT);
  r.idle(300);
});

// -- acceleration
add('accelerate-up', 'UP makes extra road steps', (r) => {
  clearRoad(r);
  r.op('opps', L1, 14);
  r.tap(UP);
  r.idle(6);
  r.tap(UP);
  r.idle(6);
});
add('accelerate-rotate', 'ROTATE does the same', (r) => {
  clearRoad(r);
  r.op('opps', L1, 14);
  r.tap(ROTATE);
  r.idle(6);
  r.tap(ROTATE);
  r.idle(6);
});
add('accelerate-fire', 'FIRE counts as ROTATE', (r) => {
  clearRoad(r);
  r.op('opps', L1, 14);
  r.tap(FIRE);
  r.idle(6);
});
add('accelerate-hold', 'a held acceleration key repeats after 2 ticks and then every 2', (r) => {
  clearRoad(r);
  r.op('opps', L0, 19);
  r.hold(UP, 40);
  r.idle(10);
});
add('accelerate-both', 'UP and ROTATE together make one extra step', (r) => {
  clearRoad(r);
  r.op('opps', L0, 19);
  r.press(UP, ROTATE);
  r.idle(6);
});
add('accelerate-down-is-nothing', 'DOWN does nothing on the normal field', (r) => {
  clearRoad(r);
  r.op('opps', L0, 14);
  r.hold(DOWN, 20);
  r.idle(10);
});
add('accelerate-into-a-car', 'accelerating onto a car in the car\'s lane is a crash', (r) => {
  clearRoad(r);
  r.op('opps', L1, 9);
  r.hold(UP, 60);
  r.idle(300);
});
add('accelerate-parity', 'the first extra step moves only the border, the second the cars too', (r) => {
  clearRoad(r);
  r.op('opps', L0, 19);
  r.op('odd', 0);
  r.tap(UP);
  r.idle(3);
  r.tap(UP);
  r.idle(3);
  r.tap(UP);
  r.idle(3);
});

// -- scoring, speed, level, extra lives
add('score-a-car-passes', 'a car that leaves the field below is one point', (r) => {
  clearRoad(r);
  r.op('opps', L0, -1);
  r.op('odd', 0);
  r.hold(UP, 12);
  r.idle(30);
});
add('speed-up-at-50', 'crossing a multiple of 50 points raises the speed and plays the level-up sound', (r) => {
  clearRoad(r);
  r.op('poke', 49);
  r.op('opps', L0, -1);
  r.hold(UP, 12);
  r.idle(10);
}, { speed: 4 });
add('speed-wraps-and-level-rises', 'speed 10 + 50 points: speed 1 and the next level', (r) => {
  clearRoad(r);
  r.op('poke', 49);
  r.op('opps', L0, -1);
  r.hold(UP, 12);
  r.idle(10);
}, { speed: 10, level: 3 });
add('level-wraps', 'level 10 at speed 10: back to level 1', (r) => {
  clearRoad(r);
  r.op('poke', 99);
  r.op('opps', L0, -1);
  r.hold(UP, 12);
  r.idle(10);
}, { speed: 10, level: 10 });
add('extra-life', 'every 2000 points one more life (below four)', (r) => {
  clearRoad(r);
  r.op('lives', 2);
  r.op('poke', 1999);
  r.op('opps', L0, -1);
  r.hold(UP, 12);
  r.idle(10);
});
add('extra-life-when-full', 'with four lives the bonus is 200 points instead', (r) => {
  clearRoad(r);
  r.op('poke', 1999);
  r.op('opps', L0, -1);
  r.hold(UP, 12);
  r.idle(10);
});
add('extra-life-and-speed-up', 'both at once: 1999 -> 2000 crosses 50 and 2000', (r) => {
  clearRoad(r);
  r.op('lives', 3);
  r.op('poke', 1999);
  r.op('opps', L0, -1);
  r.hold(UP, 12);
  r.idle(10);
}, { speed: 9 });
add('poke-crossing-several', 'a big jump of the score crosses several multiples at once', (r) => {
  clearRoad(r);
  r.op('lives', 1);
  r.op('poke', 4100);
  r.idle(5);
});
add('score-record', 'the record follows the score', (r) => {
  clearRoad(r);
  r.op('poke', 30);
  r.idle(3);
  r.op('poke', 10);
  r.idle(3);
}, { hiScore: 25 });

// -- crashes and lives
add('crash-from-ahead', 'a car driving into the player: explosion, a life lost, a clear scene, a new round', (r) => {
  clearRoad(r);
  r.op('opps', L1, 6);
  r.idle(400);
});
add('crash-explosion-pause', 'a real pause stops the explosion', (r) => {
  clearRoad(r);
  r.op('opps', L1, 5);
  r.idle(40);
  r.cmd(CMD.pause);
  r.idle(40);
  r.cmd(CMD.resume);
  r.idle(300);
});
add('crash-input-ignored', 'keys do nothing during the explosion and the clear scene', (r) => {
  clearRoad(r);
  r.op('opps', L1, 3); // hit on the first road step
  r.idle(8);
  r.hold(LEFT, 30);
  r.hold(UP, 30);
  r.idle(300);
});
add('lose-all-lives', 'the last life: explosion, flood animation, game over', (r) => {
  clearRoad(r);
  r.op('lives', 1);
  r.op('opps', L1, 5);
  r.idle(500);
});
add('start-after-over', 'a new party after game over resets score, lives and the road', (r) => {
  clearRoad(r);
  r.op('lives', 1);
  r.op('poke', 120);
  r.op('opps', L1, 5);
  r.idle(500);
  r.cmd(CMD.start);
  r.idle(200);
}, { speed: 3, level: 2 });
add('restart-midgame', 'restart returns to READY with a fresh road', (r) => {
  r.idle(120);
  r.cmd(CMD.restart);
  r.idle(3);
  r.cmd(CMD.start);
  r.idle(150);
});
add('pause-in-the-intro', 'a pause during the start melody', (r) => {
  r.idle(30);
  r.cmd(CMD.pause);
  r.idle(50);
  r.cmd(CMD.resume);
  r.idle(100);
});
add('pause-and-play', 'pause and resume on a moving road', (r) => {
  r.idle(INTRO + 40);
  r.cmd(CMD.pause);
  r.idle(60);
  r.cmd(CMD.resume);
  r.idle(60);
});
add('options-start-speed-and-level', 'the start speed and level shape the first gap between groups', (r) => {
  r.idle(300);
}, { speed: 7, level: 6 });
add('seeded-traffic', 'different seeds, different traffic', (r) => {
  r.idle(500);
}, { seed: 987654 });

/** Scores just below a speed step (50) and an extra life (2000). */
const JUMPS = [48, 49, 98, 99, 1998, 1999, 2047, 2049];

// -- players that look at the road
function botRun(r: Recorder, seed: number, ticks: number, bias: number): void {
  let state = seed >>> 0;
  const rnd = (): number => {
    state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
    return state / 4294967296;
  };
  const s = internals(r.lab);
  const taken = (laneX: number): boolean => s.road.opponents.some((c) => Math.abs(c.x - laneX) <= 2 && c.y >= -2 && c.y <= 9);
  while (r.digests.length < ticks) {
    const g = r.lab;
    if (g.phase === 'over') {
      r.idle(30);
      r.cmd(CMD.start);
      continue;
    }
    if (g.phase !== 'playing' || isBusy(g)) {
      r.idle(1);
      continue;
    }
    const here = s.carX;
    const other = here === L0 ? L1 : L0;
    if (taken(here) && !taken(other) && rnd() < bias) {
      r.tap(here === L0 ? RIGHT : LEFT);
    } else if (!taken(here) && rnd() < 0.15 * bias) {
      r.tap(rnd() < 0.5 ? UP : ROTATE);
    } else {
      r.idle(1);
    }
    // sometimes jump near a threshold: a speed step, a level step, an extra life
    if (rnd() < 0.002) {
      r.op('poke', JUMPS[Math.floor(rnd() * JUMPS.length)] as number);
      r.idle(1); // the events of the jump belong to a plain step
    }
  }
}
add('bot-careful', 'a player that dodges most of the time', (r) => botRun(r, 7, 9000, 0.9), { seed: 11, speed: 3 });
add('bot-sloppy', 'a player that often misses and crashes', (r) => botRun(r, 8, 7000, 0.05), { seed: 12, speed: 6, level: 3 });
add('bot-fast-long', 'a careful player at speed 10 on a high level', (r) => botRun(r, 9, 9000, 0.95), { seed: 13, speed: 10, level: 8 });

// -- long random sessions: keys toggled at random, pauses, restarts after game over, jumps of the score
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
    if (phase === 'playing' && rnd() < 0.002) {
      r.op('poke', JUMPS[Math.floor(rnd() * JUMPS.length)] as number);
      r.idle(1); // the events of the jump belong to a plain step
      continue;
    }
    const pressed: BrickAction[] = [];
    for (const k of keys) {
      const p = k === 'fire' ? bias * 0.1 : k === 'down' ? bias * 0.2 : bias;
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
  const seed = 3000 + i * 7919;
  add(`fuzz-${i}`, `random keys, pauses, restarts and jumps of the score (seed ${seed})`, (r) => fuzzRun(r, seed, 3000, 0.03 + 0.02 * (i % 4)), {
    seed,
    speed: 1 + ((i * 3) % 10),
    level: 1 + ((i * 5) % 10),
  });
}

// ---------------------------------------------------------------- static sections

/** The call pattern of the road: int(3) for the gap, int(2) for the lane. */
function rngSection(): unknown[] {
  const out: unknown[] = [];
  for (const seed of [0, 1, 5, 12345, 0xdeadbeef]) {
    const rng = new Rng(seed);
    const ops: unknown[] = [];
    for (let i = 0; i < 6; i++) {
      ops.push(['int', 3, rng.int(3)]);
      ops.push(['int', 2, rng.int(2)]);
    }
    out.push({ seed, ops });
  }
  return out;
}

/** Ticks between road steps for every speed 1..10 (rows) and level 1..10 (columns), two lanes. */
function halfStepSection(): number[][] {
  const out: number[][] = [];
  for (let speed = 1; speed <= 10; speed++) {
    const row: number[] = [];
    for (let level = 1; level <= 10; level++) row.push(halfStepTicks(speed, level, false));
    out.push(row);
  }
  return out;
}

// ---------------------------------------------------------------- main

runExporter(
  {
    game: 'racing',
    files: [
      'src/brick/base.ts',
      'src/brick/games/racing.ts',
      'src/brick/games/racing/base.ts',
      'src/brick/games/racing/road.ts',
      'src/brick/rng.ts',
      'src/brick/timing.ts',
      'src/core/rng.ts',
    ],
    sections: [
      ['rng', rngSection()],
      ['halfStepTicks', halfStepSection()],
    ],
    defaultOut: '../../tests/fixtures/racing/racing.conformance.json',
  },
  scenarios,
  base,
  makeRecorder,
);
