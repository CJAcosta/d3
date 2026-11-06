/**
 * Rookie trading card: drawn on a <canvas> in the browser and downloaded as a
 * PNG. The photo is read locally with FileReader and never uploaded or saved.
 */
import { CARD_COLORS, cardFacts, cleanCardText, cleanNumber, positionLabel, seasonYear } from '../lib/card';
import { svgIcon } from '../lib/icons';
import type { Plan } from '../lib/plan';
import { framed } from './env';
import { track } from './track';

const KEY = 'rr:v1:card';
const W = 750;
const H = 1050;
const FONT = 'system-ui, -apple-system, "Segoe UI", Roboto, Helvetica, Arial, sans-serif';
const INK = '#13233a';
const PAPER = '#fffaf0';

interface CardState {
  name: string;
  number: string;
  position: string;
  color: string;
  /** The plan name this card was started for, so a new kid's plan doesn't inherit it. */
  forName?: string;
}

const esc = (s: unknown) =>
  String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]!);

function load(): Partial<CardState> {
  try {
    return JSON.parse(localStorage.getItem(KEY) ?? '{}') as Partial<CardState>;
  } catch {
    return {};
  }
}

function save(s: CardState) {
  try {
    localStorage.setItem(KEY, JSON.stringify(s));
  } catch {
    /* private mode */
  }
}

// ---------------------------------------------------------------------------
// Markup
// ---------------------------------------------------------------------------

