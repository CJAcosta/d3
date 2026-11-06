import { describe, expect, it } from 'vitest';
import { batsThrows, cardFacts, cleanCardText, cleanNumber, positionLabel, seasonYear } from './card';
import type { Answers } from './types';

const a: Answers = {
  age: 8,
  heightIn: 50,
  weightLb: 55,
  throws: 'R',
  bats: 'L',
  team: 'rec',
  position: 'unsure',
  experience: 'first',
  budget: 'balanced',
  have: [],
  protection: 'cup',
};

describe('rookie card facts', () => {
  it('uses next spring once the season is over', () => {
    expect(seasonYear(new Date(2026, 10, 6))).toBe(2027);
    expect(seasonYear(new Date(2027, 2, 1))).toBe(2027);
    expect(seasonYear(new Date(2027, 6, 1))).toBe(2028);
  });
  it('writes the B/T line', () => {
    expect(batsThrows(a)).toBe('L/R');
    expect(batsThrows({ bats: 'S', throws: 'L' })).toBe('S/L');
    expect(batsThrows({ bats: 'unsure', throws: 'unsure' })).toBe('?/?');
  });
  it('never prints an estimated height', () => {
    expect(cardFacts(a).height).toBe('4′2″');
    expect(cardFacts({ ...a, heightIn: undefined, weightLb: undefined }).height).toBeNull();
  });
  it('includes the gear sizes', () => {
    expect(cardFacts(a).gear).toMatch(/Lumber \d+″ .* Leather 10¾″ .* Lid 6/);
  });
  it('labels positions and cleans input', () => {
    expect(positionLabel('unsure')).toBe('Utility');
    expect(cleanNumber('#7a2b')).toBe('72');
    expect(cleanCardText('<b>Short-stop</b>', 18)).toBe('bShort-stopb');
  });
});
