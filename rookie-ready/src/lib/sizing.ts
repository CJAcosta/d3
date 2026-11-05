/**
 * The sizing engine. Pure functions, no DOM, shared by the wizard (in the
 * browser) and the guide pages (at build time) so the numbers always match.
 *
 * Sources (checked on the date in site.rulesReviewed):
 *  - Little League bat rules: https://www.littleleague.org/playing-rules-/bat-rules
 *  - Little League cleat rule 1.11(h): https://www.littleleague.org/help-center/are-cleats-with-metal-spikes-permitted/
 *  - Little League practice gear: https://www.littleleague.org/university/articles/get-the-gear-and-what-to-wear-for-little-league-practices/
 *  - Rawlings glove size by age: https://www.rawlings.com/product/37GG.html
 *  - Industry height/weight bat chart used by most bat makers and retailers.
 * Every chart is a starting point; fit checks beat charts.
 */
import type { Answers, BatCert, BatSide, Hand, Position, Stage, Team } from './types';

// ---------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------

const FRACTIONS: Record<number, string> = {
  0.125: '⅛',
  0.25: '¼',
  0.375: '⅜',
  0.5: '½',
  0.625: '⅝',
  0.75: '¾',
  0.875: '⅞',
};

/** 10.75 -> "10¾", 6.625 -> "6⅝", 28 -> "28". */
export function frac(n: number): string {
  const whole = Math.floor(n);
  const rest = Math.round((n - whole) * 8) / 8;
  if (rest === 0) return String(whole);
  if (rest === 1) return String(whole + 1);
  return `${whole}${FRACTIONS[rest] ?? ''}`;
}

/** 50 -> "4′2″". */
export function feetInches(totalIn: number): string {
  const ft = Math.floor(totalIn / 12);
  const inch = Math.round(totalIn - ft * 12);
  return inch === 12 ? `${ft + 1}′0″` : `${ft}′${inch}″`;
}

export function roundTo(n: number, step: number): number {
  return Math.round(n / step) * step;
}

const clamp = (n: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, n));

// ---------------------------------------------------------------------------
// Growth estimates (used only when a parent skips height/weight/head size)
// Approximate 50th percentile, blended boys/girls.
// ---------------------------------------------------------------------------

const GROWTH: Record<number, { h: number; w: number; head: number }> = {
  4: { h: 40, w: 36, head: 19.8 },
  5: { h: 43, w: 41, head: 20.0 },
  6: { h: 45.5, w: 46, head: 20.2 },
  7: { h: 48, w: 51, head: 20.4 },
  8: { h: 50.5, w: 57, head: 20.5 },
  9: { h: 52.5, w: 63, head: 20.7 },
  10: { h: 54.5, w: 70, head: 20.8 },
  11: { h: 56.5, w: 79, head: 21.0 },
  12: { h: 59, w: 89, head: 21.1 },
  13: { h: 61.5, w: 100, head: 21.3 },
  14: { h: 64, w: 112, head: 21.5 },
  15: { h: 66, w: 123, head: 21.7 },
  16: { h: 67, w: 132, head: 21.8 },
  17: { h: 68, w: 140, head: 21.9 },
  18: { h: 68.5, w: 145, head: 22.0 },
};

export const MIN_AGE = 4;
export const MAX_AGE = 18;

export function typicalForAge(age: number) {
  return GROWTH[clamp(Math.round(age), MIN_AGE, MAX_AGE)];
}

/** Height and weight to size with, plus whether we had to guess. */
export function bodyFor(a: Pick<Answers, 'age' | 'heightIn' | 'weightLb'>) {
  const t = typicalForAge(a.age);
  return {
    heightIn: a.heightIn ?? t.h,
    weightLb: a.weightLb ?? t.w,
    estimated: a.heightIn == null || a.weightLb == null,
  };
}

// ---------------------------------------------------------------------------
// Stage and league rules
// ---------------------------------------------------------------------------

