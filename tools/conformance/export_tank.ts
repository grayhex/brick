/*
 * Exports golden conformance fixtures for the C port of Tank from the TypeScript reference
 * (grayhex/ApolloBrickGames `tank`, the TanksGame of the donor n0Live/BrickGame, MIT). One variant is ported (ADR-036,
 * ADR-044): donor type 1, three enemies and the ready-made fields. The reference is executed, never re-implemented.
 *
 * Fidelity: the rules are a reconstruction from the donor, NOT checked against the ROM of a real device.
 *
 * Output: tests/fixtures/tank/tank.conformance.json (format "brick-conformance/1", see docs/CONFORMANCE.md).
 *
 * Usage (from tools/conformance):
 *   APOLLO_BRICK_GAMES=/path/to/ApolloBrickGames npm run export:tank -- [--out FILE] [--dump NAME [--tick N]]
 */
import { TanksGame } from '@apollo/brick/games/tanks';
import { chooseMove, distanceToObstacle, moveWeights, shotChance, sortByWeight } from '@apollo/brick/games/tanks/ai';
import {
  BOX_KIND,
  ENEMY_KIND,
  PLAYER_KIND,
  boardIndex,
  canPlace,
  hitsBoard,
  moveTank,
  newBoard,
  tankMask,
  type Bullet,
  type Dir,
  type Tank,
  type TankKind,
} from '@apollo/brick/games/tanks/geometry';
import { presetObstacles } from '@apollo/brick/games/tanks/levels';
import { Rng } from '@apollo/brick/rng';
import { genuineTicks } from '@apollo/brick/timing';
import { makeBrickInput, type BrickAction } from '@apollo/brick/types';

import { CMD, Recorder as BaseRecorder, runExporter, type Op, type Options, type Scenario } from './lib';

// ---------------------------------------------------------------- setup operations (test only)

/** The reference keeps these members private; the scenarios reach in the way its own tests do. */
interface TankInternals {
  player: Tank;
  enemies: (Tank | null)[];
  enemyBullets: (Bullet | null)[];
  playerBullets: (Bullet | null)[];
  obstacles: Uint8Array;
  killed: number;
  onBoard: number;
  dead: boolean;
  enemyIndex: number;
  aiLeft: number;
  startLeft: number;
  bulletTimer: { left: number };
  keyDelay: Record<string, number>;
  setScore(value: number): void;
  setLives(count: number): void;
}
const internals = (g: TanksGame): TankInternals => g as unknown as TankInternals;

/** An x of this value means "no tank / no bullet in this cell" (null in the reference). */
const NONE = -100;
const KEY_NAMES = ['left', 'right', 'up', 'down', 'fire'];

function applyOp(g: TanksGame, op: Op): void {
  const s = internals(g);
  const n = (i: number): number => op[i] as number;
  const tank = (i: number): Tank | null => (n(i) === NONE ? null : { x: n(i), y: n(i + 1), dir: n(i + 2) as Dir });
  switch (op[0]) {
    case 'poke': s.setScore(n(1)); break; // the score setter of the game: 500 points give an extra life
    case 'lives': s.setLives(n(1)); break;
    case 'player': s.player = tank(1) as Tank; break; // ['player', x, y, dir]
    case 'enemy': // ['enemy', i, x, y, dir]; x = NONE removes it. The number of enemies on the field follows.
      s.enemies[n(1)] = tank(2);
      s.onBoard = s.enemies.filter((e) => e !== null).length;
      break;
    case 'pbullet': s.playerBullets[n(1)] = tank(2); break; // ['pbullet', i, x, y, dir]
    case 'ebullet': s.enemyBullets[n(1)] = tank(2); break;
    case 'obst': s.obstacles[boardIndex(n(1), n(2))] = n(3); break; // ['obst', x, y, 0|1]
    case 'obstclear': s.obstacles.fill(0); break;
    case 'killed': s.killed = n(1); break;
    case 'onboard': s.onBoard = n(1); break;
    case 'dead': s.dead = n(1) === 1; break;
    case 'eidx': s.enemyIndex = n(1); break;
    case 'ai': s.aiLeft = n(1); break;
    case 'start': s.startLeft = n(1); break;
    case 'btimer': s.bulletTimer.left = n(1); break;
    case 'delay': s.keyDelay[KEY_NAMES[n(1)] as string] = n(2); break; // ['delay', key, ticks]
    default: throw new Error(`unknown op ${String(op[0])}`);
  }
}

