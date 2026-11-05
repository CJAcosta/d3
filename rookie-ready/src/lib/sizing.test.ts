import { describe, expect, it } from 'vitest';
import {
  batCertFor,
  batFor,
  chartBatLength,
  cleatsFor,
  faceGuardFor,
  feetInches,
  frac,
  gloveFor,
  handFor,
  hatSize,
  helmetFor,
  pantsFor,
  stageFor,
} from './sizing';
import type { Answers } from './types';

const base: Answers = {
  age: 8,
  heightIn: 50,
  weightLb: 55,
  throws: 'R',
  bats: 'R',
  team: 'rec',
  position: 'unsure',
  experience: 'first',
  budget: 'balanced',
  have: [],
  protection: 'cup',
};

describe('formatting', () => {
  it('renders fractions', () => {
    expect(frac(10.75)).toBe('10¾');
    expect(frac(6.625)).toBe('6⅝');
    expect(frac(28)).toBe('28');
    expect(frac(9.5)).toBe('9½');
  });
  it('renders feet and inches', () => {
    expect(feetInches(50)).toBe('4′2″');
    expect(feetInches(60)).toBe('5′0″');
    expect(feetInches(47.6)).toBe('4′0″');
  });
});

describe('stage and certification', () => {
  it('maps ages to stages for rec leagues', () => {
    expect(stageFor(4, 'rec')).toBe('teeball');
    expect(stageFor(7, 'rec')).toBe('coachpitch');
    expect(stageFor(10, 'rec')).toBe('kidpitch');
    expect(stageFor(13, 'rec')).toBe('teen');
    expect(stageFor(16, 'rec')).toBe('highschool');
  });
  it('respects an explicit tee ball team', () => {
    expect(stageFor(7, 'teeball')).toBe('teeball');
  });
  it('requires the right bat stamp', () => {
    expect(batCertFor(5, 'teeball', 'teeball')).toBe('usa-teeball');
    expect(batCertFor(10, 'rec', 'kidpitch')).toBe('usa');
    expect(batCertFor(10, 'unsure', 'kidpitch')).toBe('usa');
    expect(batCertFor(10, 'travel', 'kidpitch')).toBe('usssa');
    expect(batCertFor(13, 'rec', 'teen')).toBe('usa-or-bbcor');
    expect(batCertFor(15, 'school', 'highschool')).toBe('bbcor');
    expect(batCertFor(16, 'travel', 'highschool')).toBe('bbcor');
  });
});

describe('bats', () => {
  it('reads the industry chart', () => {
    expect(chartBatLength(50, 55)).toBe(29); // 4'1"-4'4", under 60 lb
    expect(chartBatLength(58, 85)).toBe(30); // 4'9"-5', 81-90 lb
    expect(chartBatLength(66, 135)).toBe(33); // 5'5"-5'8", 131-140 lb
  });
  it('fills chart gaps instead of failing', () => {
    expect(chartBatLength(38, 90)).toBe(27);
    expect(chartBatLength(75, 50)).toBeGreaterThan(30);
  });
  it('keeps tee ball bats at 26 inches or less', () => {
    const r = batFor({ ...base, age: 5, heightIn: 46, weightLb: 45, team: 'teeball' });
    expect(r.cert).toBe('usa-teeball');
    expect(r.length).toBeLessThanOrEqual(26);
    expect(Math.max(...r.lengths)).toBeLessThanOrEqual(26);
  });
  it('caps long chart results for small kids by age', () => {
    const r = batFor({ ...base, age: 6, heightIn: 46, weightLb: 46 });
    expect(r.length).toBeLessThanOrEqual(27);
  });
  it('offers one size shorter for first-season players', () => {
    const r = batFor({ ...base, age: 10, heightIn: 54, weightLb: 70 });
    expect(r.lengths).toEqual([r.length - 1, r.length]);
    const r2 = batFor({ ...base, age: 10, heightIn: 54, weightLb: 70, experience: 'returning' });
    expect(r2.lengths).toEqual([r2.length]);
  });
  it('uses -3 for BBCOR and lighter drops for young kids', () => {
    expect(batFor({ ...base, age: 16, heightIn: 68, weightLb: 140, team: 'school' }).drop).toEqual([-3, -3]);
    expect(batFor({ ...base, age: 8 }).startDrop).toBeLessThanOrEqual(-10);
  });
  it('never exceeds the Little League Majors max of 33 inches for kid pitch', () => {
    const r = batFor({ ...base, age: 12, heightIn: 70, weightLb: 170 });
    expect(r.length).toBeLessThanOrEqual(33);
  });
  it('estimates from age when height and weight are skipped', () => {
    const r = batFor({ ...base, heightIn: undefined, weightLb: undefined });
    expect(r.length).toBeGreaterThanOrEqual(26);
    expect(r.length).toBeLessThanOrEqual(29);
  });
});

describe('gloves', () => {
  it('maps throwing hand to the glove hand', () => {
    expect(handFor('R').code).toBe('RHT');
    expect(handFor('R').wornOn).toContain('left');
    expect(handFor('L').code).toBe('LHT');
    expect(handFor('L').wornOn).toContain('right');
    expect(handFor('unsure').code).toBeNull();
  });
  it('sizes tee ball gloves small', () => {
    expect(gloveFor({ ...base, age: 4 }).size).toBe(9);
    expect(gloveFor({ ...base, age: 6 }).size).toBe(10);
  });
  it('stays inside the chart range and on quarter-inch steps', () => {
    for (let age = 4; age <= 18; age++) {
      for (const position of ['unsure', 'infield', 'outfield', 'pitcher'] as const) {
        const g = gloveFor({ ...base, age, position });
        expect(g.size).toBeGreaterThanOrEqual(g.range[0]);
        expect(g.size).toBeLessThanOrEqual(g.range[1]);
        expect((g.size * 4) % 1).toBe(0);
      }
    }
  });
  it('gives outfielders a bigger glove than infielders', () => {
    const inf = gloveFor({ ...base, age: 11, position: 'infield', experience: 'returning' });
    const of = gloveFor({ ...base, age: 11, position: 'outfield', experience: 'returning' });
    expect(of.size).toBeGreaterThan(inf.size);
  });
});

describe('helmets', () => {
  it('converts head circumference to hat size', () => {
    expect(hatSize(20.75)).toBe(6.625);
    expect(hatSize(22)).toBe(7);
  });
  it('flags estimated head sizes', () => {
    expect(helmetFor(base).estimated).toBe(true);
    expect(helmetFor({ ...base, headIn: 21 }).estimated).toBe(false);
  });
  it('puts the C-flap on the side facing the pitcher', () => {
    expect(faceGuardFor('R', 10).text).toContain('left side');
    expect(faceGuardFor('L', 10).text).toContain('right side');
    expect(faceGuardFor('S', 10).text).toContain('Full face guard');
  });
});

describe('cleats and pants', () => {
  it('requires molded cleats through age 12', () => {
    expect(cleatsFor({ ...base, age: 12 }).type).toContain('Molded');
    expect(cleatsFor({ ...base, age: 12 }).type).not.toContain('metal');
    expect(cleatsFor({ ...base, age: 14 }).type).toContain('metal');
  });
  it('sizes pants by height and bumps up for heavier kids', () => {
    expect(pantsFor({ ...base, heightIn: 50, weightLb: 55 }).size).toBe('Youth S');
    expect(pantsFor({ ...base, heightIn: 50, weightLb: 80 }).size).toBe('Youth M');
  });
});