export function stageFor(age: number, team: Team): Stage {
  if (team === 'teeball') return 'teeball';
  if (team === 'school') return age >= 14 ? 'highschool' : 'teen';
  if (team === 'travel') {
    if (age <= 7) return 'coachpitch';
    if (age <= 12) return 'kidpitch';
    return age <= 14 ? 'teen' : 'highschool';
  }
  if (age <= 5) return 'teeball';
  if (age <= 8) return 'coachpitch';
  if (age <= 12) return 'kidpitch';
  return age <= 14 ? 'teen' : 'highschool';
}

export const STAGE_LABEL: Record<Stage, string> = {
  teeball: 'Tee ball',
  coachpitch: 'Coach / machine pitch',
  kidpitch: 'Kid pitch',
  teen: '13U–14U',
  highschool: 'High school age',
};

export function batCertFor(age: number, team: Team, stage: Stage): BatCert {
  if (stage === 'teeball') return 'usa-teeball';
  if (team === 'school') return stage === 'highschool' ? 'bbcor' : 'usa-or-bbcor';
  if (team === 'travel') return age >= 15 ? 'bbcor' : 'usssa';
  // Little League / rec / not sure
  if (age >= 15) return 'bbcor';
  if (age >= 13) return 'usa-or-bbcor';
  return 'usa';
}

export const CERT_INFO: Record<
  BatCert,
  { label: string; stamp: string; plain: string; query: string }
> = {
  'usa-teeball': {
    label: 'USA Baseball tee ball bat',
    stamp: 'USA Baseball stamp + “Only for use with approved tee balls”',
    plain:
      'Tee ball bats are short, light and made for soft tee balls. Little League tee ball bats must carry the USA Baseball mark and be 26″ or shorter.',
    query: 'USA Baseball tee ball bat',
  },
  usa: {
    label: 'USA Baseball bat',
    stamp: 'USA Baseball stamp (the “USABat” logo)',
    plain:
      'Little League and most rec leagues require the USA Baseball stamp. A USSSA bat, even the same size, is not legal there.',
    query: 'USA Baseball bat',
  },
  usssa: {
    label: 'USSSA bat',
    stamp: 'USSSA “1.15 BPF” stamp',
    plain:
      'Most travel and tournament teams play USSSA rules, and USSSA bats hit hotter. Some travel events use USA Baseball rules instead, so confirm with the coach.',
    query: 'USSSA baseball bat',
  },
  'usa-or-bbcor': {
    label: 'USA Baseball or BBCOR bat',
    stamp: 'USA Baseball stamp, or BBCOR (-3) if they are moving toward high school ball',
    plain:
      'At 13–14, Little League’s Intermediate and Junior divisions allow USA Baseball or BBCOR bats. Many players switch to BBCOR here to get ready for high school.',
    query: 'USA Baseball bat',
  },
  bbcor: {
    label: 'BBCOR bat',
    stamp: 'BBCOR .50 certification mark (always a -3 drop)',
    plain:
      'High school (and Little League Senior division) bats must be BBCOR certified. They are heavier (-3) and hit closer to wood.',
    query: 'BBCOR bat',
  },
};

// ---------------------------------------------------------------------------
// Bats
// ---------------------------------------------------------------------------