const makeLab = (o: Options): TanksGame => new TanksGame({ seed: o.seed, speed: o.speed, level: o.level, hiScore: o.hiScore, mode: 0 });
type Recorder = BaseRecorder<TanksGame>;
const makeRecorder = (s: Scenario<TanksGame>, options: Options): Recorder => new BaseRecorder<TanksGame>(s.name, s.description, options, makeLab, applyOp);

const LEFT: BrickAction = 'left';
const RIGHT: BrickAction = 'right';
const UP: BrickAction = 'up';
const DOWN: BrickAction = 'down';
const ROTATE: BrickAction = 'rotate';
const FIRE: BrickAction = 'fire';
const isBusy = (g: TanksGame): boolean => (g as unknown as { busy: boolean }).busy;

const D_UP = 0;
const D_RIGHT = 1;
const D_DOWN = 2;
const D_LEFT = 3;
const INTRO = 90; // the start melody before every level: the player may drive, enemies and guns wait
const FAR = 100000; // "never" for the enemies' timer

// ---------------------------------------------------------------- scenarios

const base: Options = { seed: 5, speed: 1, level: 1, hiScore: 0 };
const scenarios: Scenario<TanksGame>[] = [];
const add = (name: string, description: string, body: (r: Recorder) => void, options: Partial<Options> = {}): void => {
  scenarios.push({ name, description, options, body });
};
/** Intro over, no enemy on the field and none is going to come: for scenarios about the player alone. */
const quiet = (r: Recorder): void => {
  r.op('start', 0);
  r.op('ai', FAR);
};

// -- the game on its own: enemies appear, drive, shoot, the player dies, the clear scene, game over, per speed and level
for (const speed of [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]) {
  add(`idle-speed-${speed}`, `no input at speed ${speed}: enemies come in, drive, shoot; the player is hit`, (r) => {
    r.idle(speed <= 3 ? 2600 : speed <= 6 ? 1900 : 1400);
  }, { speed, seed: 100 + speed });
}
for (const level of [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]) {
  add(`idle-level-${level}`, `no input on level ${level}: the ready-made field of the level and the chance of a newcomer's first shot`, (r) => {
    r.idle(1500);
  }, { level, speed: 6, seed: 200 + level });
}
for (let i = 0; i < 12; i++) {
  add(`seeded-${i}`, `different seeds, different spawns and moves (seed ${7000 + i * 131})`, (r) => {
    r.idle(900);
  }, { seed: 7000 + i * 131, speed: 10, level: 1 + (i % 4) });
}

// -- the intro
add('intro-driving-allowed', 'during the 90 ticks of the intro the player may drive; the gun and the enemies wait', (r) => {
  r.hold(LEFT, 12);
  r.hold(UP, 14);
  r.hold(FIRE, 12);
  r.idle(70);
  r.hold(FIRE, 30);
  r.idle(20);
});
add('intro-pause', 'a pause in the intro', (r) => {
  r.idle(30);
  r.cmd(CMD.pause);
  r.idle(40);
  r.cmd(CMD.resume);
  r.idle(100);
});

