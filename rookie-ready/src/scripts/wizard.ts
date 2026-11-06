/**
 * The guided gear wizard. Plain TypeScript, no framework.
 *
 * Privacy: answers live only in this browser (localStorage). Share links
 * carry the answers but never the child's name.
 */
import { site } from '../config/site';
import { BUDGET_LABEL } from '../data/gear';
import { svgIcon } from '../lib/icons';
import { SPONSORED_REL } from '../lib/links';
import { PRIORITY_LABEL, buildPlan, cleanName, decodeAnswers, encodeAnswers, type Plan, type PlanItem } from '../lib/plan';
import { CERT_INFO, batFor, batCertFor, dropLabel, frac, gloveFor, hatSize, pantsFor, stageFor, typicalForAge } from '../lib/sizing';
import type { Answers, BatSide, Budget, Experience, Hand, Have, Position, Priority, Protection, Team } from '../lib/types';
import { celebrate } from './celebrate';
import { mountRookieCard, rookieCardSection } from './rookieCard';
import { track } from './track';

const STEPS = ['intro', 'age', 'size', 'hands', 'team', 'position', 'budget', 'extras'] as const;
type StepId = (typeof STEPS)[number];

const KEY_DRAFT = 'rr:v1:draft';
const KEY_DONE = 'rr:v1:done';
const KEY_GOT = 'rr:v1:got';
const KEY_CARD = 'rr:v1:card';

/** One bit of dugout trivia per question. */
const TRIVIA = [
  'A big-league baseball has 108 double stitches, and every one is sewn by hand.',
  'Little League started in 1939 in Williamsport, Pennsylvania, with just three teams.',
  'The first ballplayers caught bare-handed. Gloves didn’t catch on until the 1870s and 1880s.',
  'Home plate is 17 inches wide, from Little League all the way to the big leagues.',
  'In Little League Majors the bases are 60 feet apart. In the big leagues, they’re 90.',
  'Little League pitchers throw from 46 feet away. Big leaguers throw from 60 feet, 6 inches.',
  'An official baseball weighs about 5 ounces and is about 9 inches around.',
];

/** Where the runner is after each answered question. */
const BASE_CALLS = ['Batter up!', 'Running to first', 'Safe at first!', 'Heading for second', 'Safe at second!', 'Rounding third', 'Waved home! Last one'];

interface Draft {
  name: string;
  age?: number;
  feet?: number;
  inches?: number;
  weight?: number;
  unsureSize: boolean;
  throws?: Hand;
  bats?: BatSide;
  team?: Team;
  position?: Position;
  experience?: Experience;
  budget?: Budget;
  have: Have[];
  protection?: Protection;
  head?: number;
  shoe: string;
}

// ---------------------------------------------------------------------------
// Small utilities
// ---------------------------------------------------------------------------

const $ = <T extends Element = HTMLElement>(sel: string, root: ParentNode = document) => root.querySelector<T>(sel);
const $$ = <T extends Element = HTMLElement>(sel: string, root: ParentNode = document) => Array.from(root.querySelectorAll<T>(sel));

const esc = (s: unknown) =>
  String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]!);

const money = ([lo, hi]: [number, number]) => `$${lo.toLocaleString()}–$${hi.toLocaleString()}`;

function store<T>(key: string, value?: T): T | null {
  try {
    if (value === undefined) {
      const raw = localStorage.getItem(key);
      return raw ? (JSON.parse(raw) as T) : null;
    }
    localStorage.setItem(key, JSON.stringify(value));
    return value;
  } catch {
    return null;
  }
}

function forget(...keys: string[]) {
  try {
    keys.forEach((k) => localStorage.removeItem(k));
  } catch {
    /* private mode */
  }
}

let toastTimer = 0;
function toast(msg: string) {
  let el = $('#rr-toast');
  if (!el) {
    el = document.createElement('div');
    el.id = 'rr-toast';
    el.className = 'toast';
    el.setAttribute('role', 'status');
    document.body.append(el);
  }
  el.textContent = msg;
  el.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = window.setTimeout(() => el!.classList.remove('show'), 3200);
}

async function copyText(text: string): Promise<boolean> {
  try {
    await navigator.clipboard.writeText(text);
    return true;
  } catch {
    const ta = document.createElement('textarea');
    ta.value = text;
    ta.style.position = 'fixed';
    ta.style.opacity = '0';
    document.body.append(ta);
    ta.select();
    const ok = document.execCommand('copy');
    ta.remove();
    return ok;
  }
}

// ---------------------------------------------------------------------------
// Draft <-> form <-> answers
// ---------------------------------------------------------------------------

function readDraft(form: HTMLFormElement): Draft {
  const fd = new FormData(form);
  const s = (k: string) => (fd.get(k) as string | null) ?? '';
  const n = (k: string) => {
    const v = s(k).trim();
    if (v === '') return undefined;
    const x = Number(v);
    return Number.isFinite(x) ? x : undefined;
  };
  return {
    name: cleanName(s('name')),
    age: n('age'),
    feet: n('feet'),
    inches: n('inches'),
    weight: n('weight'),
    unsureSize: fd.get('unsureSize') === 'on',
    throws: (s('throws') || undefined) as Hand | undefined,
    bats: (s('bats') || undefined) as BatSide | undefined,
    team: (s('team') || undefined) as Team | undefined,
    position: (s('position') || undefined) as Position | undefined,
    experience: (s('experience') || undefined) as Experience | undefined,
    budget: (s('budget') || undefined) as Budget | undefined,
    have: fd.getAll('have') as Have[],
    protection: (s('protection') || undefined) as Protection | undefined,
    head: n('head'),
    shoe: s('shoe').replace(/[^0-9A-Za-z.½ ]/g, '').slice(0, 6),
  };
}