/** Column boundaries (inclusive upper bound, inches) for the height/weight chart. */
export const BAT_HEIGHT_COLS = [40, 44, 48, 52, 56, 60, 64, 68, 72, Infinity];
export const BAT_HEIGHT_LABELS = [
  '3′–3′4″',
  '3′5″–3′8″',
  '3′9″–4′',
  '4′1″–4′4″',
  '4′5″–4′8″',
  '4′9″–5′',
  '5′1″–5′4″',
  '5′5″–5′8″',
  '5′9″–6′',
  '6′1″+',
];
/** Row boundaries (inclusive upper bound, pounds). */
export const BAT_WEIGHT_ROWS = [60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, Infinity];
export const BAT_WEIGHT_LABELS = [
  'Under 60',
  '61–70',
  '71–80',
  '81–90',
  '91–100',
  '101–110',
  '111–120',
  '121–130',
  '131–140',
  '141–150',
  '151–160',
  '161–170',
  '170+',
];
const _ = null;
/** The widely published industry bat-length chart (inches). null = no chart value. */
export const BAT_CHART: (number | null)[][] = [
  [26, 27, 28, 29, 29, _, _, _, _, _],
  [27, 27, 28, 29, 30, 30, _, _, _, _],
  [_, 28, 28, 29, 30, 30, 31, _, _, _],
  [_, 28, 29, 29, 30, 30, 31, 32, _, _],
  [_, 28, 29, 30, 30, 31, 31, 32, _, _],
  [_, 29, 29, 30, 30, 31, 31, 32, _, _],
  [_, 29, 29, 30, 30, 31, 31, 32, _, _],
  [_, 29, 30, 30, 30, 31, 32, 33, 33, _],
  [_, 29, 30, 30, 31, 31, 32, 33, 33, _],
  [_, _, 30, 30, 31, 31, 32, 33, 33, _],
  [_, _, 30, 31, 31, 32, 32, 33, 33, 33],
  [_, _, _, 31, 31, 32, 32, 33, 33, 34],
  [_, _, _, _, 32, 32, 33, 33, 34, 34],
];

/** Typical [shortest, longest] bat length by age, used as a sanity check on the chart. */
export const AGE_BAT_RANGE: Record<number, [number, number]> = {
  4: [24, 26], 5: [24, 26], 6: [25, 27], 7: [26, 28], 8: [26, 29], 9: [27, 30], 10: [28, 30],
  11: [28, 31], 12: [29, 32], 13: [29, 32], 14: [30, 33], 15: [30, 34], 16: [31, 34], 17: [31, 34], 18: [31, 34],
};

const indexFor = (bounds: number[], v: number) => {
  const i = bounds.findIndex((b) => v <= b);
  return i === -1 ? bounds.length - 1 : i;
};

/** Chart length for a height and weight. Fills chart gaps from the nearest cell in the same height column. */
export function chartBatLength(heightIn: number, weightLb: number): number {
  const col = indexFor(BAT_HEIGHT_COLS, heightIn);
  const row = indexFor(BAT_WEIGHT_ROWS, weightLb);
  const direct = BAT_CHART[row][col];
  if (direct != null) return direct;
  for (let d = 1; d < BAT_CHART.length; d++) {
    for (const r of [row - d, row + d]) {
      const v = BAT_CHART[r]?.[col];
      if (v != null) return v;
    }
  }
  return 30;
}

export interface BatRec {
  cert: BatCert;
  /** Best single length. */
  length: number;
  /** Lengths that are also reasonable (sorted). */
  lengths: number[];
  /** Drop range, as negative numbers, lightest first, e.g. [-12, -10]. */
  drop: [number, number];
  /** Drop to start with. */
  startDrop: number;
  maxLength: number;
  notes: string[];
}

