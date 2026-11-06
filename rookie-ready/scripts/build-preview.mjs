/**
 * Builds a single self-contained HTML file of the gear guide for sharing as a
 * preview (for example a claude.ai artifact): CSS and JS inlined, header and
 * footer links removed. Run after `astro build`:  npm run build:preview
 */
import { build } from 'esbuild';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const dist = join(root, 'dist');
let html = await readFile(join(dist, 'gear-guide', 'index.html'), 'utf8');

// 1. Inline stylesheets.
const links = [...html.matchAll(/<link rel="stylesheet" href="(\/_astro\/[^"]+\.css)"\s*\/?>/g)];
for (const [tag, href] of links) {
  const css = await readFile(join(dist, href), 'utf8');
  html = html.replace(tag, () => `<style>${css}</style>`);
}

// 2. Replace Astro's module scripts with one bundled, inlined script.
html = html.replace(/<script type="module"[^>]*src="[^"]+"[^>]*><\/script>\s*/g, '');
const out = await build({
  entryPoints: [join(root, 'src', 'preview', 'entry.ts')],
  bundle: true,
  format: 'iife',
  minify: true,
  target: 'es2020',
  write: false,
  define: { 'import.meta.env.SITE': '""' },
});
const js = out.outputFiles[0].text.replace(/<\/script/gi, '<\\/script');
html = html.replace('</body>', () => `<script>${js}</script>\n</body>`);

// 3. Strip navigation that can't work in a single-page preview.
html = html
  .replace(/<a class="skip-link"[^>]*>[\s\S]*?<\/a>/, '')
  .replace(/<nav class="nav"[\s\S]*?<\/nav>/, '<span class="preview-pill">Preview</span>')
  .replace(/<a class="logo" href="\/"([^>]*)>/g, '<span class="logo"$1>')
  .replace(/(<span class="logo"[\s\S]*?<\/span>\s*)<\/a>/g, '$1</span>')
  .replace(
    /<footer class="site-footer">[\s\S]*?<\/footer>/,
    '<footer class="preview-foot-note"><p>Rookie Ready preview · Shopping links go to real stores. Sizes are general guidance: always check your league’s rules.</p></footer>',
  )
  .replace(/<link rel="(canonical|sitemap|apple-touch-icon|icon)"[^>]*>\s*/g, '')
  .replace(
    '</head>',
    `<style>.preview-pill{padding:.35rem .8rem;border-radius:999px;background:var(--sun-tint);color:#8a6312;font:800 .8rem/1 var(--font-display);letter-spacing:.08em;text-transform:uppercase}
.preview-foot-note{margin-top:3rem;padding:1.5rem;text-align:center;color:var(--ink-3);font-size:.85rem}</style>\n</head>`,
  );

const file = join(root, 'dist-preview', 'rookie-ready-preview.html');
await mkdir(dirname(file), { recursive: true });
await writeFile(file, html);
console.log(`Wrote ${file} (${(Buffer.byteLength(html) / 1024).toFixed(0)} KB)`);