// -- the player's tank: turns, steps, auto-repeat
add('move-turn-then-step', 'a key towards another side turns the tank on the spot, towards the same side steps', (r) => {
  quiet(r);
  r.tap(RIGHT);
  r.idle(6);
  r.tap(RIGHT);
  r.idle(6);
  r.tap(DOWN);
  r.idle(6);
  r.tap(DOWN);
  r.idle(6);
  r.tap(LEFT);
  r.idle(6);
  r.tap(UP);
  r.idle(6);
});
add('move-hold-repeats', 'a held key repeats every 5 ticks: the turn first, then the steps', (r) => {
  quiet(r);
  r.hold(RIGHT, 40);
  r.hold(UP, 40);
  r.hold(LEFT, 60);
  r.idle(6);
});
add('move-fresh-press-is-immediate', 'a new press does not wait for the pause of the previous one', (r) => {
  quiet(r);
  r.tap(RIGHT);
  r.tap(RIGHT);
  r.tap(RIGHT);
  r.tap(LEFT);
  r.tap(LEFT);
  r.idle(6);
});
add('move-keys-together', 'several direction keys at once: the first free one in the order left, right, up, down', (r) => {
  quiet(r);
  r.press(LEFT, RIGHT);
  r.idle(8);
  r.press(UP, DOWN);
  r.idle(8);
  r.press(RIGHT, DOWN);
  r.idle(8);
  r.press(LEFT, RIGHT, UP, DOWN);
  r.idle(20);
});
add('move-into-the-edges', 'the tank stops at the edges of the field', (r) => {
  quiet(r);
  r.hold(LEFT, 70);
  r.hold(DOWN, 70);
  r.hold(RIGHT, 100);
  r.hold(UP, 120);
  r.idle(6);
});
add('move-into-an-obstacle', 'a step into an obstacle is refused, a turn next to it is not', (r) => {
  quiet(r);
  for (const y of [7, 8, 9]) r.op('obst', 6, y, 1);
  r.hold(RIGHT, 30);
  r.hold(UP, 20);
  r.hold(RIGHT, 20);
  r.idle(6);
});
add('move-turn-with-a-step', 'a turn that would touch an obstacle is made with a step forward', (r) => {
  quiet(r);
  r.op('player', 4, 7, D_UP);
  r.op('obst', 3, 8, 1); // a cell only the tank turned to the right occupies
  r.tap(RIGHT);
  r.idle(8);
  r.op('obstclear');
  r.op('player', 4, 7, D_UP);
  r.op('obst', 5, 8, 1); // a cell only the tank turned to the left occupies
  r.tap(LEFT);
  r.idle(8);
  r.op('obstclear');
  r.op('player', 1, 7, D_UP); // at the left edge: a turn that needs a step out of the field is refused
  r.op('obst', 2, 8, 1);
  r.tap(LEFT);
  r.idle(8);
});
add('move-blocked-by-an-enemy', 'a tank does not drive into another tank', (r) => {
  quiet(r);
  r.op('enemy', 0, 4, 11, D_DOWN);
  r.hold(UP, 40);
  r.idle(6);
});
add('move-blocked-by-own-bullet', "the player's own bullet in front blocks the step", (r) => {
  quiet(r);
  r.op('pbullet', 0, 4, 9, D_UP);
  r.op('btimer', 1000);
  r.hold(UP, 12);
  r.idle(6);
});