export function batFor(a: Answers): BatRec {
  const stage = stageFor(a.age, a.team);
  const cert = batCertFor(a.age, a.team, stage);
  const { heightIn, weightLb } = bodyFor(a);
  const first = a.experience === 'first';
  const notes: string[] = [];

  let length: number;
  let maxLength: number;
  let drop: [number, number];
  let startDrop: number;

  if (stage === 'teeball') {
    length = heightIn <= 40 ? 24 : heightIn <= 44 ? 25 : 26;
    maxLength = 26;
    drop = [-13, -10];
    startDrop = -12;
    notes.push('Tee ball bats are only for soft tee balls. Once they face real baseballs, they need a regular USA Baseball bat.');
  } else {
    const chart = chartBatLength(heightIn, weightLb);
    const [lo, hi] =
      stage === 'coachpitch' ? [25, 29] : stage === 'kidpitch' ? [26, 33] : stage === 'teen' ? [28, 34] : [29, 34];
    // The height/weight chart runs long for small kids, so cap it with an age-based ceiling.
    const [ageLo, ageHi] = AGE_BAT_RANGE[clamp(Math.round(a.age), MIN_AGE, MAX_AGE)];
    length = clamp(chart, Math.max(lo, ageLo), Math.min(hi, ageHi));
    if (chart > ageHi) notes.push(`The height chart says ${chart}″, but that’s long for a ${a.age}-year-old, so we’ve capped it at ${ageHi}″.`);
    maxLength = stage === 'kidpitch' || stage === 'coachpitch' ? 33 : stage === 'teen' ? 34 : 34;

    if (cert === 'bbcor') {
      drop = [-3, -3];
      startDrop = -3;
    } else if (a.age <= 8) {
      drop = [-13, -10];
      startDrop = -11;
    } else if (a.age <= 10) {
      drop = [-12, -10];
      startDrop = first ? -11 : -10;
    } else if (a.age <= 12) {
      drop = [-10, -8];
      startDrop = first ? -10 : -8;
    } else {
      drop = [-8, -5];
      startDrop = first ? -8 : -5;
    }
    if (stage === 'kidpitch' && length > 33) length = 33;
  }

  const lengths = first && length > (stage === 'teeball' ? 24 : 26) ? [length - 1, length] : [length];
  if (first && lengths.length > 1) {
    notes.push(`New players do better with a bat they can control. If ${length}″ feels heavy in the store, ${length - 1}″ is the right call.`);
  }
  if (cert === 'usa-or-bbcor') {
    notes.push('Heading to high school ball soon? A BBCOR bat now means no surprise switch later.');
  }
  return { cert, length, lengths, drop, startDrop, maxLength, notes };
}

export function batWeightOz(length: number, drop: number): number {
  return length + drop;
}

export function dropLabel(d: number): string {
  return `−${Math.abs(d)}`;
}

// ---------------------------------------------------------------------------
// Gloves
// ---------------------------------------------------------------------------

type GloveSpot = 'infield' | 'outfield' | 'allaround' | 'pitcher';

/** [min, max] inches by age band and spot. Based on the Rawlings age chart. */
const GLOVE_TABLE: { maxAge: number; ranges: Record<GloveSpot, [number, number]> }[] = [
  { maxAge: 6, ranges: { infield: [9, 10.5], outfield: [9, 10.5], allaround: [9, 10.5], pitcher: [9, 10.5] } },
  { maxAge: 8, ranges: { infield: [10.25, 10.75], outfield: [10.75, 11.5], allaround: [10.5, 11], pitcher: [10.5, 11] } },
  { maxAge: 10, ranges: { infield: [10.75, 11.25], outfield: [11.25, 12], allaround: [11, 11.5], pitcher: [11, 11.5] } },
  { maxAge: 12, ranges: { infield: [11, 11.5], outfield: [11.75, 12.25], allaround: [11.25, 11.75], pitcher: [11.25, 11.75] } },
  { maxAge: 14, ranges: { infield: [11.25, 11.75], outfield: [12, 12.5], allaround: [11.5, 12], pitcher: [11.5, 12] } },
  { maxAge: 99, ranges: { infield: [11.25, 12], outfield: [12, 12.75], allaround: [11.75, 12.25], pitcher: [11.75, 12] } },
];

export const GLOVE_CHART_ROWS = [
  { ages: '4–6 (tee ball)', ...GLOVE_TABLE[0].ranges },
  { ages: '7–8', ...GLOVE_TABLE[1].ranges },
  { ages: '9–10', ...GLOVE_TABLE[2].ranges },
  { ages: '11–12', ...GLOVE_TABLE[3].ranges },
  { ages: '13–14', ...GLOVE_TABLE[4].ranges },
  { ages: '15+', ...GLOVE_TABLE[5].ranges },
];

