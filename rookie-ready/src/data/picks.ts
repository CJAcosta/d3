/**
 * Curated product picks (optional).
 *
 * By default every gear card links to a spec-matched search ("youth glove
 * 10.75 inch right hand throw"), so shoppers always land on the right size.
 * Add picks here once you've handled or tested a product yourself. Firsthand
 * experience is what makes reviews trustworthy (and what Google rewards).
 *
 * Each pick shows up on matching gear cards with your own blurb.
 *
 * Example:
 *
 *   {
 *     id: 'example-tee-ball-glove',
 *     gear: 'glove',
 *     name: 'Brand Model 10" Tee Ball Glove',
 *     blurb: 'Soft right out of the box. Our 5-year-old could close it on day one.',
 *     links: [{ merchant: 'amazon', url: 'https://www.amazon.com/dp/ASIN_HERE' }],
 *     when: (a) => a.age <= 6,
 *   },
 */
import type { Answers, GearId } from '../lib/types';

export interface Pick {
  id: string;
  gear: GearId;
  name: string;
  blurb: string;
  links: { merchant: string; url: string }[];
  /** Only show this pick for players it fits. */
  when?: (a: Answers) => boolean;
}

export const picks: Pick[] = [];