function fillForm(form: HTMLFormElement, d: Partial<Draft>) {
  const setRadio = (name: string, value?: string) => {
    $$<HTMLInputElement>(`input[name="${name}"]`, form).forEach((i) => (i.checked = value != null && i.value === value));
  };
  ($<HTMLInputElement>('input[name="name"]', form)!).value = d.name ?? '';
  setRadio('age', d.age != null ? String(d.age) : undefined);
  ($<HTMLSelectElement>('select[name="feet"]', form)!).value = d.feet != null ? String(d.feet) : '';
  ($<HTMLSelectElement>('select[name="inches"]', form)!).value = d.inches != null ? String(d.inches) : '';
  ($<HTMLInputElement>('input[name="weight"]', form)!).value = d.weight != null ? String(d.weight) : '';
  ($<HTMLInputElement>('input[name="unsureSize"]', form)!).checked = !!d.unsureSize;
  setRadio('throws', d.throws);
  setRadio('bats', d.bats);
  setRadio('team', d.team);
  setRadio('position', d.position);
  setRadio('experience', d.experience ?? 'first');
  setRadio('budget', d.budget ?? 'balanced');
  $$<HTMLInputElement>('input[name="have"]', form).forEach((i) => (i.checked = (d.have ?? []).includes(i.value as Have)));
  setRadio('protection', d.protection);
  ($<HTMLInputElement>('input[name="head"]', form)!).value = d.head != null ? String(d.head) : '';
  ($<HTMLInputElement>('input[name="shoe"]', form)!).value = d.shoe ?? '';
}

function heightOf(d: Draft): number | undefined {
  if (d.unsureSize || d.feet == null) return undefined;
  return d.feet * 12 + (d.inches ?? 0);
}

function weightOf(d: Draft): number | undefined {
  if (d.unsureSize || d.weight == null || d.weight < 25 || d.weight > 300) return undefined;
  return d.weight;
}

/** Fill gaps with neutral defaults so a partial draft can still be previewed. */
function toAnswers(d: Draft): Answers {
  const h = heightOf(d);
  const w = weightOf(d);
  return {
    name: d.name,
    age: d.age ?? 8,
    heightIn: h != null && w != null ? h : undefined,
    weightLb: h != null && w != null ? w : undefined,
    throws: d.throws ?? 'unsure',
    bats: d.bats ?? 'unsure',
    team: d.team ?? 'unsure',
    position: d.position ?? 'unsure',
    experience: d.experience ?? 'first',
    budget: d.budget ?? 'balanced',
    have: d.position === 'catcher' ? d.have : d.have.filter((x) => x !== 'catcher'),
    protection: d.protection ?? 'skip',
    headIn: d.head != null && d.head >= 16 && d.head <= 26 ? d.head : undefined,
    shoe: d.shoe || undefined,
  };
}

function draftFromAnswers(a: Answers): Draft {
  return {
    name: a.name ?? '',
    age: a.age,
    feet: a.heightIn != null ? Math.floor(a.heightIn / 12) : undefined,
    inches: a.heightIn != null ? Math.round(a.heightIn % 12) : undefined,
    weight: a.weightLb,
    unsureSize: a.heightIn == null,
    throws: a.throws,
    bats: a.bats,
    team: a.team,
    position: a.position,
    experience: a.experience,
    budget: a.budget,
    have: a.have,
    protection: a.protection,
    head: a.headIn,
    shoe: a.shoe ?? '',
  };
}

function validate(step: StepId, d: Draft): boolean {
  switch (step) {
    case 'age':
      return d.age != null;
    case 'size':
      return d.unsureSize || (heightOf(d) != null && weightOf(d) != null && heightOf(d)! >= 30);
    case 'hands':
      return d.throws != null && d.bats != null;
    case 'team':
      return d.team != null;
    case 'position':
      return d.position != null;
    case 'budget':
      return d.experience != null && d.budget != null;
    case 'extras':
      return d.protection != null;
    default:
      return true;
  }
}

// ---------------------------------------------------------------------------
// Coach reactions
// ---------------------------------------------------------------------------

function coachHtml(text: string, title = 'Coach'): string {
  return `<div class="coach coach-react"><span class="coach-avatar">${svgIcon('whistle')}</span><div class="coach-bubble"><span class="coach-name">${esc(title)}</span><p>${text}</p></div></div>`;
}