const CATCHER_MITT: { maxAge: number; range: [number, number] }[] = [
  { maxAge: 8, range: [29.5, 31] },
  { maxAge: 10, range: [30, 31.5] },
  { maxAge: 12, range: [31, 32.5] },
  { maxAge: 14, range: [31.5, 33] },
  { maxAge: 99, range: [32.5, 34] },
];

const FIRST_BASE_MITT: { maxAge: number; range: [number, number] }[] = [
  { maxAge: 12, range: [11.5, 12] },
  { maxAge: 99, range: [12, 13] },
];

export const MITT_CHART_ROWS = [
  { ages: '7–8', catcher: CATCHER_MITT[0].range, firstbase: null },
  { ages: '9–10', catcher: CATCHER_MITT[1].range, firstbase: FIRST_BASE_MITT[0].range },
  { ages: '11–12', catcher: CATCHER_MITT[2].range, firstbase: FIRST_BASE_MITT[0].range },
  { ages: '13–14', catcher: CATCHER_MITT[3].range, firstbase: FIRST_BASE_MITT[1].range },
  { ages: '15+', catcher: CATCHER_MITT[4].range, firstbase: FIRST_BASE_MITT[1].range },
];

export interface HandInfo {
  code: 'RHT' | 'LHT' | null;
  label: string;
  wornOn: string;
  query: string;
}

export function handFor(throws: Hand): HandInfo {
  if (throws === 'R')
    return { code: 'RHT', label: 'Right-Hand Throw (RHT)', wornOn: 'worn on the left hand', query: 'right hand throw' };
  if (throws === 'L')
    return { code: 'LHT', label: 'Left-Hand Throw (LHT)', wornOn: 'worn on the right hand', query: 'left hand throw' };
  return {
    code: null,
    label: 'Throwing hand to confirm',
    wornOn: 'the glove goes on the hand they do NOT throw with',
    query: '',
  };
}

export interface GloveRec {
  kind: 'fielder' | 'catcher' | 'firstbase';
  range: [number, number];
  size: number;
  spot: GloveSpot;
  web: string;
  hand: HandInfo;
  notes: string[];
}

function spotFor(position: Position, age: number): GloveSpot {
  if (age <= 8) return 'allaround';
  if (position === 'infield') return 'infield';
  if (position === 'outfield') return 'outfield';
  if (position === 'pitcher') return 'pitcher';
  return 'allaround';
}

export function gloveFor(a: Answers): GloveRec {
  const spot = spotFor(a.position, a.age);
  const band = GLOVE_TABLE.find((b) => a.age <= b.maxAge)!;
  const range = band.ranges[spot];
  const first = a.experience === 'first';
  const notes: string[] = [];
  let size: number;
  if (a.age <= 6) {
    size = a.age <= 4 ? 9 : a.age === 5 ? 9.5 : 10;
  } else {
    const mid = (range[0] + range[1]) / 2;
    size = roundTo(first ? Math.min(mid, range[0] + 0.25) : mid, 0.25);
    size = clamp(size, range[0], range[1]);
  }
  const web =
    spot === 'pitcher'
      ? 'Closed web, so batters can’t see their grip'
      : spot === 'infield'
        ? 'I-web or H-web with a shallow pocket for quick transfers'
        : spot === 'outfield'
          ? 'H-web or trapeze with a deep pocket for fly balls'
          : 'Any web works. A basket or closed web is a bit more forgiving for beginners';
  if (a.age <= 10) notes.push('Young players rotate positions, so an all-around glove is the right first glove.');
  if (first) notes.push('Bigger is not better. A glove they can squeeze shut with one hand catches more balls.');
  if (a.position === 'catcher' && a.age >= 7)
    notes.push('They’ll also want a regular glove for innings when they are not catching.');
  return { kind: 'fielder', range, size, spot, web, hand: handFor(a.throws), notes };
}