export function rookieCardSection(plan: Plan, stepNumber: number): string {
  const who = esc(plan.who);
  const swatches = CARD_COLORS.map(
    (c, i) =>
      `<label class="rc-swatch" style="--sw:${c.hex}" title="${c.name}"><input type="radio" name="rc-color" value="${c.hex}"${i === 0 ? ' checked' : ''}><span class="visually-hidden">${c.name}</span></label>`,
  ).join('');
  return `<section class="card step-card rookie no-print" id="rookie-card" aria-labelledby="s-rc">
    <h2 id="s-rc"><span class="num">${stepNumber}</span> Make ${plan.answers.name ? esc(plan.whose) : 'their'} rookie card</h2>
    <p>Every big leaguer has a rookie card. Now ${who} gets one too! Add a photo, pick a number and a team color, then download it or send it to the family. <strong>This part is the kid’s job.</strong></p>
    <div class="rc-grid">
      <div class="rc-preview"><canvas width="${W}" height="${H}" role="img" aria-label="Rookie card preview" data-rc-canvas></canvas></div>
      <div class="rc-form">
        <div class="field"><label for="rc-name">Name on the card</label><input id="rc-name" type="text" data-rc="name" maxlength="18" placeholder="Slugger" autocomplete="off"></div>
        <div class="rc-row">
          <div class="field"><label for="rc-number">Jersey #</label><input id="rc-number" type="text" inputmode="numeric" data-rc="number" maxlength="2" placeholder="e.g. 7" autocomplete="off"></div>
          <div class="field"><label for="rc-pos">Position</label><input id="rc-pos" type="text" data-rc="position" maxlength="16" autocomplete="off"></div>
        </div>
        <fieldset class="rc-colors"><legend>Team color</legend><div class="rc-swatches">${swatches}</div></fieldset>
        <div class="field">
          <span class="label">Photo</span>
          <div class="rc-photo-row">
            <label class="btn btn-ghost rc-photo">${svgIcon('camera')} <span data-rc-photo-label>Add a photo</span><input type="file" accept="image/*" class="visually-hidden" data-rc-file></label>
            <button type="button" class="linkish" data-rc-remove hidden>Remove photo</button>
          </div>
          <p class="hint">${svgIcon('shield')} The photo stays on this device. It’s never uploaded.</p>
        </div>
        <div class="rc-actions">
          <button type="button" class="btn btn-red" data-rc-download>${svgIcon('download')} Download card</button>
          <button type="button" class="btn btn-ghost" data-rc-share hidden>${svgIcon('share')} Send to family</button>
        </div>
      </div>
    </div>
  </section>`;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

function roundRect(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number) {
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.arcTo(x + w, y, x + w, y + h, r);
  ctx.arcTo(x + w, y + h, x, y + h, r);
  ctx.arcTo(x, y + h, x, y, r);
  ctx.arcTo(x, y, x + w, y, r);
  ctx.closePath();
}

function star(ctx: CanvasRenderingContext2D, cx: number, cy: number, r: number) {
  ctx.beginPath();
  for (let i = 0; i < 10; i++) {
    const rad = i % 2 === 0 ? r : r * 0.45;
    const ang = -Math.PI / 2 + (i * Math.PI) / 5;
    ctx.lineTo(cx + rad * Math.cos(ang), cy + rad * Math.sin(ang));
  }
  ctx.closePath();
}

function setSpacing(ctx: CanvasRenderingContext2D, px: number) {
  if ('letterSpacing' in ctx) (ctx as CanvasRenderingContext2D & { letterSpacing: string }).letterSpacing = `${px}px`;
}

/** Shrink the font until the text fits. Returns the size used. */
function fitFont(ctx: CanvasRenderingContext2D, text: string, maxW: number, start: number, min: number, weight = 800): number {
  let size = start;
  ctx.font = `${weight} ${size}px ${FONT}`;
  while (size > min && ctx.measureText(text).width > maxW) {
    size -= 2;
    ctx.font = `${weight} ${size}px ${FONT}`;
  }
  return size;
}

function drawBaseball(ctx: CanvasRenderingContext2D, cx: number, cy: number, r: number) {
  ctx.save();
  ctx.fillStyle = '#fff';
  ctx.strokeStyle = 'rgba(19,35,58,.18)';
  ctx.lineWidth = r * 0.04;
  ctx.beginPath();
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  ctx.strokeStyle = '#c62f2a';
  ctx.lineWidth = r * 0.05;
  ctx.lineCap = 'round';
  for (const side of [-1, 1]) {
    const ax = cx + side * r * 1.25;
    ctx.beginPath();
    ctx.arc(ax, cy, r * 0.82, side < 0 ? -0.95 : Math.PI - 0.95, side < 0 ? 0.95 : Math.PI + 0.95);
    ctx.stroke();
    for (let k = -3; k <= 3; k++) {
      const ang = (side < 0 ? 0 : Math.PI) + k * 0.26;
      const px = ax + Math.cos(ang) * r * 0.82;
      const py = cy + Math.sin(ang) * r * 0.82;
      const nx = Math.cos(ang);
      const ny = Math.sin(ang);
      ctx.beginPath();
      ctx.moveTo(px - nx * r * 0.07 - ny * r * 0.05, py - ny * r * 0.07 + nx * r * 0.05);
      ctx.lineTo(px - nx * r * 0.07 + ny * r * 0.05, py - ny * r * 0.07 - nx * r * 0.05);
      ctx.stroke();
    }
  }
  ctx.restore();
}

function drawCard(canvas: HTMLCanvasElement, s: CardState, plan: Plan, photo: HTMLImageElement | null) {
  const ctx = canvas.getContext('2d');
  if (!ctx) return;
  const facts = cardFacts(plan.answers);
  const color = s.color;
  const season = seasonYear();
  ctx.clearRect(0, 0, W, H);

  // Frame with pinstripes
  roundRect(ctx, 0, 0, W, H, 40);
  ctx.fillStyle = color;
  ctx.fill();
  ctx.save();
  roundRect(ctx, 0, 0, W, H, 40);
  ctx.clip();
  ctx.strokeStyle = 'rgba(255,255,255,.09)';
  ctx.lineWidth = 6;
  for (let x = -H; x < W; x += 26) {
    ctx.beginPath();
    ctx.moveTo(x, H);
    ctx.lineTo(x + H, 0);
    ctx.stroke();
  }
  ctx.restore();

  // Inner panel
  roundRect(ctx, 28, 28, W - 56, H - 56, 26);
  ctx.fillStyle = PAPER;
  ctx.fill();

  // Photo window
  const px = 52;
  const py = 52;
  const pw = W - 104;
  const ph = 600;
  ctx.save();
  roundRect(ctx, px, py, pw, ph, 20);
  ctx.clip();
  if (photo) {
    const scale = Math.max(pw / photo.naturalWidth, ph / photo.naturalHeight);
    const dw = photo.naturalWidth * scale;
    const dh = photo.naturalHeight * scale;
    ctx.drawImage(photo, px + (pw - dw) / 2, py + (ph - dh) * 0.35, dw, dh);
  } else {
    const g = ctx.createLinearGradient(0, py, 0, py + ph);
    g.addColorStop(0, '#e9f4ee');
    g.addColorStop(1, '#cfe7da');
    ctx.fillStyle = g;
    ctx.fillRect(px, py, pw, ph);
    // Infield dirt arc + chalk lines for a ballpark feel
    ctx.fillStyle = '#d9a774';
    ctx.beginPath();
    ctx.ellipse(W / 2, py + ph + 60, pw * 0.75, 250, 0, Math.PI, 0);
    ctx.fill();
    ctx.strokeStyle = 'rgba(255,255,255,.9)';
    ctx.lineWidth = 6;
    ctx.beginPath();
    ctx.moveTo(px - 20, py + ph);
    ctx.lineTo(W / 2, py + ph - 220);
    ctx.lineTo(px + pw + 20, py + ph);
    ctx.stroke();
    drawBaseball(ctx, W / 2, py + 270, 150);
  }
  ctx.restore();
  ctx.lineWidth = 8;
  ctx.strokeStyle = '#fff';
  roundRect(ctx, px + 4, py + 4, pw - 8, ph - 8, 17);
  ctx.stroke();

  // ROOKIE badge
  ctx.font = `800 30px ${FONT}`;
  setSpacing(ctx, 3);
  const label = 'ROOKIE';
  const lw = ctx.measureText(label).width;
  roundRect(ctx, 76, 76, lw + 92, 58, 29);
  ctx.fillStyle = color;
  ctx.fill();
  ctx.lineWidth = 4;
  ctx.strokeStyle = '#fff';
  ctx.stroke();
  ctx.fillStyle = '#ffd34d';
  star(ctx, 112, 105, 17);
  ctx.fill();
  ctx.fillStyle = '#fff';
  ctx.textBaseline = 'middle';
  ctx.fillText(label, 138, 106);

  // Season pill
  const seasonText = `${season} SEASON`;
  ctx.font = `800 24px ${FONT}`;
  setSpacing(ctx, 2);
  const sw = ctx.measureText(seasonText).width;
  roundRect(ctx, W - 76 - sw - 40, 80, sw + 40, 50, 25);
  ctx.fillStyle = '#fff';
  ctx.fill();
  ctx.fillStyle = color;
  ctx.fillText(seasonText, W - 76 - sw - 20, 106);
  setSpacing(ctx, 0);

  // Name band (slanted)
  ctx.beginPath();
  ctx.moveTo(28, 684);
  ctx.lineTo(W - 28, 664);
  ctx.lineTo(W - 28, 798);
  ctx.lineTo(28, 818);
  ctx.closePath();
  ctx.fillStyle = color;
  ctx.fill();

  const hasNumber = s.number !== '';
  const name = (s.name.trim() || 'Slugger').toUpperCase();
  const nameMax = hasNumber ? 470 : 620;
  fitFont(ctx, name, nameMax, 78, 34);
  setSpacing(ctx, 1);
  ctx.fillStyle = '#fff';
  ctx.textBaseline = 'alphabetic';
  ctx.fillText(name, 64, 758);
  ctx.font = `700 28px ${FONT}`;
  setSpacing(ctx, 4);
  ctx.fillStyle = 'rgba(255,255,255,.88)';
  ctx.fillText((s.position.trim() || 'Utility').toUpperCase(), 66, 796);
  setSpacing(ctx, 0);

  // Jersey number
  if (hasNumber) {
    const cx = W - 118;
    const cy = 716;
    ctx.beginPath();
    ctx.arc(cx, cy, 74, 0, Math.PI * 2);
    ctx.fillStyle = '#fff';
    ctx.fill();
    ctx.lineWidth = 10;
    ctx.strokeStyle = color;
    ctx.stroke();
    ctx.textAlign = 'center';
    ctx.fillStyle = '#6b7a90';
    ctx.font = `800 18px ${FONT}`;
    ctx.textBaseline = 'middle';
    ctx.fillText('NO.', cx, cy - 38);
    ctx.fillStyle = color;
    ctx.font = `900 ${s.number.length > 1 ? 66 : 76}px ${FONT}`;
    ctx.fillText(s.number, cx, cy + 10);
    ctx.textAlign = 'left';
  }

  // Stat line
  const cols = [
    { k: 'B/T', v: facts.bt },
    { k: 'AGE', v: facts.age },
    facts.height ? { k: 'HEIGHT', v: facts.height } : { k: 'ROOKIE YR', v: `’${String(season).slice(2)}` },
  ];
  const left = 52;
  const colW = (W - 104) / cols.length;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'alphabetic';
  cols.forEach((c, i) => {
    const cx = left + colW * i + colW / 2;
    ctx.fillStyle = '#6b7a90';
    ctx.font = `800 20px ${FONT}`;
    setSpacing(ctx, 3);
    ctx.fillText(c.k, cx, 870);
    setSpacing(ctx, 0);
    ctx.fillStyle = INK;
    ctx.font = `800 44px ${FONT}`;
    ctx.fillText(c.v, cx, 922);
    if (i > 0) {
      ctx.strokeStyle = '#e6ddcc';
      ctx.lineWidth = 3;
      ctx.beginPath();
      ctx.moveTo(left + colW * i, 846);
      ctx.lineTo(left + colW * i, 930);
      ctx.stroke();
    }
  });

  // Gear line + footer
  ctx.fillStyle = color;
  fitFont(ctx, facts.gear, W - 120, 26, 18, 800);
  ctx.fillText(facts.gear, W / 2, 972);
  ctx.fillStyle = '#8a97aa';
  ctx.font = `700 18px ${FONT}`;
  setSpacing(ctx, 1);
  const host = (import.meta.env.SITE ? new URL(import.meta.env.SITE).host : '').replace(/^www\./, '');
  ctx.fillText(host ? `Made with Rookie Ready · ${host}` : 'Made with Rookie Ready', W / 2, 1008);
  setSpacing(ctx, 0);
  ctx.textAlign = 'left';
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

declare global {
  interface Window {
    /** Optional host hook (e.g. a preview frame) that saves a file. Returns true if handled. */
    rrSaveFile?: (blob: Blob, filename: string) => Promise<boolean>;
  }
}

/** Where downloads are blocked (embeds, some in-app browsers), show the card big so it can be saved by hand. */
function showCardToSave(blob: Blob) {
  const url = URL.createObjectURL(blob);
  const wrap = document.createElement('div');
  wrap.className = 'rc-modal';
  wrap.setAttribute('role', 'dialog');
  wrap.setAttribute('aria-modal', 'true');
  wrap.setAttribute('aria-label', 'Save your rookie card');
  wrap.innerHTML = `<div class="rc-modal-box"><img src="${url}" alt="Rookie card"><p><strong>Press and hold the card</strong> (or right-click it) and choose “Save image.”</p><button type="button" class="btn">Done</button></div>`;
  const close = () => {
    wrap.remove();
    URL.revokeObjectURL(url);
  };
  wrap.addEventListener('click', (e) => {
    if (e.target === wrap || (e.target as HTMLElement).closest('button')) close();
  });
  document.body.append(wrap);
  wrap.querySelector('button')?.focus();
}

async function saveCard(blob: Blob, filename: string) {
  if (window.rrSaveFile && (await window.rrSaveFile(blob, filename).catch(() => false))) return;
  if (framed) return showCardToSave(blob);
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = filename;
  document.body.append(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 4000);
}

// ---------------------------------------------------------------------------
// Wiring
// ---------------------------------------------------------------------------

export function mountRookieCard(root: HTMLElement, plan: Plan) {
  const section = root.querySelector<HTMLElement>('#rookie-card');
  const canvas = section?.querySelector<HTMLCanvasElement>('[data-rc-canvas]');
  if (!section || !canvas) return;

  const planName = plan.answers.name ?? '';
  const stored = load();
  const saved = stored.forName === planName ? stored : {};
  const state: CardState = {
    name: cleanCardText(saved.name ?? planName, 18),
    number: cleanNumber(saved.number ?? ''),
    position: cleanCardText(saved.position ?? positionLabel(plan.answers.position), 16),
    color: CARD_COLORS.some((c) => c.hex === saved.color) ? saved.color! : CARD_COLORS[0].hex,
    forName: planName,
  };
  let photo: HTMLImageElement | null = null;

  const inputs = {
    name: section.querySelector<HTMLInputElement>('[data-rc="name"]')!,
    number: section.querySelector<HTMLInputElement>('[data-rc="number"]')!,
    position: section.querySelector<HTMLInputElement>('[data-rc="position"]')!,
  };
  inputs.name.value = state.name;
  inputs.number.value = state.number;
  inputs.position.value = state.position;
  section.querySelectorAll<HTMLInputElement>('input[name="rc-color"]').forEach((r) => (r.checked = r.value === state.color));

  let queued = false;
  const redraw = () => {
    if (queued) return;
    queued = true;
    requestAnimationFrame(() => {
      queued = false;
      drawCard(canvas, state, plan, photo);
    });
  };
  redraw();

  section.addEventListener('input', (e) => {
    const t = e.target as HTMLInputElement;
    if (t.dataset.rc === 'name') state.name = cleanCardText(t.value, 18);
    else if (t.dataset.rc === 'number') {
      state.number = cleanNumber(t.value);
      if (t.value !== state.number) t.value = state.number;
    } else if (t.dataset.rc === 'position') state.position = cleanCardText(t.value, 16);
    else if (t.name === 'rc-color') state.color = t.value;
    else return;
    save(state);
    redraw();
  });

  const fileInput = section.querySelector<HTMLInputElement>('[data-rc-file]')!;
  const removeBtn = section.querySelector<HTMLButtonElement>('[data-rc-remove]')!;
  const photoLabel = section.querySelector<HTMLElement>('[data-rc-photo-label]')!;
  fileInput.addEventListener('change', () => {
    const file = fileInput.files?.[0];
    if (!file || !file.type.startsWith('image/')) return;
    const url = URL.createObjectURL(file);
    const img = new Image();
    img.onload = () => {
      photo = img;
      removeBtn.hidden = false;
      photoLabel.textContent = 'Change photo';
      redraw();
      track('Rookie Card Photo Added', {});
    };
    img.src = url;
  });
  removeBtn.addEventListener('click', () => {
    photo = null;
    fileInput.value = '';
    removeBtn.hidden = true;
    photoLabel.textContent = 'Add a photo';
    redraw();
  });

  const fileName = () => `${(state.name.trim() || 'rookie').replace(/[^\w-]+/g, '-').toLowerCase()}-rookie-card.png`;
  const toBlob = () => new Promise<Blob | null>((res) => canvas.toBlob(res, 'image/png'));

  section.querySelector('[data-rc-download]')!.addEventListener('click', async () => {
    drawCard(canvas, state, plan, photo);
    const blob = await toBlob();
    if (!blob) return;
    await saveCard(blob, fileName());
    track('Rookie Card Downloaded', { photo: !!photo, number: state.number !== '' });
  });

  const shareBtn = section.querySelector<HTMLButtonElement>('[data-rc-share]')!;
  try {
    shareBtn.hidden = framed || !navigator.canShare?.({ files: [new File([''], 'card.png', { type: 'image/png' })] });
  } catch {
    shareBtn.hidden = true;
  }
  shareBtn.addEventListener('click', async () => {
    drawCard(canvas, state, plan, photo);
    const blob = await toBlob();
    if (!blob) return;
    const file = new File([blob], fileName(), { type: 'image/png' });
    try {
      await navigator.share({ files: [file], title: `${state.name || 'Our rookie'}’s rookie card` });
      track('Rookie Card Shared', {});
    } catch {
      /* cancelled */
    }
  });
}