function reaction(field: string, d: Draft): string {
  const who = esc(d.name || 'your player');
  const Who = esc(d.name || 'Your player');
  const a = toAnswers(d);
  switch (field) {
    case 'age': {
      if (d.age == null) return '';
      if (d.age <= 5) return `Tee ball age, the most fun stage there is. At this age it’s all about <strong>light, small and easy</strong>. Kids don’t need much, and we’ll keep the list short.`;
      if (d.age <= 8) return `Great age! Most ${d.age}-year-olds play coach or machine pitch. Gear needs to be <strong>light enough to swing</strong> and a glove they can actually squeeze shut.`;
      if (d.age <= 12) return `Kid pitch! The ball comes in faster now, so <strong>fit really matters</strong>. The right bat length and a well-sized glove make a big difference.`;
      if (d.age <= 14) return `A big year. The field gets bigger and the rules change (bats, metal cleats). Don’t worry, we’ll keep it all straight.`;
      return `High school ball has the strictest gear rules, especially for bats. We’ll make sure everything is legal.`;
    }
    case 'size': {
      if (d.unsureSize) {
        const t = typicalForAge(a.age);
        return `No problem. We’ll use typical sizes for a ${a.age}-year-old (about ${Math.floor(t.h / 12)}′${Math.round(t.h % 12)}″, ${t.w} lb). You can measure later and update the plan in one tap.`;
      }
      if (heightOf(d) == null || weightOf(d) == null) return '';
      const bat = batFor(a);
      const len = bat.lengths.length > 1 ? `${bat.lengths[0]}–${bat.lengths[1]}″` : `${bat.length}″`;
      return `Nice. That puts ${who} at about a <strong>${len} bat</strong> and <strong>${esc(pantsFor(a).size)}</strong> pants. We’ll fine-tune once we know the league.`;
    }
    case 'throws': {
      if (d.throws === 'R')
        return `A righty! ${Who} needs a <strong>Right-Hand Throw (RHT)</strong> glove, which goes on the <strong>left</strong> hand. This trips up a lot of parents in the store.`;
      if (d.throws === 'L')
        return `A lefty! Look for <strong>Left-Hand Throw (LHT)</strong> gloves, which go on the <strong>right</strong> hand. Stores carry fewer lefty gloves, so shopping online is often easier.`;
      if (d.throws === 'unsure')
        return `Easy test: roll ${who} a ball and say “throw it back!” The hand they throw with is their throwing hand, and the glove goes on the <em>other</em> one. You can update this later.`;
      return '';
    }
    case 'bats': {
      if (d.bats === 'R') return `Right-handed batters face the pitcher with their left side, so a jaw guard (C-flap) goes on the <strong>left</strong>.`;
      if (d.bats === 'L') return `Left-handed batters face the pitcher with their right side, so a jaw guard (C-flap) goes on the <strong>right</strong>.`;
      if (d.bats === 'S') return `A switch hitter! A <strong>full face guard</strong> covers both sides, so that’s what we’ll suggest.`;
      if (d.bats === 'unsure') return `Most young kids bat on the same side they throw. If you’re not sure, a <strong>full face guard</strong> covers both sides.`;
      return '';
    }
    case 'team': {
      if (!d.team) return '';
      const cert = CERT_INFO[batCertFor(a.age, a.team, stageFor(a.age, a.team))];
      const lead: Record<Team, string> = {
        teeball: 'Tee ball bats are their own category: short, light and made only for soft tee balls.',
        rec: 'Little League and most rec leagues only allow bats with the <strong>USA Baseball</strong> stamp. Buying the wrong stamp is the #1 bat mistake, so we’ll flag it.',
        travel: 'Most travel teams play USSSA rules, but not all. We’ll give you the exact question to ask the coach.',
        school: 'School ball usually means stricter bat rules. High school requires BBCOR bats.',
        unsure: 'No problem! We’ll plan for Little League/rec rules (the most common) and remind you what to confirm.',
      };
      return `${lead[d.team]} For ${who}, that means a <strong>${esc(cert.label)}</strong>.`;
    }
    case 'position': {
      if (!d.position) return '';
      const g = gloveFor(a);
      const size = `<strong>${frac(g.size)}″</strong>`;
      const map: Record<Position, string> = {
        unsure: `Perfect, that’s the most common answer. Young players move around the field, so we’ll pick an all-around ${size} glove.`,
        infield: `Infielders want a slightly smaller glove for quick transfers. We’re thinking ${size}.`,
        outfield: `Outfielders get a little extra length for fly balls. We’re thinking ${size}.`,
        pitcher: `Pitchers want a closed-web glove so batters can’t see their grip. We’re thinking ${size}. Also ask the coach about pitch-count rules.`,
        catcher: `Catchers need the most gear, so we’ll add a catcher’s set and mitt and show you how to measure for them. Many teams provide a set, so ask first.`,
        firstbase: a.age >= 9 ? `We’ll add a first base mitt as an option, but ${who} still needs a regular glove for other positions.` : `At this age a regular glove works fine at first base. No special mitt needed yet.`,
      };
      return map[d.position];
    }
    case 'experience':
      return d.experience === 'first'
        ? `First season, how exciting! We’ll lean toward <strong>lighter and smaller</strong>. It’s easier to learn with, and it builds confidence.`
        : `Welcome back! Kids grow a lot between seasons, so we’ll double-check last year’s gear too.`;
    case 'budget':
      return {
        lean: `Smart. Kids outgrow gear every year or two. We’ll focus on fit and safety, not fancy features.`,
        balanced: `The sweet spot: good gear that lasts the season, and maybe the next one.`,
        allin: `Love it. We’ll still steer you away from stuff that doesn’t help a young player.`,
      }[d.budget ?? 'balanced'];
    default:
      return '';
  }
}

// ---------------------------------------------------------------------------
// Live preview panel
// ---------------------------------------------------------------------------

function previewRows(d: Draft): { label: string; value: string; done: boolean }[] {
  const a = toAnswers(d);
  const bat = batFor(a);
  const glove = gloveFor(a);
  const hasSize = d.unsureSize || (heightOf(d) != null && weightOf(d) != null);
  const len = bat.lengths.length > 1 ? `${bat.lengths[0]}–${bat.lengths[1]}″` : `${bat.length}″`;
  const certShort: Record<string, string> = {
    'usa-teeball': 'tee ball',
    usa: 'USA',
    usssa: 'USSSA',
    'usa-or-bbcor': 'USA/BBCOR',
    bbcor: 'BBCOR',
  };
  const head = a.headIn ?? typicalForAge(a.age).head;
  return [
    { label: 'Bat', value: `${len} · ${dropLabel(bat.startDrop)} · ${certShort[bat.cert]}`, done: hasSize && d.team != null },
    { label: 'Glove', value: `${frac(glove.size)}″${glove.hand.code ? ` ${glove.hand.code}` : ''}`, done: d.throws != null && d.position != null },
    { label: 'Helmet', value: `Hat ${a.headIn ? '' : '~'}${frac(hatSize(head))}`, done: d.age != null },
    { label: 'Cleats', value: a.age <= 12 ? 'Molded' : 'Molded or metal', done: d.age != null },
    { label: 'Pants', value: pantsFor(a).size, done: hasSize },
  ];
}

