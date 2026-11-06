/**
 * A short burst of baseballs and confetti. Skipped entirely for people who
 * prefer reduced motion.
 */
const COLORS = ['#1d7a4b', '#c62f2a', '#f2b84b', '#b8703c', '#2457c5'];

interface Bit {
  x: number;
  y: number;
  vx: number;
  vy: number;
  r: number;
  rot: number;
  vr: number;
  ball: boolean;
  color: string;
}

function drawBall(ctx: CanvasRenderingContext2D, b: Bit) {
  ctx.save();
  ctx.translate(b.x, b.y);
  ctx.rotate(b.rot);
  ctx.fillStyle = '#fff';
  ctx.strokeStyle = 'rgba(19,35,58,.25)';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.arc(0, 0, b.r, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  ctx.strokeStyle = '#c62f2a';
  ctx.lineWidth = Math.max(1.2, b.r * 0.16);
  ctx.beginPath();
  ctx.arc(-b.r * 1.3, 0, b.r * 0.95, -0.8, 0.8);
  ctx.stroke();
  ctx.beginPath();
  ctx.arc(b.r * 1.3, 0, b.r * 0.95, Math.PI - 0.8, Math.PI + 0.8);
  ctx.stroke();
  ctx.restore();
}

export function celebrate(): void {
  if (typeof window === 'undefined' || matchMedia('(prefers-reduced-motion: reduce)').matches) return;
  const canvas = document.createElement('canvas');
  canvas.className = 'celebrate';
  canvas.setAttribute('aria-hidden', 'true');
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  const w = window.innerWidth;
  const h = window.innerHeight;
  canvas.width = w * dpr;
  canvas.height = h * dpr;
  document.body.append(canvas);
  const ctx = canvas.getContext('2d');
  if (!ctx) return canvas.remove();
  ctx.scale(dpr, dpr);

  const bits: Bit[] = [];
  const count = Math.min(90, Math.round(w / 10));
  for (let i = 0; i < count; i++) {
    const fromLeft = i % 2 === 0;
    const ball = i % 4 === 0;
    bits.push({
      x: fromLeft ? -10 : w + 10,
      y: h * (0.55 + Math.random() * 0.3),
      vx: (fromLeft ? 1 : -1) * (4 + Math.random() * 7),
      vy: -(9 + Math.random() * 9),
      r: ball ? 8 + Math.random() * 5 : 4 + Math.random() * 3,
      rot: Math.random() * Math.PI,
      vr: (Math.random() - 0.5) * 0.3,
      ball,
      color: COLORS[i % COLORS.length],
    });
  }

  const start = performance.now();
  const life = 2200;
  const tick = (t: number) => {
    const age = t - start;
    ctx.clearRect(0, 0, w, h);
    ctx.globalAlpha = age > life - 500 ? Math.max(0, (life - age) / 500) : 1;
    for (const b of bits) {
      b.vy += 0.32;
      b.vx *= 0.99;
      b.x += b.vx;
      b.y += b.vy;
      b.rot += b.vr;
      if (b.ball) drawBall(ctx, b);
      else {
        ctx.save();
        ctx.translate(b.x, b.y);
        ctx.rotate(b.rot);
        ctx.fillStyle = b.color;
        ctx.fillRect(-b.r, -b.r * 0.6, b.r * 2, b.r * 1.2);
        ctx.restore();
      }
    }
    if (age < life) requestAnimationFrame(tick);
    else canvas.remove();
  };
  requestAnimationFrame(tick);
}