// -- the gun
add('fire-single', 'a shot: the bullet appears on the cell in front of the barrel and at once makes its first step', (r) => {
  quiet(r);
  r.tap(FIRE);
  r.idle(160);
});
add('fire-rotate-is-fire', 'the big action key (ROTATE) fires too', (r) => {
  quiet(r);
  r.tap(ROTATE);
  r.idle(40);
  r.press(ROTATE, FIRE);
  r.idle(40);
});
add('fire-hold-autofire', 'a held gun fires no more than once per 14 ticks and at most four bullets fly', (r) => {
  quiet(r);
  r.hold(FIRE, 220);
  r.idle(200);
});
add('fire-while-driving', 'driving does not delay the gun and the gun does not delay driving', (r) => {
  quiet(r);
  r.press(RIGHT, FIRE);
  for (let i = 0; i < 40; i++) r.tick(2 | 16);
  r.idle(10);
  r.hold(UP, 20);
  r.idle(100);
});
add('fire-in-every-direction', 'bullets fly in the direction of the barrel', (r) => {
  quiet(r);
  r.op('player', 4, 9, D_UP);
  r.tap(FIRE);
  r.idle(20);
  r.tap(RIGHT);
  r.tap(FIRE);
  r.idle(20);
  r.tap(DOWN);
  r.tap(FIRE);
  r.idle(20);
  r.tap(LEFT);
  r.tap(FIRE);
  r.idle(150);
});
add('bullet-erases-an-obstacle-cell', 'a bullet that hits an obstacle disappears and erases the cell', (r) => {
  quiet(r);
  r.op('obst', 4, 12, 1);
  r.op('obst', 4, 13, 1);
  r.tap(FIRE);
  r.idle(80);
  r.tap(FIRE);
  r.idle(80);
  r.tap(FIRE);
  r.idle(120);
});
add('bullet-destroys-an-enemy', 'a bullet in an enemy tank destroys it: a point, a kill and the explode sound', (r) => {
  quiet(r);
  r.op('enemy', 0, 4, 14, D_DOWN);
  r.tap(FIRE);
  r.idle(100);
});
add('bullet-hits-every-cell-of-an-enemy', 'the hit is tested on the cells of the drawing, not the 3 x 3 square', (r) => {
  quiet(r);
  r.op('enemy', 0, 4, 14, D_UP); // the cell behind the centre is empty: (4, 13) lets the bullet through
  r.tap(FIRE);
  r.idle(80);
  r.op('enemy', 0, 4, 14, D_UP);
  r.op('player', 4, 7, D_UP);
  r.tap(FIRE);
  r.idle(80);
});
add('bullet-meets-an-enemy-bullet', 'a player bullet and an enemy bullet meeting remove each other', (r) => {
  quiet(r);
  r.op('ebullet', 0, 4, 15, D_DOWN);
  r.op('btimer', 3);
  r.tap(FIRE);
  r.idle(120);
});
add('enemy-bullet-meets-player-bullet', "an enemy bullet stepping into a player's bullet removes both", (r) => {
  quiet(r);
  r.op('pbullet', 0, 4, 12, D_UP);
  r.op('ebullet', 0, 4, 13, D_DOWN);
  r.op('btimer', 1);
  r.idle(20);
  r.op('pbullet', 1, 5, 12, D_UP);
  r.op('ebullet', 1, 5, 14, D_DOWN);
  r.op('btimer', 1);
  r.idle(40);
});
add('enemy-bullet-meets-enemy-bullet', 'two enemy bullets meet and both vanish', (r) => {
  quiet(r);
  r.op('ebullet', 0, 2, 15, D_RIGHT);
  r.op('ebullet', 1, 4, 15, D_LEFT);
  r.op('btimer', 1);
  r.idle(30);
});
add('enemy-bullet-in-an-enemy', 'an enemy bullet in an enemy tank just vanishes', (r) => {
  quiet(r);
  r.op('enemy', 0, 4, 14, D_DOWN);
  r.op('ebullet', 0, 4, 18, D_DOWN);
  r.op('btimer', 1);
  r.idle(30);
});
add('player-bullet-in-own-tank', "a player's bullet that meets the player's tank vanishes", (r) => {
  quiet(r);
  r.op('pbullet', 0, 4, 5, D_UP);
  r.op('btimer', 1);
  r.idle(20);
});
add('player-is-hit', 'an enemy bullet in the player: a crash, the explosion, a life lost, a clear scene, the same level again', (r) => {
  quiet(r);
  r.op('ebullet', 0, 4, 11, D_DOWN);
  r.op('btimer', 1);
  r.idle(400);
});
add('player-is-hit-in-the-intro', 'the bullets stand still in the intro, so a hit comes after it', (r) => {
  r.op('ebullet', 0, 4, 11, D_DOWN);
  r.op('ai', FAR);
  r.idle(120);
  r.idle(400);
});
add('hit-order-enemy-bullets-first', 'in one step the enemy bullets move before the player ones; after a hit on the player the others wait', (r) => {
  quiet(r);
  r.op('ebullet', 0, 4, 11, D_DOWN);
  r.op('pbullet', 0, 9, 3, D_UP);
  r.op('pbullet', 1, 8, 3, D_UP);
  r.op('btimer', 1);
  r.idle(8);
});
add('explosion-pause', 'a real pause stops the explosion', (r) => {
  quiet(r);
  r.op('ebullet', 0, 4, 11, D_DOWN);
  r.op('btimer', 1);
  r.idle(30);
  r.cmd(CMD.pause);
  r.idle(40);
  r.cmd(CMD.resume);
  r.idle(300);
});
add('explosion-input-ignored', 'keys do nothing during the explosion and the clear scene', (r) => {
  quiet(r);
  r.op('ebullet', 0, 4, 11, D_DOWN);
  r.op('btimer', 1);
  r.idle(4);
  r.hold(LEFT, 30);
  r.hold(FIRE, 30);
  r.idle(300);
});
add('explosion-at-the-edge', 'the explosion centre is moved inside the field', (r) => {
  quiet(r);
  r.op('player', 1, 1, D_UP);
  r.op('ebullet', 0, 1, 5, D_DOWN);
  r.op('btimer', 1);
  r.idle(400);
  quiet(r);
  r.op('player', 8, 18, D_UP);
  r.op('ebullet', 1, 8, 19, D_DOWN);
  r.op('btimer', 1);
  r.idle(400);
});

