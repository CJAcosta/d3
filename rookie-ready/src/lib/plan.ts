/**
 * Turns wizard answers into a personalized gear plan. Pure and deterministic,
 * so it runs in the browser and in unit tests.
 */
import { gear, type GearInfo } from '../data/gear';
import { picks } from '../data/picks';
import { productLink, shopLinks, type ShopLink } from './links';
import {
  CERT_INFO,
  STAGE_LABEL,
  batFor,
  batCertFor,
  battingGloveSizeFor,
  bodyFor,
  catchersMittFor,
  catchersSetFor,
  cleatsFor,
  cupSizeFor,
  dropLabel,
  feetInches,
  firstBaseMittFor,
  frac,
  gloveFor,
  helmetFor,
  pantsFor,
  rangeLabel,
  stageFor,
  teamLabel,
} from './sizing';
import type { Answers, BatCert, Budget, GearId, Priority, Stage } from './types';

export interface Spec {
  label: string;
  value: string;
  note?: string;
}

export interface PlanPick {
  name: string;
  blurb: string;
  links: ShopLink[];
}

export interface PlanItem {
  id: GearId;
  info: GearInfo;
  priority: Priority;
  /** Already owned or provided by the team. */
  covered: boolean;
  headline: string;
  specs: Spec[];
  why: string;
  notes: string[];
  /** Typical price range for the chosen budget tier (USD). */
  price: [number, number];
  query: string;
  links: ShopLink[];
  picks: PlanPick[];
}

export interface TimelineStep {
  when: string;
  tasks: string[];
}

export interface Plan {
  answers: Answers;
  /** The kid's name or "your player". */
  who: string;
  /** Possessive form: "Sam’s" or "your player’s". */
  whose: string;
  stage: Stage;
  stageLabel: string;
  cert: BatCert;
  estimatedBody: boolean;
  chips: string[];
  items: PlanItem[];
  cheatSheet: Spec[];
  budget: { must: [number, number]; withRecommended: [number, number] };
  coachMessage: string;
  timeline: TimelineStep[];
  /** Night-before checklist; icons make it easy for kids to check off themselves. */
  bag: { text: string; icon: string }[];
}

export const PRIORITY_LABEL: Record<Priority, string> = {
  must: 'Must-have',
  recommended: 'Recommended',
  nice: 'Nice to have',
};

