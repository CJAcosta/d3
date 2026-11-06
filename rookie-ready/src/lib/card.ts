/**
 * Facts printed on the rookie trading card. Pure, so it's unit-tested; the
 * drawing itself lives in src/scripts/rookieCard.ts.
 */
import { batFor, feetInches, frac, gloveFor, helmetFor } from './sizing';
import type { Answers, Position } from './types';

export const CARD_COLORS = [
  { name: 'Red', hex: '#c62f2a' },
  { name: 'Navy', hex: '#1b2f52' },
  { name: 'Royal', hex: '#2457c5' },
  { name: 'Green', hex: '#1d7a4b' },
  { name: 'Orange', hex: '#d9661f' },
  { name: 'Purple', hex: '#6b3fa0' },
  { name: 'Maroon', hex: '#7a1f2b' },
  { name: 'Teal', hex: '#0f7c80' },
] as const;

const POSITION_LABEL: Record<Position, string> = {
  unsure: 'Utility',
  infield: 'Infield',
  outfield: 'Outfield',
  pitcher: 'Pitcher',
  catcher: 'Catcher',
  firstbase: 'First Base',
};

export function positionLabel(p: Position): string {
  return POSITION_LABEL[p];
}

/** Spring season the card is for: from July on, it's next year's season. */
export function seasonYear(now = new Date()): number {
  return now.getMonth() >= 6 ? now.getFullYear() + 1 : now.getFullYear();
}

/** Classic baseball-card "B/T" line, e.g. "R/R". */
export function batsThrows(a: Pick<Answers, 'bats' | 'throws'>): string {
  const b = a.bats === 'S' ? 'S' : a.bats === 'unsure' ? '?' : a.bats;
  const t = a.throws === 'unsure' ? '?' : a.throws;
  return `${b}/${t}`;
}

export function cleanNumber(raw: string): string {
  return raw.replace(/\D/g, '').slice(0, 2);
}

export function cleanCardText(raw: string, max: number): string {
  return raw.replace(/[^\p{L}\p{M}\p{N}' .&-]/gu, '').slice(0, max);
}

export interface CardFacts {
  bt: string;
  /** Only shown when the parent entered a real height. */
  height: string | null;
  age: string;
  gear: string;
}

export function cardFacts(a: Answers): CardFacts {
  const bat = batFor(a);
  const glove = gloveFor(a);
  const helmet = helmetFor(a);
  return {
    bt: batsThrows(a),
    height: a.heightIn != null ? feetInches(a.heightIn) : null,
    age: String(a.age),
    gear: `Lumber ${bat.length}″  ·  Leather ${frac(glove.size)}″  ·  Lid ${frac(helmet.hat)}`,
  };
}