// -- the enemies
add('ai-spawn-and-move', 'enemies come in one by one at the six places and drive towards the player', (r) => {
  r.idle(INTRO);
  r.idle(1200);
}, { speed: 10, seed: 41 });
add('ai-spawn-cap', 'killed + standing enemies never exceed 25', (r) => {
  r.op('start', 0);
  r.op('killed', 23);
  r.idle(900);
  r.op('killed', 22);
  r.op('onboard', 2);
  r.idle(900);
}, { speed: 10, seed: 42 });
add('ai-shoots-along-the-line', 'an enemy in line with the player shoots; a friend in the way lowers the chance', (r) => {
  r.op('start', 0);
  r.op('ai', 1);
  r.op('player', 4, 3, D_UP);
  r.op('enemy', 0, 4, 12, D_DOWN);
  r.op('enemy', 1, 4, 16, D_DOWN);
  r.idle(500);
}, { speed: 10, seed: 43 });
add('ai-point-blank', 'next to the player the chance of a shot is high', (r) => {
  r.op('start', 0);
  r.op('ai', 1);
  r.op('player', 4, 3, D_LEFT);
  r.op('enemy', 0, 6, 8, D_LEFT);
  r.op('enemy', 1, 1, 8, D_RIGHT);
  r.idle(400);
}, { speed: 10, seed: 44 });
add('ai-obstacle-on-the-line', 'a close obstacle on the line of fire raises the chance, a far one does not', (r) => {
  r.op('start', 0);
  r.op('ai', 1);
  r.op('player', 4, 3, D_UP);
  r.op('enemy', 0, 4, 14, D_DOWN);
  r.op('obst', 4, 12, 1);
  r.idle(300);
  r.op('obst', 4, 12, 0);
  r.op('obst', 4, 8, 1);
  r.idle(300);
}, { speed: 10, seed: 45 });
add('ai-boxed-in', 'an enemy that cannot move at all does not move', (r) => {
  r.op('start', 0);
  r.op('ai', 1);
  r.op('player', 4, 3, D_UP);
  r.op('enemy', 0, 1, 18, D_LEFT);
  for (const [x, y] of [[3, 16], [3, 17], [3, 18], [3, 19], [1, 16], [2, 16], [0, 16]]) r.op('obst', x, y, 1);
  r.idle(300);
}, { speed: 10, seed: 46 });
add('ai-round-robin', 'one enemy acts per move; the turn passes on in a circle and skips cells that cannot move', (r) => {
  r.op('start', 0);
  r.op('eidx', 2);
  r.op('enemy', 0, 2, 16, D_DOWN);
  r.op('enemy', 2, 7, 16, D_DOWN);
  r.idle(500);
}, { speed: 10, seed: 47 });
add('ai-newcomer-fires-at-once', 'on a low level a newcomer sometimes shoots at once (1 in 20); on level 10, 1 in 11', (r) => {
  r.idle(INTRO + 1500);
}, { speed: 10, seed: 48, level: 10 });

// -- levels, kills, win
add('win-level', '25 kills win the level: the win sound, a clear scene of 314 ticks, the next level, kills from zero', (r) => {
  quiet(r);
  r.op('killed', 24);
  r.op('enemy', 0, 4, 14, D_DOWN);
  r.tap(FIRE);
  r.idle(500);
  r.idle(100);
});
add('win-level-wraps', 'level 10 at speed 10: back to level 1 and speed 1', (r) => {
  quiet(r);
  r.op('obstclear'); // level 10 has a block on the line of fire
  r.op('killed', 24);
  r.op('enemy', 0, 4, 14, D_DOWN);
  r.tap(FIRE);
  r.idle(500);
  r.idle(200);
}, { speed: 10, level: 10, seed: 51 });
add('win-level-speed-wraps-only-at-10', 'level 10 at speed 4: level 1 and speed 5', (r) => {
  quiet(r);
  r.op('obstclear');
  r.op('killed', 24);
  r.op('enemy', 0, 4, 14, D_DOWN);
  r.tap(FIRE);
  r.idle(500);
}, { speed: 4, level: 10, seed: 52 });
add('win-pause', 'a pause in the clear scene of a won level', (r) => {
  quiet(r);
  r.op('killed', 24);
  r.op('enemy', 0, 4, 14, D_DOWN);
  r.tap(FIRE);
  r.idle(120);
  r.cmd(CMD.pause);
  r.idle(40);
  r.cmd(CMD.resume);
  r.idle(400);
});
add('reload-keeps-the-kills', 'after a lost life the same level starts again, the kills and the score are kept', (r) => {
  quiet(r);
  r.op('killed', 10);
  r.op('poke', 10);
  r.op('ebullet', 0, 4, 11, D_DOWN);
  r.op('btimer', 1);
  r.idle(300);
  r.idle(300);
}, { level: 4, seed: 53 });
add('start-level-above-one', 'a start level above the first has obstacles from the very beginning', (r) => {
  r.idle(300);
}, { level: 7, seed: 54 });