export function cleanName(name?: string): string {
  return (name ?? '').replace(/[^\p{L}\p{M}' .-]/gu, '').trim().slice(0, 24);
}

function possessive(name: string): string {
  return /s$/i.test(name) ? `${name}’` : `${name}’s`;
}

/** Bats vary in price far more by certification than other gear does. */
const BAT_PRICE: Record<BatCert, Record<Budget, [number, number]>> = {
  'usa-teeball': { lean: [20, 35], balanced: [30, 50], allin: [45, 80] },
  usa: { lean: [30, 60], balanced: [60, 150], allin: [150, 350] },
  usssa: { lean: [50, 100], balanced: [100, 250], allin: [250, 450] },
  'usa-or-bbcor': { lean: [40, 90], balanced: [90, 200], allin: [200, 400] },
  bbcor: { lean: [60, 120], balanced: [120, 250], allin: [250, 500] },
};

/** Inch sizes as plain decimals for store search boxes (10.75, not 10¾). */
const dec = (n: number) => String(n);

export function buildPlan(answers: Answers): Plan {
  const a: Answers = { ...answers, name: cleanName(answers.name) };
  const who = a.name || 'your player';
  const whose = a.name ? possessive(a.name) : 'your player’s';
  const stage = stageFor(a.age, a.team);
  const cert = batCertFor(a.age, a.team, stage);
  const body = bodyFor(a);
  const have = new Set(a.have);
  const first = a.experience === 'first';
  const items: PlanItem[] = [];

  const add = (
    id: GearId,
    priority: Priority,
    headline: string,
    specs: Spec[],
    query: string,
    opts: { why?: string; notes?: string[]; covered?: boolean; price?: [number, number] } = {},
  ) => {
    const info = gear[id];
    items.push({
      id,
      info,
      priority,
      covered: opts.covered ?? false,
      headline,
      specs,
      why: opts.why ?? info.why,
      notes: opts.notes ?? [],
      price: opts.price ?? info.price[a.budget],
      query,
      links: shopLinks(query),
      picks: picks
        .filter((p) => p.gear === id && (!p.when || p.when(a)))
        .map((p) => ({
          name: p.name,
          blurb: p.blurb,
          links: p.links.map((l) => productLink(l.merchant, l.url)).filter((l): l is ShopLink => l !== null),
        })),
    });
  };

  // --- Glove -----------------------------------------------------------------
  const glove = gloveFor(a);
  const handCode = glove.hand.code;
  add(
    'glove',
    'must',
    `${frac(glove.size)}″ ${handCode ?? ''} glove`.replace(/\s+/g, ' ').trim(),
    [
      { label: 'Size', value: `${frac(glove.size)}″`, note: `Good range for ${who}: ${rangeLabel(glove.range)}` },
      { label: 'Hand', value: glove.hand.label, note: glove.hand.wornOn },
      { label: 'Web', value: glove.web },
    ],
    `${stage === 'teeball' ? 'tee ball' : 'youth baseball'} glove ${dec(glove.size)} inch ${glove.hand.query}`.trim(),
    {
      covered: have.has('glove'),
      notes: [
        ...(a.throws === 'unsure'
          ? ['Not sure which hand? Roll them a ball and say “throw it back!” The hand they throw with is their throwing hand. The glove goes on the other one.']
          : []),
        ...glove.notes,
      ],
    },
  );

  // --- Bat -------------------------------------------------------------------
  const bat = batFor(a);
  const certInfo = CERT_INFO[bat.cert];
  const lenText = bat.lengths.length > 1 ? `${bat.lengths[0]}–${bat.lengths[1]}″` : `${bat.length}″`;
  const ozLo = bat.lengths[0] + bat.startDrop;
  const ozHi = bat.lengths[bat.lengths.length - 1] + bat.startDrop;
  const dropText =
    bat.drop[0] === bat.drop[1] ? dropLabel(bat.drop[0]) : `${dropLabel(bat.startDrop)} (anywhere from ${dropLabel(bat.drop[0])} to ${dropLabel(bat.drop[1])} works)`;
  add(
    'bat',
    stage === 'teeball' ? 'recommended' : 'must',
    `${lenText} ${certInfo.label}, ${dropLabel(bat.startDrop)} drop`,
    [
      { label: 'Length', value: lenText, note: `League max: ${bat.maxLength}″` },
      { label: 'Drop', value: dropText, note: `About ${ozLo === ozHi ? ozLo : `${ozLo}–${ozHi}`} oz. Bigger drop number = lighter bat.` },
      { label: 'Stamp', value: certInfo.stamp, note: certInfo.plain },
    ],
    `${certInfo.query} ${bat.length} inch${bat.cert === 'bbcor' ? '' : ` ${bat.startDrop}`}`,
    {
      covered: have.has('bat'),
      price: BAT_PRICE[bat.cert][a.budget],
      why:
        stage === 'teeball'
          ? `Most tee ball teams share a few bats, but a short, light bat of ${whose} own makes contact a lot easier, and hitting the ball is the best part of tee ball.`
          : undefined,
      notes: [
        ...(a.team === 'unsure' ? ['You haven’t picked a league yet, so we’ve assumed Little League/rec rules (USA Baseball). Confirm before you buy.'] : []),
        ...bat.notes,
      ],
    },
  );

  // --- Helmet ----------------------------------------------------------------
  const helmet = helmetFor(a);
  add(
    'helmet',
    'must',
    `${helmet.category.split('.')[0]} helmet with face guard`,
    [
      {
        label: 'Head size',
        value: `${frac(helmet.circIn)}″ around (hat size ${frac(helmet.hat)})`,
        note: helmet.estimated
          ? `Estimated from age. Please measure: wrap a string around ${whose} head about an inch above the eyebrows, then measure the string.`
          : 'Compare this to the helmet maker’s chart. Sizes vary by brand.',
      },
      { label: 'Size to look for', value: helmet.category },
      { label: 'Face protection', value: helmet.faceGuard },
      { label: 'Must have', value: 'NOCSAE stamp and double ear flaps' },
    ],
    `${stage === 'teeball' ? 'tee ball' : helmet.category.startsWith('Senior') ? 'senior' : 'youth'} batting helmet with face guard`,
    { covered: have.has('helmet') },
  );

  // --- Cleats ----------------------------------------------------------------
  const cleats = cleatsFor(a);
  add(
    'cleats',
    stage === 'teeball' ? 'recommended' : 'must',
    cleats.type,
    [
      { label: 'Type', value: cleats.type },
      {
        label: 'Size',
        value: a.shoe ? `${a.shoe} (their sneaker size)` : 'Same as their sneakers',
        note: 'Leave a thumb’s width at the toe. No more than a half size up.',
      },
    ],
    cleats.query,
    { covered: have.has('cleats'), notes: cleats.notes },
  );

  // --- Pants -----------------------------------------------------------------
  const pants = pantsFor(a);
  add(
    'pants',
    stage === 'teeball' ? 'recommended' : 'must',
    `${pants.size} baseball pants (×2)`,
    [{ label: 'Size', value: pants.size, note: 'Check the waist and inseam on the maker’s chart. Ask the coach for color and style.' }],
    `youth baseball pants ${pants.size.replace('Youth ', 'youth ').replace('Adult ', 'adult ')}`,
    {
      covered: have.has('pants'),
      notes: stage === 'teeball' ? ['Some tee ball leagues just use a team shirt and hat. Ask before buying pants.'] : [],
    },
  );

  add('beltsocks', stage === 'teeball' ? 'nice' : 'recommended', 'Belt and socks in team colors', [], 'youth baseball belt and socks');

  // --- Protection ------------------------------------------------------------
  const isCatcher = a.position === 'catcher' && stage !== 'teeball';
  if (a.protection === 'cup') {
    add(
      'protection',
      isCatcher || stage === 'kidpitch' || stage === 'teen' || stage === 'highschool' ? 'must' : 'recommended',
      `${cupSizeFor(a.age)} cup + compression shorts`,
      [{ label: 'Cup size', value: cupSizeFor(a.age), note: 'Follow the maker’s age/waist chart.' }],
      a.age <= 12 ? 'youth compression shorts with cup baseball' : 'compression sliding shorts with cup baseball',
      { notes: isCatcher ? ['Required for catchers in Little League.'] : [] },
    );
  } else if (a.protection === 'pelvic') {
    add(
      'protection',
      isCatcher ? 'recommended' : 'nice',
      'Pelvic protector + compression shorts',
      [{ label: 'Size', value: 'Youth, by waist size', note: 'Optional for most positions; more common for catchers.' }],
      'girls youth pelvic protector compression shorts',
      {
        why: 'Optional for most girls, but a pelvic protector in compression shorts gives catchers and infielders extra peace of mind.',
      },
    );
  }

  add('water', 'must', '24–32 oz bottle with their name on it', [], 'kids insulated water bottle 32 oz');

  // --- Catcher / first base -------------------------------------------------
  if (isCatcher) {
    const set = catchersSetFor(a.age);
    add(
      'catchersgear',
      'must',
      `${set.set} catcher’s set`,
      [
        { label: 'Set size', value: set.set },
        { label: 'Chest protector', value: set.chest, note: 'Base of throat to belly button.' },
        { label: 'Leg guards', value: set.legs, note: 'Middle of kneecap to top of ankle.' },
      ],
      `youth catchers gear set ${set.set.toLowerCase()}`,
      {
        covered: have.has('catcher'),
        notes: ['Many leagues have a team set. Ask the coach. If it fits well, you can skip this.'],
      },
    );
    if (a.age >= 7) {
      const mitt = catchersMittFor(a);
      add(
        'catchersmitt',
        stage === 'coachpitch' ? 'recommended' : 'must',
        `${frac(mitt.size)}″ youth catcher’s mitt${handCode ? ` (${handCode})` : ''}`,
        [
          { label: 'Size', value: `${frac(mitt.size)}″`, note: `Range: ${rangeLabel(mitt.range)}` },
          { label: 'Hand', value: mitt.hand.label, note: mitt.hand.wornOn },
        ],
        `youth catchers mitt ${dec(mitt.size)} inch ${mitt.hand.query}`.trim(),
        { notes: mitt.notes },
      );
    }
  }

  if (a.position === 'firstbase' && a.age >= 9) {
    const mitt = firstBaseMittFor(a);
    add(
      'firstbasemitt',
      'nice',
      `${rangeLabel(mitt.range)} first base mitt${handCode ? ` (${handCode})` : ''}`,
      [
        { label: 'Size', value: rangeLabel(mitt.range) },
        { label: 'Hand', value: mitt.hand.label, note: mitt.hand.wornOn },
      ],
      `youth first base mitt ${dec(mitt.size)} inch ${mitt.hand.query}`.trim(),
      { notes: mitt.notes },
    );
  }

  // --- Extras ----------------------------------------------------------------
  add(
    'bag',
    stage === 'teeball' ? 'nice' : 'recommended',
    isCatcher && a.age >= 9 ? 'Wheeled bag (for catcher’s gear)' : 'Backpack-style bat bag',
    [],
    isCatcher && a.age >= 9 ? 'youth catchers wheeled bat bag' : 'youth baseball backpack bat bag',
  );

  const bgSize = battingGloveSizeFor(a.age);
  add(
    'battinggloves',
    stage === 'teen' || stage === 'highschool' ? 'recommended' : 'nice',
    `${bgSize} batting gloves`,
    [{ label: 'Size', value: bgSize, note: 'Confirm with the maker’s hand-length chart.' }],
    `youth batting gloves ${bgSize.split(' or ')[0].toLowerCase()}`,
  );

  add(
    'practice',
    first && (stage === 'teeball' || stage === 'coachpitch') ? 'recommended' : 'nice',
    a.age <= 8 ? 'Batting tee + soft training balls' : 'Batting tee + hitting net',
    [],
    a.age <= 8 ? 'batting tee youth soft training baseballs' : 'batting tee and hitting net baseball',
  );

  if (!have.has('glove') || first) {
    add('glovecare', 'nice', 'Glove conditioner + wrap', [], 'baseball glove conditioner break in kit');
  }

  if (a.position === 'outfield' || a.age >= 9) {
    add('sunglasses', 'nice', 'Youth sport sunglasses', [], 'youth baseball sunglasses');
  }

  // --- Summary pieces ---------------------------------------------------------
  const order: Priority[] = ['must', 'recommended', 'nice'];
  items.sort((x, y) => order.indexOf(x.priority) - order.indexOf(y.priority));

  const sum = (pri: Priority[]) =>
    items
      .filter((i) => !i.covered && pri.includes(i.priority))
      .reduce<[number, number]>(
        (acc, i) => {
          const [lo, hi] = i.price;
          const qty = i.id === 'pants' ? 2 : 1;
          return [acc[0] + lo * qty, acc[1] + hi * qty];
        },
        [0, 0],
      );

  const throwsText = a.throws === 'R' ? 'Throws right' : a.throws === 'L' ? 'Throws left' : 'Throwing hand TBD';
  const chips = [
    `Age ${a.age}`,
    body.estimated ? `~${feetInches(body.heightIn)} (estimated)` : `${feetInches(body.heightIn)}, ${Math.round(body.weightLb)} lb`,
    throwsText,
    teamLabel(a.team),
    STAGE_LABEL[stage],
  ];

  const cheatSheet: Spec[] = [
    { label: 'Bat', value: `${lenText}, ${dropLabel(bat.startDrop)} drop`, note: certInfo.stamp },
    { label: 'Glove', value: `${frac(glove.size)}″ ${glove.hand.code ?? '(hand TBD)'}`, note: glove.hand.wornOn },
    { label: 'Helmet', value: `Hat ${frac(helmet.hat)} · ${frac(helmet.circIn)}″ head`, note: helmet.estimated ? 'Estimated. Measure to confirm.' : helmet.category },
    { label: 'Cleats', value: cleats.type, note: a.shoe ? `Size ${a.shoe}` : 'Same size as sneakers' },
    { label: 'Pants', value: pants.size },
    { label: 'Batting gloves', value: bgSize },
  ];
  if (isCatcher) cheatSheet.push({ label: 'Catcher’s set', value: catchersSetFor(a.age).set });

  const greeting = a.name ? `${a.name} is` : 'My kid is';
  const coachMessage = [
    `Hi Coach! ${greeting} excited to play this season. Before we buy gear, a few quick questions:`,
    '',
    `1. Which bat stamp does our league require (USA Baseball, USSSA or BBCOR)? Is there a max length or drop?`,
    `2. Does the team provide batting helmets${isCatcher ? ' and catcher’s gear' : ''}?`,
    '3. What color and style of pants, socks and belt should we get?',
    '4. When and where is the first practice?',
    '',
    'Thanks so much!',
  ].join('\n');

  const timeline: TimelineStep[] = [
    {
      when: '3–4 weeks before the first practice',
      tasks: [
        'Send the coach the message above.',
        body.estimated || helmet.estimated
          ? `Measure ${whose} height, weight and head (5 minutes, a tape measure and a string), then update this plan.`
          : `Double-check ${whose} measurements. Kids grow fast.`,
        have.has('glove') ? 'Check that last season’s glove still fits (see the fit check).' : 'Order the glove first. It needs time to break in.',
      ],
    },
    {
      when: '2 weeks before',
      tasks: [
        'Buy the rest of the must-haves once the coach confirms the bat stamp and uniform colors.',
        'Play catch for 10 minutes a day. It’s the best glove break-in there is.',
        'Try cleats on with their game socks.',
      ],
    },
    {
      when: '1 week before',
      tasks: [
        'Have them wear the cleats around the yard so they’re comfortable on day one.',
        a.protection === 'cup' ? 'Practice putting on the helmet and cup at home, so it’s no big deal at the field.' : 'Practice putting on the helmet at home so it feels normal.',
        'Write their name on everything: glove, bat, helmet, bottle, bag.',
      ],
    },
    {
      when: 'The night before',
      tasks: ['Pack the bag with the checklist below.', 'Fill and freeze half the water bottle (top it off in the morning).', 'Lay out the uniform, hat and cleats.'],
    },
  ];

  const bag: Plan['bag'] = [
    { text: 'Glove', icon: 'glove' },
    ...(have.has('bat') || stage !== 'teeball' ? [{ text: 'Bat', icon: 'bat' }] : []),
    { text: 'Batting helmet', icon: 'helmet' },
    { text: 'Water bottle (full)', icon: 'bottle' },
    { text: 'Cleats (or on their feet)', icon: 'cleat' },
    ...(a.protection === 'cup' ? [{ text: 'Cup + compression shorts (on before you leave)', icon: 'shield' }] : []),
    { text: 'Team hat', icon: 'cap' },
    ...(isCatcher ? [{ text: 'Catcher’s gear + mitt', icon: 'mask' }] : []),
    { text: 'Sunscreen', icon: 'sun' },
    { text: 'A snack for after', icon: 'apple' },
  ];

  return {
    answers: a,
    who,
    whose,
    stage,
    stageLabel: STAGE_LABEL[stage],
    cert,
    estimatedBody: body.estimated,
    chips,
    items,
    cheatSheet,
    budget: { must: sum(['must']), withRecommended: sum(['must', 'recommended']) },
    coachMessage,
    timeline,
    bag,
  };
}

// ---------------------------------------------------------------------------
// Share links: answers <-> URL query string (name is never included)
// ---------------------------------------------------------------------------

const KEYS = {
  age: 'a',
  heightIn: 'h',
  weightLb: 'w',
  throws: 't',
  bats: 'b',
  team: 'tm',
  position: 'p',
  experience: 'x',
  budget: 'bg',
  have: 'hv',
  protection: 'pr',
  headIn: 'hd',
  shoe: 'sh',
} as const;

export function encodeAnswers(a: Answers): string {
  const q = new URLSearchParams();
  q.set(KEYS.age, String(a.age));
  if (a.heightIn != null) q.set(KEYS.heightIn, String(a.heightIn));
  if (a.weightLb != null) q.set(KEYS.weightLb, String(a.weightLb));
  q.set(KEYS.throws, a.throws);
  q.set(KEYS.bats, a.bats);
  q.set(KEYS.team, a.team);
  q.set(KEYS.position, a.position);
  q.set(KEYS.experience, a.experience);
  q.set(KEYS.budget, a.budget);
  if (a.have.length) q.set(KEYS.have, a.have.join('.'));
  q.set(KEYS.protection, a.protection);
  if (a.headIn != null) q.set(KEYS.headIn, String(a.headIn));
  if (a.shoe) q.set(KEYS.shoe, a.shoe);
  return q.toString();
}

function oneOf<T extends string>(v: string | null, allowed: readonly T[], fallback: T): T {
  return v && (allowed as readonly string[]).includes(v) ? (v as T) : fallback;
}

function num(v: string | null, lo: number, hi: number): number | undefined {
  if (v == null || v === '') return undefined;
  const n = Number(v);
  return Number.isFinite(n) && n >= lo && n <= hi ? n : undefined;
}

/** Parse a share link. Returns null if it doesn't look like one. */
export function decodeAnswers(search: string): Answers | null {
  const q = new URLSearchParams(search);
  const age = num(q.get(KEYS.age), 4, 18);
  if (age == null) return null;
  const haveAllowed = ['glove', 'bat', 'helmet', 'cleats', 'pants', 'catcher'] as const;
  return {
    age: Math.round(age),
    heightIn: num(q.get(KEYS.heightIn), 30, 84),
    weightLb: num(q.get(KEYS.weightLb), 25, 300),
    throws: oneOf(q.get(KEYS.throws), ['R', 'L', 'unsure'] as const, 'unsure'),
    bats: oneOf(q.get(KEYS.bats), ['R', 'L', 'S', 'unsure'] as const, 'unsure'),
    team: oneOf(q.get(KEYS.team), ['teeball', 'rec', 'travel', 'school', 'unsure'] as const, 'unsure'),
    position: oneOf(q.get(KEYS.position), ['unsure', 'infield', 'outfield', 'pitcher', 'catcher', 'firstbase'] as const, 'unsure'),
    experience: oneOf(q.get(KEYS.experience), ['first', 'returning'] as const, 'first'),
    budget: oneOf(q.get(KEYS.budget), ['lean', 'balanced', 'allin'] as const, 'balanced'),
    have: (q.get(KEYS.have) ?? '')
      .split('.')
      .filter((h): h is (typeof haveAllowed)[number] => (haveAllowed as readonly string[]).includes(h)),
    protection: oneOf(q.get(KEYS.protection), ['cup', 'pelvic', 'skip'] as const, 'skip'),
    headIn: num(q.get(KEYS.headIn), 15, 26),
    shoe: (q.get(KEYS.shoe) ?? '').replace(/[^0-9A-Za-z.½ ]/g, '').slice(0, 6) || undefined,
  };
}