export function catchersMittFor(a: Answers): GloveRec {
  const range = CATCHER_MITT.find((b) => a.age <= b.maxAge)!.range;
  const size = roundTo((range[0] + range[1]) / 2, 0.5);
  return {
    kind: 'catcher',
    range,
    size,
    spot: 'allaround',
    web: 'Catcher’s mitts are measured around the circumference, not tip to heel',
    hand: handFor(a.throws),
    notes: ['Look for a “youth” or “youth pattern” mitt. Adult mitts are too stiff for small hands.'],
  };
}

export function firstBaseMittFor(a: Answers): GloveRec {
  const range = FIRST_BASE_MITT.find((b) => a.age <= b.maxAge)!.range;
  return {
    kind: 'firstbase',
    range,
    size: range[0],
    spot: 'allaround',
    web: 'Long, scoop-shaped mitt with no individual fingers',
    hand: handFor(a.throws),
    notes: ['Only worth it if they play first base most games. Otherwise their regular glove is fine.'],
  };
}

export function gloveSizeLabel(g: Pick<GloveRec, 'range' | 'size'>): string {
  return `${frac(g.size)}″`;
}

export function rangeLabel([lo, hi]: [number, number]): string {
  return lo === hi ? `${frac(lo)}″` : `${frac(lo)}–${frac(hi)}″`;
}

// ---------------------------------------------------------------------------
// Helmets
// ---------------------------------------------------------------------------

export interface HelmetRec {
  circIn: number;
  hat: number;
  estimated: boolean;
  category: string;
  faceGuard: string;
  faceGuardQuery: string;
}

/** Head circumference (in) -> fitted hat size, rounded to the nearest 1/8. */
export function hatSize(circIn: number): number {
  return roundTo(circIn / Math.PI, 0.125);
}

export function helmetCategory(hat: number): string {
  if (hat < 6.25) return 'Tee ball / smallest youth size';
  if (hat <= 6.875) return 'Junior (youth) size';
  if (hat <= 7.125) return 'Junior or Senior. Sizes overlap here, so check the brand’s chart';
  return 'Senior size';
}

export function faceGuardFor(bats: BatSide, age: number): { text: string; query: string } {
  if (bats === 'R')
    return {
      text: age <= 8
        ? 'Full face guard (cage). If you add a C-flap jaw guard instead, it goes on the left side, labeled “for right-handed batters.”'
        : 'C-flap jaw guard on the left side (sold as “for right-handed batters”), or a full face guard',
      query: 'C-flap right handed batter',
    };
  if (bats === 'L')
    return {
      text: age <= 8
        ? 'Full face guard (cage). If you add a C-flap jaw guard instead, it goes on the right side, labeled “for left-handed batters.”'
        : 'C-flap jaw guard on the right side (sold as “for left-handed batters”), or a full face guard',
      query: 'C-flap left handed batter',
    };
  return {
    text: 'Full face guard (cage). It protects both sides, so it works whichever way they end up batting.',
    query: 'face guard',
  };
}

export function helmetFor(a: Answers): HelmetRec {
  const estimated = a.headIn == null;
  const circIn = a.headIn ?? typicalForAge(a.age).head;
  const hat = hatSize(circIn);
  const fg = faceGuardFor(a.bats, a.age);
  return {
    circIn,
    hat,
    estimated,
    category: helmetCategory(hat),
    faceGuard: fg.text,
    faceGuardQuery: fg.query,
  };
}

export const HAT_CHART = [6, 6.125, 6.25, 6.375, 6.5, 6.625, 6.75, 6.875, 7, 7.125, 7.25, 7.375, 7.5].map((hat) => ({
  hat,
  circ: Math.round(hat * Math.PI * 8) / 8,
  category: helmetCategory(hat),
}));

// ---------------------------------------------------------------------------
// Cleats, pants, batting gloves, protection, catcher's gear
// ---------------------------------------------------------------------------