// -- score and lives
add('extra-life', 'every 500 points one more life (below four)', (r) => {
  quiet(r);
  r.op('lives', 2);
  r.op('poke', 499);
  r.op('enemy', 0, 4, 14, D_DOWN);
  r.tap(FIRE);
  r.idle(100);
});
add('extra-life-when-full', 'with four lives the bonus is 50 points instead', (r) => {
  quiet(r);
  r.op('poke', 499);
  r.op('enemy', 0, 4, 14, D_DOWN);
  r.tap(FIRE);
  r.idle(100);
});
add('poke-crossing-several', 'a big jump of the score crosses several multiples of 500 at once', (r) => {
  quiet(r);
  r.op('lives', 1);
  r.op('poke', 1700);
  r.idle(5);
});
add('score-record', 'the record follows the score', (r) => {
  quiet(r);
  r.op('poke', 30);
  r.idle(3);
  r.op('poke', 10);
  r.idle(3);
}, { hiScore: 25 });
add('lose-all-lives', 'the last life: explosion, flood animation, game over', (r) => {
  quiet(r);
  r.op('lives', 1);
  r.op('ebullet', 0, 4, 11, D_DOWN);
  r.op('btimer', 1);
  r.idle(500);
});
add('start-after-over', 'a new party after game over resets score, lives, kills and the field', (r) => {
  quiet(r);
  r.op('lives', 1);
  r.op('poke', 120);
  r.op('killed', 7);
  r.op('ebullet', 0, 4, 11, D_DOWN);
  r.op('btimer', 1);
  r.idle(500);
  r.cmd(CMD.start);
  r.idle(300);
}, { speed: 3, level: 2 });
add('dead-and-won-together', 'if the player is hit and 25 are killed in the same step, the hit is handled first', (r) => {
  quiet(r);
  r.op('killed', 25);
  r.op('dead', 1);
  r.idle(8);
  r.idle(500);
});
add('start-after-over-with-enemies', 'game over reached while the enemies play, then a new party: the enemies come in from scratch', (r) => {
  r.idle(INTRO);
  r.op('lives', 1);
  for (let i = 0; i < 4000 && r.lab.phase !== 'over'; i++) r.tick(0);
  r.idle(10);
  r.cmd(CMD.start);
  r.idle(700);
}, { speed: 10, seed: 61 });
add('restart-midgame', 'restart returns to READY with a fresh field', (r) => {
  r.idle(300);
  r.cmd(CMD.restart);
  r.idle(3);
  r.cmd(CMD.start);
  r.idle(250);
});
add('pause-and-play', 'pause and resume while enemies move', (r) => {
  r.idle(INTRO + 200);
  r.cmd(CMD.pause);
  r.idle(60);
  r.cmd(CMD.resume);
  r.idle(200);
}, { speed: 8 });
add('options-start-speed-and-level', 'the start speed shapes the pace of the enemies, the start level the field', (r) => {
  r.idle(500);
}, { speed: 9, level: 5, seed: 55 });

/** Scores just below 500 (an extra life) and 1000. */
const JUMPS = [498, 499, 999, 1000, 1499, 1950];