// ---------------------------------------------------------------------------
// Results
// ---------------------------------------------------------------------------

const GROUPS: { key: Priority | 'covered'; tag: string; title: string; blurb: string }[] = [
  { key: 'must', tag: 'The starting lineup', title: 'Must-haves', blurb: 'Get these before the first practice.' },
  { key: 'recommended', tag: 'Off the bench', title: 'Recommended', blurb: 'Not required, but most families are glad they got them.' },
  { key: 'nice', tag: 'Extra innings', title: 'Nice to have', blurb: 'Totally optional. Wait and see how the season goes.' },
  { key: 'covered', tag: 'Already in the dugout', title: 'Already covered', blurb: 'You have these or the team provides them. Give them a quick fit check.' },
];

function list(items: string[], cls = ''): string {
  return items.length ? `<ul class="${cls}">${items.map((i) => `<li>${esc(i)}</li>`).join('')}</ul>` : '';
}

function shopButtons(it: PlanItem): string {
  if (!it.links.length) return '';
  const btns = it.links
    .map(
      (l, i) =>
        `<a class="shop-btn${i ? ' alt' : ''}" href="${esc(l.href)}" rel="${SPONSORED_REL}" target="_blank" data-aff="${esc(l.merchant)}" data-item="${esc(it.id)}">Shop at ${esc(l.label)} ${svgIcon('external')}</a>`,
    )
    .join('');
  return `<div class="shop"><p class="shop-for">Search pre-filled for: <strong>${esc(it.headline)}</strong></p><div class="shop-row">${btns}<span class="paid-note">Paid links</span></div></div>`;
}

function picksHtml(it: PlanItem): string {
  if (!it.picks.length) return '';
  return `<div class="picks"><p class="picks-title">Our picks</p>${it.picks
    .map(
      (p) =>
        `<div class="pick"><p><strong>${esc(p.name)}</strong> ${esc(p.blurb)}</p><div class="shop-row">${p.links
          .map(
            (l) =>
              `<a class="shop-btn alt" href="${esc(l.href)}" rel="${SPONSORED_REL}" target="_blank" data-aff="${esc(l.merchant)}" data-item="${esc(it.id)}-pick">${esc(l.label)} ${svgIcon('external')}</a>`,
          )
          .join('')}</div></div>`,
    )
    .join('')}</div>`;
}

const nick = (it: PlanItem) => (it.info.nickname ? ` <span class="nick">a.k.a. “${esc(it.info.nickname)}”</span>` : '');

function batTimer(plan: Plan): string {
  return `<div class="timer no-print" data-timer>
    <div class="timer-face" aria-live="polite"><span class="timer-num" data-timer-num>20</span><span class="timer-unit">sec</span></div>
    <div class="timer-body">
      <p class="timer-title">Kid challenge: the 20-second bat hold</p>
      <p class="timer-text" data-timer-text>In the store, have ${esc(plan.who)} hold the bat straight out to the side by the handle. If it dips before the clock hits zero, it’s too heavy.</p>
      <button type="button" class="btn btn-ghost" data-act="timer">${svgIcon('clock')} <span data-timer-label>Start the clock</span></button>
    </div>
  </div>`;
}

function itemCard(it: PlanItem, plan: Plan, got: Set<string>): string {
  const key = it.covered ? 'covered' : it.priority;
  const badge = it.covered ? 'Already covered' : PRIORITY_LABEL[it.priority];
  const specs = it.specs.length
    ? `<dl class="specs">${it.specs
        .map((s) => `<div><dt>${esc(s.label)}</dt><dd>${esc(s.value)}${s.note ? `<small>${esc(s.note)}</small>` : ''}</dd></div>`)
        .join('')}</dl>`
    : '';
  const notes = it.notes.length
    ? `<ul class="item-notes">${it.notes.map((n) => `<li>${svgIcon('whistle')}<span>${esc(n)}</span></li>`).join('')}</ul>`
    : '';
  const [lo, hi] = it.price;
  const more = `<details class="item-more">
      <summary>How to check the fit, what to look for &amp; mistakes to skip</summary>
      <div class="item-more-body">
        <h4>${svgIcon('ruler')} Fit check</h4>${list(it.info.fitCheck)}
        <h4>${svgIcon('check')} What to look for</h4>${list(it.info.lookFor)}
        <h4>${svgIcon('info')} Skip these mistakes</h4>${list(it.info.mistakes)}
        <p class="tier"><strong>${esc(BUDGET_LABEL[plan.answers.budget])}:</strong> ${esc(it.info.tier[plan.answers.budget])} <span class="muted">Typical range: $${lo}–$${hi}.</span></p>
        ${it.info.guide ? `<p><a href="${esc(it.info.guide.href)}">${esc(it.info.guide.label)} →</a></p>` : ''}
      </div>
    </details>`;

  if (it.covered) {
    return `<article class="item item-covered" id="item-${it.id}">
      <div class="item-head">
        <span class="item-icon">${svgIcon(it.info.icon)}</span>
        <div class="item-title"><span class="badge badge-covered">${badge}</span><h3>${esc(it.info.name)}${nick(it)}</h3>
        <p class="item-spec">Should be: ${esc(it.headline)}</p></div>
      </div>
      <p class="item-why"><strong>Quick check:</strong> ${esc(it.info.fitCheck[0])}</p>
      ${more}
    </article>`;
  }

  return `<article class="item item-${key}${got.has(it.id) ? ' is-got' : ''}" id="item-${it.id}">
    <div class="item-head">
      <span class="item-icon">${svgIcon(it.info.icon)}</span>
      <div class="item-title">
        <span class="badge badge-${key}">${badge}</span>
        <h3>${esc(it.info.name)}${nick(it)}</h3>
        <p class="item-spec">${esc(it.headline)}</p>
      </div>
      <label class="got-it no-print"><input type="checkbox" data-got="${it.id}"${got.has(it.id) ? ' checked' : ''}><span>Got it</span></label>
    </div>
    <p class="item-why">${esc(it.why)}</p>
    ${specs}
    ${notes}
    ${it.id === 'bat' ? batTimer(plan) : ''}
    ${picksHtml(it)}
    ${shopButtons(it)}
    ${more}
  </article>`;
}

