// Bundles an exporter (export_<game>.ts) together with the reference sources. The reference checkout is read, never modified.
// Usage: APOLLO_BRICK_GAMES=/path/to/ApolloBrickGames node build.mjs [game ...]   (default: every game)
import { build } from 'esbuild';
import { existsSync } from 'node:fs';
import { resolve } from 'node:path';

const apollo = resolve(process.env.APOLLO_BRICK_GAMES ?? '../../../ApolloBrickGames');
if (!existsSync(resolve(apollo, 'src/brick/types.ts'))) {
  console.error(`Reference checkout not found at ${apollo}. Set APOLLO_BRICK_GAMES=/path/to/ApolloBrickGames.`);
  process.exit(1);
}
const games = process.argv.length > 2 ? process.argv.slice(2) : ['tetris', 'snake', 'racing'];
for (const game of games) {
  await build({
  entryPoints: [`export_${game}.ts`],
  outfile: `dist/export_${game}.mjs`,
  bundle: true,
  platform: 'node',
  format: 'esm',
  target: 'node20',
  alias: { '@apollo': resolve(apollo, 'src'), '@apollo-tests': resolve(apollo, 'tests') },
  define: { 'process.env.APOLLO_BRICK_GAMES': JSON.stringify(apollo) },
  logLevel: 'info',
});
}