// -- players that look at the field and shoot
function botRun(r: Recorder, seed: number, ticks: number, aim: number): void {
  let state = seed >>> 0;
  const rnd = (): number => {
    state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
    return state / 4294967296;
  };
  const s = internals(r.lab);
  const dirKey = [UP, RIGHT, DOWN, LEFT];
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
    const p = s.player;
    let target: Tank | null = null;
    let best = 1000;
    for (const e of s.enemies) {
      if (e === null) continue;
      const d = Math.abs(e.x - p.x) + Math.abs(e.y - p.y);
      if (d < best) {
        best = d;
        target = e;
      }
    }
    if (target === null || rnd() > aim) {
      r.tap(dirKey[Math.floor(rnd() * 4)] as BrickAction);
      continue;
    }
    const dx = target.x - p.x;
    const dy = target.y - p.y;
    const aligned = Math.abs(dx) <= 1 || Math.abs(dy) <= 1;
    // face the enemy along the axis on which it is aligned, otherwise step to align
    let want: Dir;
    if (Math.abs(dx) <= 1) want = (dy > 0 ? D_UP : D_DOWN) as Dir;
    else if (Math.abs(dy) <= 1) want = (dx > 0 ? D_RIGHT : D_LEFT) as Dir;
    else want = (Math.abs(dx) < Math.abs(dy) ? (dx > 0 ? D_RIGHT : D_LEFT) : dy > 0 ? D_UP : D_DOWN) as Dir;
    if (p.dir !== want) r.tap(dirKey[want] as BrickAction);
    else if (aligned) {
      r.press(FIRE);
      r.idle(1);
    } else r.hold(dirKey[want] as BrickAction, 2);
    // sometimes jump near a threshold: an extra life
    if (rnd() < 0.003 && r.lab.phase === 'playing') {
      r.op('poke', JUMPS[Math.floor(rnd() * JUMPS.length)] as number);
      r.idle(1); // the events of the jump belong to a plain step
    }
  }
}
add('bot-hunter', 'a player that aims at the nearest enemy and shoots', (r) => botRun(r, 7, 10000, 0.95), { seed: 11, speed: 4 });
add('bot-sloppy', 'a player that often wanders and misses', (r) => botRun(r, 8, 8000, 0.3), { seed: 12, speed: 8, level: 3 });
add('bot-fast-long', 'a hunter at speed 10 on a high level', (r) => botRun(r, 9, 10000, 0.97), { seed: 13, speed: 10, level: 8 });
add('bot-long-run', 'a hunter that survives long enough to win levels', (r) => botRun(r, 10, 14000, 0.99), { seed: 14, speed: 2, level: 1 });

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
      const p = k === 'rotate' || k === 'fire' ? bias * 0.8 : bias;
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
  const seed = 4000 + i * 6007;
  add(`fuzz-${i}`, `random keys, pauses, restarts and jumps of the score (seed ${seed})`, (r) => fuzzRun(r, seed, 3200, 0.03 + 0.02 * (i % 4)), {
    seed,
    speed: 1 + ((i * 3) % 10),
    level: 1 + ((i * 5) % 10),
  });
}

// ---------------------------------------------------------------- static sections

/** The call pattern of the game: int(4) for the chance of a newcomer, int(6) for the place, int(20) for the first shot, next(). */
function rngSection(): unknown[] {
  const out: unknown[] = [];
  for (const seed of [0, 1, 5, 12345, 0xdeadbeef]) {
    const rng = new Rng(seed);
    const ops: unknown[] = [];
    for (let i = 0; i < 6; i++) {
      ops.push(['int', 4, rng.int(4)]);
      ops.push(['int', 6, rng.int(6)]);
      ops.push(['int', 11 + (i % 10), rng.int(11 + (i % 10))]);
      ops.push(['u32', Math.round(rng.next() * 4294967296)]);
    }
    out.push({ seed, ops });
  }
  return out;
}

/** Ticks between two enemy moves for speed 1..10 (genuineTicks(speed, 1500, 350)). */
function enemyTicksSection(): number[] {
  const out: number[] = [];
  for (let speed = 1; speed <= 10; speed++) out.push(genuineTicks(speed, 1500, 350));
  return out;
}

/** The cells of every tank shape in every direction, sorted, so that the C tables can be compared cell by cell. */
function masksSection(): unknown[] {
  const out: unknown[] = [];
  for (const kind of [PLAYER_KIND, ENEMY_KIND, BOX_KIND] as TankKind[]) {
    for (const dir of [0, 1, 2, 3] as Dir[]) {
      const cells = tankMask(kind, dir).cells.map(([x, y]) => [x, y]).sort((a, b) => (a[0] as number) - (b[0] as number) || (a[1] as number) - (b[1] as number));
      out.push({ kind, dir, cells });
    }
  }
  return out;
}

/** The ready-made obstacles of every level, row by row from y = 0 (the bottom row) up. */
function levelsSection(): unknown[] {
  const out: unknown[] = [];
  for (let level = 1; level <= 10; level++) {
    const board = presetObstacles(level);
    const rows: string[] = [];
    for (let y = 0; y < 20; y++) {
      let row = '';
      for (let x = 0; x < 10; x++) row += board[boardIndex(x, y)] ? '#' : '.';
      rows.push(row);
    }
    out.push({ level, rows });
  }
  return out;
}

const f64bits = (v: number): string => {
  const view = new DataView(new ArrayBuffer(8));
  view.setFloat64(0, v);
  return view.getBigUint64(0).toString(16).padStart(16, '0');
};

/**
 * Samples of the pure decision functions on random fields: weights and the order of the directions, the chance of a shot
 * as the bits of the IEEE double, the distance to an obstacle, and moves of tanks. The C functions must agree exactly.
 */
