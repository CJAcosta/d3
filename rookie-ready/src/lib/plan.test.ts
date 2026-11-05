import { describe, expect, it } from 'vitest';
import { buildPlan, cleanName, decodeAnswers, encodeAnswers } from './plan';
import { searchUrl } from './links';
import type { Answers } from './types';
import { merchants } from '../config/affiliates';

const base: Answers = {
  name: 'Sam',
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

const ids = (a: Answers) => buildPlan(a).items.map((i) => i.id);

describe('buildPlan', () => {
  it('personalizes with the name', () => {
    const p = buildPlan(base);
    expect(p.who).toBe('Sam');
    expect(p.whose).toBe('Sam’s');
    expect(buildPlan({ ...base, name: 'Wes' }).whose).toBe('Wes’');
    expect(buildPlan({ ...base, name: '' }).whose).toBe('your player’s');
  });

  it('includes the core must-haves for a coach-pitch player', () => {
    const p = buildPlan(base);
    const must = p.items.filter((i) => i.priority === 'must').map((i) => i.id);
    expect(must).toEqual(expect.arrayContaining(['glove', 'bat', 'helmet', 'cleats', 'pants', 'water']));
  });

  it('adds catcher gear only for catchers', () => {
    expect(ids(base)).not.toContain('catchersgear');
    const c = ids({ ...base, age: 10, position: 'catcher' });
    expect(c).toContain('catchersgear');
    expect(c).toContain('catchersmitt');
    const cup = buildPlan({ ...base, age: 10, position: 'catcher' }).items.find((i) => i.id === 'protection');
    expect(cup?.priority).toBe('must');
  });

  it('skips protection when asked and uses pelvic protector wording for girls', () => {
    expect(ids({ ...base, protection: 'skip' })).not.toContain('protection');
    const p = buildPlan({ ...base, protection: 'pelvic' }).items.find((i) => i.id === 'protection');
    expect(p?.headline).toContain('Pelvic');
  });

  it('marks owned gear as covered and leaves it out of the budget', () => {
    const p1 = buildPlan(base);
    const p2 = buildPlan({ ...base, have: ['glove', 'helmet'] });
    expect(p2.items.find((i) => i.id === 'glove')?.covered).toBe(true);
    expect(p2.budget.must[0]).toBeLessThan(p1.budget.must[0]);
  });

  it('builds a spec-matched shop query for the glove', () => {
    const g = buildPlan(base).items.find((i) => i.id === 'glove')!;
    expect(g.query).toMatch(/glove 10\.75 inch right hand throw/);
    expect(g.links.length).toBeGreaterThan(0);
    for (const l of g.links) expect(l.href).toMatch(/^https:\/\//);
  });

  it('sorts must-haves first', () => {
    const order = buildPlan(base).items.map((i) => i.priority);
    const firstNice = order.indexOf('nice');
    expect(order.slice(firstNice).every((p) => p === 'nice')).toBe(true);
  });

  it('writes a coach message without the name when none is given', () => {
    expect(buildPlan({ ...base, name: undefined }).coachMessage).toContain('My kid is');
    expect(buildPlan(base).coachMessage).toContain('Sam is');
  });

  it('strips markup from names', () => {
    expect(cleanName('<img src=x onerror=alert(1)>')).not.toMatch(/[<>=()]/);
    expect(cleanName('  Mary-Kate O\'Neil  ')).toBe("Mary-Kate O'Neil");
  });
});

describe('share links', () => {
  it('round-trips answers without the name', () => {
    const decoded = decodeAnswers(encodeAnswers(base));
    const { name: _name, ...rest } = base;
    expect(decoded).toMatchObject(rest);
    expect(decoded?.name).toBeUndefined();
    expect(encodeAnswers(base)).not.toContain('Sam');
  });
  it('rejects garbage', () => {
    expect(decodeAnswers('')).toBeNull();
    expect(decodeAnswers('a=999')).toBeNull();
    const d = decodeAnswers('a=9&t=<script>&tm=evil&hv=glove.hack');
    expect(d?.throws).toBe('unsure');
    expect(d?.team).toBe('unsure');
    expect(d?.have).toEqual(['glove']);
  });
});

describe('affiliate links', () => {
  it('adds the Amazon tag only when configured', () => {
    const amazon = { ...merchants.find((m) => m.id === 'amazon')! };
    expect(searchUrl({ ...amazon, params: { tag: '' } }, 'youth glove')).toBe('https://www.amazon.com/s?k=youth+glove');
    expect(searchUrl({ ...amazon, params: { tag: 'test-20' } }, 'youth glove')).toContain('tag=test-20');
  });
  it('wraps in a network deep link template', () => {
    const m = { ...merchants[0], params: {}, deepLinkTemplate: 'https://net.example/c/1?u={url}' };
    const href = searchUrl(m, 'bat');
    expect(href.startsWith('https://net.example/c/1?u=https%3A%2F%2F')).toBe(true);
  });
});