function newsletterHtml(plan: Plan): string {
  const n = site.newsletter;
  if (!n.action) return '';
  const hidden = Object.entries(n.hiddenFields)
    .map(([k, v]) => `<input type="hidden" name="${esc(k)}" value="${esc(v)}">`)
    .join('');
  return `<div class="card side-card no-print">
    <h3>${svgIcon('mail')} Get a pre-season reminder</h3>
    <p>We’ll email you this checklist plus a reminder two weeks before the season, when gear sells out. No spam. Unsubscribe anytime.</p>
    <form class="signup" action="${esc(n.action)}" method="post" target="_blank" data-signup>
      <label class="visually-hidden" for="rr-email">Email address</label>
      <input id="rr-email" type="email" name="${esc(n.emailFieldName)}" required placeholder="you@example.com" autocomplete="email">
      <input type="hidden" name="stage" value="${esc(plan.stage)}">
      ${hidden}
      <button class="btn" type="submit">Send it</button>
    </form>
  </div>`;
}

function updateMeter(root: HTMLElement, plan: Plan): boolean {
  const active = plan.items.filter((i) => !i.covered);
  const mustCount = active.filter((i) => i.priority === 'must').length;
  const checked = new Set($$<HTMLInputElement>('input[data-got]:checked', root).map((i) => i.dataset.got!));
  const mustDone = active.filter((i) => i.priority === 'must' && checked.has(i.id)).length;
  const pct = mustCount ? Math.round((mustDone / mustCount) * 100) : 100;
  const fill = $('[data-meter-fill]', root);
  if (fill) fill.style.width = `${pct}%`;
  const txt = $('[data-meter-text]', root);
  if (txt)
    txt.innerHTML =
      mustDone === mustCount
        ? `${svgIcon('star')} <strong>Opening Day ready!</strong> ${esc(plan.answers.name || 'Your rookie')} has every must-have. Play ball!`
        : `<strong>${mustDone} of ${mustCount}</strong> must-haves checked off. Check them off as you buy.`;
  root.querySelector('[data-meter]')?.classList.toggle('is-done', mustDone === mustCount);
  return mustCount > 0 && mustDone === mustCount;
}

