/*
 * Shared machinery of the conformance exporters: the encoding of tick codes and events, the per-tick state digest,
 * and the Recorder that runs one scenario against the TypeScript reference while recording inputs, setup operations
 * and digests. The reference is executed, never re-implemented. Layout of fixtures: docs/CONFORMANCE.md.
 */
import { execFileSync } from 'node:child_process';
import { mkdirSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';

import { makeBrickInput, type BrickAction, type BrickEvent, type BrickInput, type BrickPhase } from '@apollo/brick/types';

// ---------------------------------------------------------------- encoding shared with the C runner

/** Key bits; identical to bk_key_t in firmware/include/brick/input.h. */
export const KEY_BITS: Record<BrickAction, number> = { left: 1, right: 2, up: 4, down: 8, rotate: 16, fire: 32 };
export const ACTIONS = Object.keys(KEY_BITS) as BrickAction[];
/** A tick code is: held keys (bits 0-5) | pressed keys << 6 | command << 12. */
export const CMD = { none: 0, start: 1, pause: 2, resume: 3, restart: 4 } as const;
export const heldBits = (code: number): number => code & 63;
export const pressedBits = (code: number): number => (code >> 6) & 63;

export const actionsOf = (bits: number): BrickAction[] => ACTIONS.filter((a) => (bits & KEY_BITS[a]) !== 0);
export const bitsOf = (actions: Iterable<BrickAction>): number => {
  let bits = 0;
  for (const a of actions) bits |= KEY_BITS[a];
  return bits;
};
export const codeOf = (input: BrickInput, cmd = 0): number => bitsOf(input.held) | (bitsOf(input.pressed) << 6) | (cmd << 12);
export const inputOf = (code: number): BrickInput => makeBrickInput(actionsOf(pressedBits(code)), actionsOf(heldBits(code)));

/** Event ids; identical to bk_event_type_t in firmware/include/brick/event.h. */
export function encodeEvent(e: BrickEvent): [number, number, number, number] {
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

export const PHASE_ID = { ready: 0, playing: 1, paused: 2, over: 3 } as const;

/** FNV-1a 32 over bytes; the digest layout is specified in docs/CONFORMANCE.md. */
export class Fnv {
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

export function digest(lab: LabLike, events: readonly BrickEvent[]): number {
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

export const hex = (v: number, width: number): string => (v >>> 0).toString(16).padStart(width, '0');
export const fold16 = (h: number): number => (h ^ (h >>> 16)) & 0xffff;


/** What the digest and the Recorder need from a game under test (the reference BrickGame, or a lab wrapping it). */
export interface LabLike {
  readonly tick: number;
  readonly phase: BrickPhase;
  readonly score: number;
  readonly hiScore: number;
  readonly speed: number;
  readonly level: number;
  readonly lives: number | null;
  readonly overFill: number;
  cells(): Uint8Array;
  preview(): Uint8Array | null;
  start(): BrickEvent[];
  pause(): BrickEvent[];
  resume(): BrickEvent[];
  restart(): BrickEvent[];
  step(input: BrickInput): BrickEvent[];
}

export type Op = (string | number)[];

export interface Options {
  seed: number;
  speed: number;
  level: number;
  hiScore: number;
}


export const CHECKPOINT_EVERY = 250;
export const rowsOf = (cells: Uint8Array, width: number): string[] => {
  const out: string[] = [];
  for (let r = 0; r < cells.length / width; r++) out.push([...cells.slice(r * width, (r + 1) * width)].map((v) => '.#*'[v]).join(''));
  return out;
};

/** Executes a scenario against the reference while recording inputs, ops and per-tick digests. */

/** Executes a scenario against the reference while recording inputs, ops and per-tick digests. */
export class Recorder<L extends LabLike> {
  readonly lab: L;
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
    makeLab: (options: Options) => L,
    private readonly applyOp: (lab: L, op: Op) => void,
  ) {
    this.lab = makeLab(options);
    this.lab.start(); // events of the very first start are not part of the contract (as plainLab in the reference)
  }

  private flushRun(): void {
    if (this.run) this.trace.push([this.run[0], this.run[1]]);
    this.run = null;
  }

  /** Test-setup operation (reference Lab method). */
  op(...op: Op): void {
    this.flushRun();
    this.applyOp(this.lab, op);
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

/** One scenario: a name, a description, the options of the game and the script that drives the Recorder. */
export interface Scenario<L extends LabLike> {
  name: string;
  description: string;
  options: Partial<Options>;
  body: (r: Recorder<L>) => void;
}

export interface FixtureSpec {
  game: string;
  /** Reference sources the scenarios depend on (documentation of the fixture). */
  files: string[];
  /** Extra sections written between the header and the scenarios, in order: name -> JSON value. */
  sections: [string, unknown][];
  defaultOut: string;
}

/**
 * Runs every scenario and writes the fixture (or, with --dump NAME [--tick N], prints the reference state per tick of one
 * scenario). The text layout is fixed so that regenerating from the same reference commit reproduces the file byte for byte.
 */
export function runExporter<L extends LabLike>(
  spec: FixtureSpec,
  scenarios: Scenario<L>[],
  base: Options,
  makeRecorder: (s: Scenario<L>, options: Options) => Recorder<L>,
): void {
  const apollo = process.env.APOLLO_BRICK_GAMES as string;
  const args = process.argv.slice(2);
  const arg = (flag: string): string | undefined => {
    const i = args.indexOf(flag);
    return i >= 0 ? args[i + 1] : undefined;
  };
  const out = resolve(arg('--out') ?? spec.defaultOut);
  const dumpName = arg('--dump');

  let commit = 'unknown';
  try {
    commit = execFileSync('git', ['-C', apollo, 'rev-parse', 'HEAD'], { encoding: 'utf8' }).trim();
  } catch {
    console.warn('warning: reference checkout is not a git repository, commit id is unknown');
  }

  const records: Record<string, unknown>[] = [];
  let totalTicks = 0;
  for (const s of scenarios) {
    const recorder = makeRecorder(s, { ...base, ...s.options });
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
  lines.push(` "game": "${spec.game}",`);
  lines.push(` "reference": ${JSON.stringify({ repo: 'grayhex/ApolloBrickGames', commit, files: spec.files })},`);
  lines.push(` "note": "Generated by tools/conformance/export_${spec.game}.ts. Do not edit by hand. Tick codes, event ids and the digest layout are specified in docs/CONFORMANCE.md.",`);
  for (const [name, value] of spec.sections) lines.push(` "${name}": ${JSON.stringify(value)},`);
  lines.push(` "scenarios": [`);
  records.forEach((rec, i) => lines.push(`  ${JSON.stringify(rec)}${i + 1 < records.length ? ',' : ''}`));
  lines.push(' ]');
  lines.push('}');
  mkdirSync(dirname(out), { recursive: true });
  writeFileSync(out, lines.join('\n') + '\n');
  console.log(`wrote ${out}: ${records.length} scenarios, ${totalTicks} ticks, reference ${commit}`);
}