function samplesSection(): { ai: unknown[]; moves: unknown[]; picks: unknown[] } {
  let state = 987654321;
  const rnd = (): number => {
    state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
    return state / 4294967296;
  };
  const int = (n: number): number => Math.floor(rnd() * n);
  const boardString = (b: Uint8Array): string => [...b].join('');
  const randomBoard = (density: number): Uint8Array => {
    const b = newBoard();
    for (let i = 0; i < b.length; i++) b[i] = rnd() < density ? 1 : 0;
    return b;
  };
  const randomTank = (): Tank => ({ x: 1 + int(8), y: 1 + int(18), dir: int(4) as Dir });
  const ai: unknown[] = [];
  const moves: unknown[] = [];
  for (let i = 0; i < 400; i++) {
    const board = randomBoard(rnd() * 0.25);
    const tank = randomTank();
    const player = randomTank();
    if (tank.x === player.x && tank.y === player.y) continue;
    // the board in the game never has the tank itself: clear its cells
    for (const [cx, cy] of tankMask(ENEMY_KIND, tank.dir).cells) board[boardIndex(tank.x + cx, tank.y + cy)] = 0;
    const enemies: (Tank | null)[] = [tank, int(2) ? randomTank() : null, int(2) ? randomTank() : null];
    const weights = moveWeights(board, tank, player);
    ai.push({
      board: boardString(board),
      tank: [tank.x, tank.y, tank.dir],
      player: [player.x, player.y, player.dir],
      enemies: enemies.map((e) => (e === null ? [NONE, 0, 0] : [e.x, e.y, e.dir])),
      weights4: weights.map((w) => Math.round(w * 4)), // quarters: exact
      order: sortByWeight(weights),
      chance: f64bits(shotChance(board, tank, player, enemies)),
      distances: [0, 1, 2, 3].map((d) => distanceToObstacle(board, tank.x, tank.y, d as Dir)),
    });
  }
  for (let i = 0; i < 300; i++) {
    const board = randomBoard(rnd() * 0.2);
    const kind = [PLAYER_KIND, ENEMY_KIND][int(2)] as TankKind;
    const tank = randomTank();
    for (const [cx, cy] of tankMask(kind, tank.dir).cells) board[boardIndex(tank.x + cx, tank.y + cy)] = 0;
    const dir = int(4) as Dir;
    const rotationOnly = int(4) === 0;
    const next = moveTank(board, kind, tank, dir, rotationOnly);
    const probe = { x: int(10), y: int(20) };
    moves.push({
      board: boardString(board),
      kind,
      tank: [tank.x, tank.y, tank.dir],
      dir,
      rotationOnly,
      result: [next.x, next.y, next.dir],
      hits: hitsBoard(board, BOX_KIND, dir, probe.x, probe.y),
      place: canPlace(board, kind, dir, probe.x, probe.y),
      probe: [probe.x, probe.y],
    });
  }
  // chooseMove consumes the dice in a fixed order: a few runs with the seed that make the order visible
  const picks: unknown[] = [];
  for (let i = 0; i < 40; i++) {
    const seed = 1000 + i;
    const board = randomBoard(0.1);
    const tank = randomTank();
    const player = randomTank();
    if (tank.x === player.x && tank.y === player.y) continue;
    for (const [cx, cy] of tankMask(ENEMY_KIND, tank.dir).cells) board[boardIndex(tank.x + cx, tank.y + cy)] = 0;
    const rng = new Rng(seed);
    const dir = chooseMove(board, tank, player, rng);
    picks.push({ seed, board: boardString(board), tank: [tank.x, tank.y, tank.dir], player: [player.x, player.y, player.dir], dir, next: Math.round(rng.next() * 4294967296) });
  }
  return { ai, moves, picks };
}

// ---------------------------------------------------------------- main

const samples = samplesSection();
runExporter(
  {
    game: 'tank',
    files: [
      'src/brick/base.ts',
      'src/brick/games/tanks.ts',
      'src/brick/games/tanks/ai.ts',
      'src/brick/games/tanks/geometry.ts',
      'src/brick/games/tanks/levels.ts',
      'src/brick/rng.ts',
      'src/brick/timing.ts',
      'src/core/rng.ts',
    ],
    sections: [
      ['rng', rngSection()],
      ['enemyTicks', enemyTicksSection()],
      ['masks', masksSection()],
      ['levels', levelsSection()],
      ['aiSamples', samples.ai],
      ['moveSamples', samples.moves],
      ['pickSamples', samples.picks],
    ],
    defaultOut: '../../tests/fixtures/tank/tank.conformance.json',
  },
  scenarios,
  base,
  makeRecorder,
);