function renderResults(root: HTMLElement, plan: Plan, opts: { shared: boolean }) {
  const got = new Set(store<string[]>(KEY_GOT) ?? []);
  const disclosure = ($<HTMLTemplateElement>('#tpl-disclosure')?.innerHTML ?? '').trim();
  const titleName = plan.answers.name ? `${esc(plan.whose)} first-season` : 'Your first-season';

  const groups = GROUPS.map((g) => {
    const items = plan.items.filter((i) => (g.key === 'covered' ? i.covered : !i.covered && i.priority === g.key));
    if (!items.length) return '';
    return `<div class="group group-${g.key}">
      <div class="group-head"><p class="group-tag">${g.tag}</p><h3>${g.title} <span class="count">${items.length}</span></h3><p>${g.blurb}</p></div>
      ${items.map((i) => itemCard(i, plan, got)).join('')}
    </div>`;
  }).join('');

  const certNote = CERT_INFO[plan.cert];

  root.innerHTML = `
  ${opts.shared ? `<div class="shared-banner no-print">${svgIcon('link')}<p>Someone shared this gear plan with you. <a href="/gear-guide/" data-act="restart">Build one for your own player →</a></p></div>` : ''}
  <header class="res-hero">
    <p class="eyebrow">${svgIcon('baseball')} Safe at home! Your game plan is ready</p>
    <h1 tabindex="-1" data-res-title>${titleName} gear plan</h1>
    <ul class="chips">${plan.chips.map((c) => `<li>${esc(c)}</li>`).join('')}</ul>
    <div class="res-actions no-print">
      <button type="button" class="btn btn-red" data-act="goto-card">${svgIcon('star')} Make a rookie card</button>
      <button type="button" class="btn btn-ghost" data-act="print">${svgIcon('print')} Print / save PDF</button>
      <button type="button" class="btn btn-ghost" data-act="share">${svgIcon('link')} Share with family</button>
      <button type="button" class="btn btn-ghost" data-act="edit">${svgIcon('edit')} Edit answers</button>
      <button type="button" class="linkish" data-act="restart">Start over</button>
    </div>
  </header>
  ${disclosure}
  <div class="res-grid">
    <div class="res-main">
      <section class="card step-card" aria-labelledby="s1">
        <h2 id="s1"><span class="num">1</span> Text the coach before you buy</h2>
        <p>Leagues and teams have their own rules, and some hand out gear. Two minutes now can save you a return trip. Here’s a message you can copy and send:</p>
        <pre class="coach-msg" data-coach-msg>${esc(plan.coachMessage)}</pre>
        <button type="button" class="btn no-print" data-act="copy-coach">${svgIcon('copy')} Copy message</button>
      </section>

      <section class="card step-card" aria-labelledby="s2">
        <h2 id="s2"><span class="num">2</span> ${plan.answers.name ? esc(plan.whose) : 'Your'} size cheat sheet</h2>
        <p>Screenshot this or print it for the store.</p>
        <div class="table-wrap"><table class="cheat">
          <tbody>${plan.cheatSheet
            .map((s) => `<tr><th scope="row">${esc(s.label)}</th><td><strong>${esc(s.value)}</strong>${s.note ? `<small>${esc(s.note)}</small>` : ''}</td></tr>`)
            .join('')}</tbody>
        </table></div>
        <div class="callout callout-warn"><p><strong>The bat stamp matters most.</strong> ${esc(certNote.plain)}</p></div>
        ${plan.estimatedBody ? `<p class="muted">${svgIcon('info')} Sizes are based on typical measurements for age ${plan.answers.age}. Measuring height and weight makes the bat and pants sizes more accurate. <button type="button" class="linkish" data-act="edit-size">Add measurements</button></p>` : ''}
      </section>

      <section class="step-card gear-list" aria-labelledby="s3">
        <h2 id="s3"><span class="num">3</span> The gear list</h2>
        <div class="meter no-print" data-meter>
          <div class="meter-bar"><span data-meter-fill></span></div>
          <p data-meter-text></p>
        </div>
        ${groups}
      </section>

      <section class="card step-card" aria-labelledby="s4">
        <h2 id="s4"><span class="num">4</span> Countdown to the first practice</h2>
        <ol class="timeline">${plan.timeline
          .map((t) => `<li><p class="when">${svgIcon('clock')} ${esc(t.when)}</p>${list(t.tasks)}</li>`)
          .join('')}</ol>
      </section>

      <section class="card step-card" aria-labelledby="s5">
        <h2 id="s5"><span class="num">5</span> Pack the bag</h2>
        <p>The night before, hand this one to ${esc(plan.who)}. <strong>Packing the bag is the player’s job</strong>, just like the big leaguers.</p>
        <ul class="bag-list">${plan.bag.map((b, i) => `<li><label><input type="checkbox" id="bag-${i}"><span class="bag-icon">${svgIcon(b.icon)}</span><span>${esc(b.text)}</span></label></li>`).join('')}</ul>
      </section>

      ${rookieCardSection(plan, 6)}
    </div>

    <aside class="res-side">
      <div class="card side-card budget-card">
        <h3>${svgIcon('tag')} Estimated spend</h3>
        <p class="muted">${esc(BUDGET_LABEL[plan.answers.budget])} budget</p>
        <dl class="budget">
          <div><dt>Must-haves</dt><dd>${money(plan.budget.must)}</dd></div>
          <div><dt>+ Recommended</dt><dd>${money(plan.budget.withRecommended)}</dd></div>
        </dl>
        <p class="fine">Typical price ranges for youth gear, not quotes. Items you already have are left out.</p>
        <h4>Ways to spend less</h4>
        <ul class="tips">
          <li>Ask what the team provides first. Helmets, catcher’s gear and even bats are often shared.</li>
          <li>Buy used bats, bags and catcher’s gear. Check that the bat stamp is legal and there are no cracks or dents.</li>
          <li>Spend on fit, not features: glove, helmet and cleats are where fit matters most.</li>
          <li>Late summer and fall clearance is the cheapest time to buy for next year.</li>
        </ul>
      </div>
      ${newsletterHtml(plan)}
      <div class="card side-card no-print">
        <h3>${svgIcon('info')} Learn the why</h3>
        <ul class="side-links">
          <li><a href="/guides/youth-bat-size-chart/">Bat size chart &amp; fit tests</a></li>
          <li><a href="/guides/bat-certifications-usa-usssa-bbcor/">USA vs USSSA vs BBCOR</a></li>
          <li><a href="/guides/youth-glove-size-chart/">Glove size chart</a></li>
          <li><a href="/guides/how-to-break-in-a-glove/">Break in a glove in a week</a></li>
          <li><a href="/guides/batting-helmet-size-guide/">Helmet sizing</a></li>
        </ul>
      </div>
    </aside>
  </div>`;

  updateMeter(root, plan);
  mountRookieCard(root, plan);
}

let timerHandle = 0;
function startTimer(box: HTMLElement | null) {
  if (!box) return;
  const num = $('[data-timer-num]', box)!;
  const text = $('[data-timer-text]', box)!;
  const label = $('[data-timer-label]', box)!;
  const original = text.dataset.original ?? text.textContent ?? '';
  text.dataset.original = original;
  clearInterval(timerHandle);
  let left = 20;
  box.classList.add('running');
  box.classList.remove('done');
  num.textContent = String(left);
  label.textContent = 'Restart';
  text.textContent = 'Arm straight out to the side… hold it… keep it level!';
  track('Bat Timer Started', {});
  timerHandle = window.setInterval(() => {
    left -= 1;
    num.textContent = String(left);
    if (left === 10) text.textContent = 'Halfway there! Is the barrel still level?';
    if (left === 3) text.textContent = 'Almost… 3… 2… 1…';
    if (left <= 0) {
      clearInterval(timerHandle);
      box.classList.remove('running');
      box.classList.add('done');
      num.textContent = '✓';
      text.textContent = 'Time! If the bat stayed level the whole time, it’s not too heavy. If it dipped, try one an ounce lighter.';
      label.textContent = 'Try again';
    }
  }, 1000);
}

// ---------------------------------------------------------------------------
// Controller
// ---------------------------------------------------------------------------