export function cleatsFor(a: Answers): { type: string; query: string; notes: string[] } {
  const stage = stageFor(a.age, a.team);
  const notes: string[] = [];
  if (stage === 'teeball') {
    notes.push('Sneakers are fine for tee ball. Molded cleats help on wet grass and make them feel like a real ballplayer.');
    return { type: 'Molded (rubber) cleats, or sneakers', query: 'kids molded baseball cleats', notes };
  }
  if (a.age <= 12) {
    notes.push('Little League (Majors and below) allows molded cleats only. No metal.');
    notes.push('Molded soccer cleats are allowed in most youth leagues, but baseball cleats add a toe cleat for push-off.');
    return { type: 'Molded (rubber) baseball cleats', query: 'youth molded baseball cleats', notes };
  }
  notes.push('Metal spikes are allowed from 13U up (Little League Intermediate/Junior/Senior, high school). Molded cleats are still fine, and better on turf.');
  return { type: 'Molded or metal baseball cleats (metal allowed at 13+)', query: 'baseball cleats', notes };
}

export const PANTS_SIZES = ['Youth XS', 'Youth S', 'Youth M', 'Youth L', 'Youth XL', 'Adult S', 'Adult M'] as const;

/** Rough typical weight for a height, used to decide whether to size pants up. */
function typicalWeightForHeight(h: number): number {
  const pts: [number, number][] = [
    [40, 35], [44, 43], [48, 52], [52, 63], [56, 78], [60, 95], [64, 115], [68, 140], [72, 165],
  ];
  if (h <= pts[0][0]) return pts[0][1];
  for (let i = 1; i < pts.length; i++) {
    const [h1, w1] = pts[i];
    const [h0, w0] = pts[i - 1];
    if (h <= h1) return w0 + ((h - h0) / (h1 - h0)) * (w1 - w0);
  }
  return pts[pts.length - 1][1];
}

export function pantsFor(a: Answers): { size: string; index: number } {
  const { heightIn, weightLb } = bodyFor(a);
  let i = heightIn < 45 ? 0 : heightIn < 51 ? 1 : heightIn < 57 ? 2 : heightIn < 62 ? 3 : heightIn < 66 ? 4 : heightIn < 69 ? 5 : 6;
  if (weightLb > typicalWeightForHeight(heightIn) * 1.2) i = Math.min(i + 1, PANTS_SIZES.length - 1);
  return { size: PANTS_SIZES[i], index: i };
}

export function battingGloveSizeFor(age: number): string {
  if (age <= 5) return 'Youth XS';
  if (age <= 7) return 'Youth S';
  if (age <= 10) return 'Youth M';
  if (age <= 12) return 'Youth L';
  if (age <= 14) return 'Youth XL or Adult S';
  return 'Adult S–M';
}

export function cupSizeFor(age: number): string {
  if (age <= 6) return 'Peewee / smallest youth';
  if (age <= 12) return 'Youth';
  return 'Adult (or Youth if they are on the smaller side)';
}

export function catchersSetFor(age: number): { set: string; chest: string; legs: string } {
  if (age <= 8) return { set: 'Ages 7–9', chest: 'about 12½–13″', legs: 'about 12½″' };
  if (age <= 11) return { set: 'Ages 9–12', chest: 'about 13–14½″', legs: 'about 13½″' };
  if (age <= 15) return { set: 'Ages 12–16', chest: 'about 15–15½″', legs: 'about 14½–15″' };
  return { set: 'Adult', chest: 'about 15½–16½″', legs: 'about 15½–16½″' };
}

export const CATCHERS_CHART = [
  { set: 'Ages 7–9', chest: '12½–13″', legs: '12½″' },
  { set: 'Ages 9–12', chest: '13–14½″', legs: '13½″' },
  { set: 'Ages 12–16', chest: '15–15½″', legs: '14½–15″' },
  { set: 'Adult (16+)', chest: '15½–16½″', legs: '15½–16½″' },
];

export function teamLabel(team: Team): string {
  return {
    teeball: 'Tee ball',
    rec: 'Little League / rec league',
    travel: 'Travel / tournament',
    school: 'School team',
    unsure: 'League not decided yet',
  }[team];
}