export function initWizard() {
  const wizard = $('#wizard')!;
  const form = $<HTMLFormElement>('#wiz-form')!;
  const results = $('#results')!;
  const preview = $('[data-preview]')!;
  const previewList = $('[data-preview-list]')!;
  const progressWrap = $('[data-progress-wrap]')!;
  const basepath = $('[data-basepath]')!;
  const trivia = $('[data-trivia]')!;
  const count = $('[data-count]')!;
  const steps = $$<HTMLFieldSetElement>('.step', form);
  let current: StepId = 'intro';
  let started = false;
  let currentPlan: Plan | null = null;

  results.addEventListener('change', (e) => {
    const input = e.target as HTMLInputElement;
    if (!input.dataset.got || !currentPlan) return;
    const ids = new Set(store<string[]>(KEY_GOT) ?? []);
    if (input.checked) ids.add(input.dataset.got);
    else ids.delete(input.dataset.got);
    store(KEY_GOT, [...ids]);
    input.closest('.item')?.classList.toggle('is-got', input.checked);
    if (updateMeter(results, currentPlan) && input.checked) {
      celebrate();
      track('Opening Day Ready', {});
    }
  });
  results.addEventListener('submit', (e) => {
    if ((e.target as HTMLElement).matches('[data-signup]')) track('Newsletter Signup', { stage: currentPlan?.stage ?? '' });
  });

  const draft = () => readDraft(form);

  const personalize = () => {
    const d = draft();
    const name = d.name;
    $$('[data-who]').forEach((el) => (el.textContent = name || 'your player'));
    $$('[data-whose]').forEach((el) => (el.textContent = name ? (/s$/i.test(name) ? `${name}’` : `${name}’s`) : 'your player’s'));
    $$('[data-whose-cap]').forEach((el) => (el.textContent = name ? (/s$/i.test(name) ? `${name}’` : `${name}’s`) : 'Your player’s'));
    $$('[data-catcher-only]').forEach((el) => (el.hidden = d.position !== 'catcher'));
  };

  const renderReaction = (field: string) => {
    const box = $(`[data-react="${field}"]`, form);
    if (!box) return;
    const text = reaction(field, draft());
    // Skip identical re-renders so nothing shifts (or re-animates) under the user's finger.
    if (box.dataset.text === text) return;
    box.dataset.text = text;
    box.innerHTML = text ? coachHtml(text) : '';
  };

  const renderPreview = () => {
    const d = draft();
    preview.hidden = current === 'intro' || d.age == null;
    previewList.innerHTML = previewRows(d)
      .map(
        (r) =>
          `<li class="${r.done ? 'done' : ''}"><span class="pl-label">${r.done ? svgIcon('check') : '<i></i>'}${esc(r.label)}</span><span class="pl-value">${esc(r.value)}</span></li>`,
      )
      .join('');
  };

  const save = () => store(KEY_DRAFT, draft());

  const show = (step: StepId, { push = true } = {}) => {
    current = step;
    wizard.hidden = false;
    results.hidden = true;
    steps.forEach((fs) => (fs.hidden = fs.dataset.step !== step));
    $$('[data-error]', form).forEach((e) => (e.hidden = true));
    const idx = STEPS.indexOf(step);
    const total = STEPS.length - 1;
    progressWrap.hidden = step === 'intro';
    const p = Math.max(0, idx - 1) / total;
    basepath.style.setProperty('--p', String(p));
    basepath.setAttribute('aria-valuenow', String(Math.round(p * 100)));
    $$('[data-base]', basepath).forEach((b) => b.classList.toggle('reached', p >= Number(b.dataset.base) / 4));
    count.innerHTML = `<span>Question ${idx} of ${total}</span> <strong>${BASE_CALLS[Math.max(0, idx - 1)] ?? ''}</strong>`;
    trivia.hidden = step === 'intro';
    if (step !== 'intro') trivia.innerHTML = `<strong>Dugout trivia:</strong> ${esc(TRIVIA[(idx - 1) % TRIVIA.length])}`;
    personalize();
    renderPreview();
    if (push) history.pushState({ rr: step }, '', location.pathname);
    const fs = steps.find((f) => f.dataset.step === step)!;
    const heading = $<HTMLElement>('.q, h1', fs);
    if (heading && step !== 'intro') {
      heading.setAttribute('tabindex', '-1');
      heading.focus({ preventScroll: true });
    }
    if (started) wizard.scrollIntoView({ block: 'start', behavior: 'smooth' });
    if (step !== 'intro') track('Wizard Step', { step });
  };

  const finish = ({ push = true, fresh = true } = {}) => {
    const answers = toAnswers(draft());
    store(KEY_DONE, true);
    showPlan(answers, { push, shared: false });
    if (fresh) setTimeout(celebrate, 150);
    track('Plan Complete', {
      age: answers.age,
      team: answers.team,
      stage: stageFor(answers.age, answers.team),
      budget: answers.budget,
      position: answers.position,
    });
  };

  const showPlan = (answers: Answers, { push = true, shared = false } = {}) => {
    const plan = buildPlan(answers);
    currentPlan = plan;
    wizard.hidden = true;
    results.hidden = false;
    renderResults(results, plan, { shared });
    const url = `${location.pathname}?${encodeAnswers(answers)}`;
    if (push) history.pushState({ rr: 'results' }, '', url);
    else history.replaceState({ rr: 'results' }, '', url);
    document.title = `${plan.answers.name ? `${plan.whose} gear plan` : 'Your gear plan'} | ${site.name}`;
    window.scrollTo({ top: 0 });
    $<HTMLElement>('[data-res-title]', results)?.focus({ preventScroll: true });
  };

  const next = () => {
    const d = draft();
    if (!validate(current, d)) {
      const err = $(`[data-error="${current}"]`, form);
      if (err) {
        err.hidden = false;
        err.setAttribute('role', 'alert');
      }
      return;
    }
    if (current === 'intro' && !started) {
      started = true;
      track('Wizard Start', {});
    }
    save();
    const idx = STEPS.indexOf(current);
    if (idx === STEPS.length - 1) finish();
    else show(STEPS[idx + 1]);
  };

  // --- Events -----------------------------------------------------------------

  form.addEventListener('submit', (e) => {
    e.preventDefault();
    next();
  });
  form.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' && (e.target as HTMLElement).tagName === 'INPUT') {
      e.preventDefault();
      next();
    }
  });
  form.addEventListener('click', (e) => {
    const t = e.target as HTMLElement;
    if (t.closest('[data-next]')) next();
    else if (t.closest('[data-back]')) history.back();
  });
  form.addEventListener('input', (e) => {
    personalize();
    // Show the size reaction while they type, so it doesn't pop in (and move the
    // Next button) when the weight field loses focus.
    if ((e.target as HTMLInputElement).name === 'weight') {
      renderReaction('size');
      renderPreview();
    }
  });
  form.addEventListener('change', (e) => {
    const name = (e.target as HTMLInputElement).name;
    if (['feet', 'inches', 'weight', 'unsureSize'].includes(name)) {
      if (name === 'unsureSize') {
        const off = (e.target as HTMLInputElement).checked;
        $$<HTMLInputElement | HTMLSelectElement>('select[name="feet"], select[name="inches"], input[name="weight"]', form).forEach((el) => (el.disabled = off));
      }
      renderReaction('size');
    } else {
      renderReaction(name);
      if (name === 'experience') renderReaction('budget');
    }
    $$('[data-error]', form).forEach((er) => (er.hidden = true));
    personalize();
    renderPreview();
    save();
  });

  // Results actions (delegated, so it survives re-renders)
  document.addEventListener('click', async (e) => {
    const btn = (e.target as HTMLElement).closest<HTMLElement>('[data-act]');
    if (!btn) return;
    const act = btn.dataset.act;
    const answers = decodeAnswers(location.search);
    if (act === 'print') {
      track('Plan Printed', {});
      window.print();
    } else if (act === 'share' && answers) {
      const url = `${location.origin}/gear-guide/?${encodeAnswers(answers)}`;
      track('Plan Shared', {});
      if (navigator.share) {
        try {
          await navigator.share({ title: `First-season baseball gear list`, text: 'Here’s the gear list (with sizes) for this season:', url });
          return;
        } catch {
          /* fall back to copy */
        }
      }
      if (await copyText(url)) toast('Link copied! It includes the sizes, but not your player’s name.');
    } else if (act === 'copy-coach') {
      const msg = $('[data-coach-msg]')?.textContent ?? '';
      if (await copyText(msg)) {
        toast('Message copied. Paste it into a text or email to the coach.');
        track('Coach Message Copied', {});
      }
    } else if (act === 'edit' || act === 'edit-size') {
      const saved = store<Draft>(KEY_DRAFT);
      if (answers) fillForm(form, { ...draftFromAnswers(answers), name: saved?.name ?? answers.name ?? '' });
      show(act === 'edit-size' ? 'size' : 'age');
      ['age', 'size', 'throws', 'bats', 'team', 'position', 'experience', 'budget'].forEach(renderReaction);
    } else if (act === 'restart') {
      e.preventDefault();
      forget(KEY_DRAFT, KEY_DONE, KEY_GOT, KEY_CARD);
      form.reset();
      fillForm(form, { have: [], unsureSize: false });
      $$<HTMLElement>('[data-react]', form).forEach((r) => {
        r.innerHTML = '';
        r.dataset.text = '';
      });
      $$<HTMLInputElement | HTMLSelectElement>('select, input[name="weight"]', form).forEach((el) => (el.disabled = false));
      $('[data-resume]', form)!.hidden = true;
      history.replaceState({ rr: 'intro' }, '', location.pathname);
      show('intro', { push: false });
    } else if (act === 'goto-card') {
      $('#rookie-card')?.scrollIntoView({ behavior: 'smooth', block: 'start' });
      $<HTMLInputElement>('#rc-number')?.focus({ preventScroll: true });
    } else if (act === 'timer') {
      startTimer(btn.closest<HTMLElement>('[data-timer]'));
    } else if (act === 'resume') {
      const d = draft();
      const firstOpen = STEPS.find((s) => s !== 'intro' && !validate(s, d)) ?? 'extras';
      started = true;
      show(firstOpen);
    }
  });

  window.addEventListener('popstate', (e) => {
    const state = e.state as { rr?: string } | null;
    if (state?.rr === 'results') {
      const a = decodeAnswers(location.search);
      if (a) showPlan({ ...a, name: store<Draft>(KEY_DRAFT)?.name }, { push: false });
      return;
    }
    const step = (STEPS as readonly string[]).includes(state?.rr ?? '') ? (state!.rr as StepId) : 'intro';
    show(step, { push: false });
  });

  // --- Boot -----------------------------------------------------------------

  const saved = store<Draft>(KEY_DRAFT);
  const fromUrl = decodeAnswers(location.search);

  if (fromUrl) {
    const mine = saved && encodeAnswers(toAnswers(saved)) === encodeAnswers(fromUrl);
    if (mine && saved) fillForm(form, saved);
    else fillForm(form, draftFromAnswers(fromUrl));
    showPlan({ ...fromUrl, name: mine ? saved?.name : undefined }, { push: false, shared: !mine });
    if (!mine) track('Shared Plan Viewed', {});
    return;
  }

  if (saved) {
    fillForm(form, saved);
    if (saved.unsureSize) {
      $$<HTMLInputElement | HTMLSelectElement>('select[name="feet"], select[name="inches"], input[name="weight"]', form).forEach((el) => (el.disabled = true));
    }
    ['age', 'size', 'throws', 'bats', 'team', 'position', 'experience', 'budget'].forEach(renderReaction);
    if (store<boolean>(KEY_DONE)) {
      finish({ push: false, fresh: false });
      return;
    }
    if (saved.age != null) $('[data-resume]', form)!.hidden = false;
  }

  history.replaceState({ rr: 'intro' }, '', location.pathname);
  show('intro', { push: false });
}
