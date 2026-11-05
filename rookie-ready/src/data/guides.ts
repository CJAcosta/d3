/** Guide index: drives the guides hub, home page cards and "related guides". */
export interface GuideMeta {
  slug: string;
  href: string;
  title: string;
  blurb: string;
  icon: string;
}

const g = (slug: string, title: string, blurb: string, icon: string): GuideMeta => ({
  slug,
  href: `/guides/${slug}/`,
  title,
  blurb,
  icon,
});

export const guides: GuideMeta[] = [
  g('youth-baseball-gear-checklist', 'The first-season baseball gear checklist', 'Everything a new player needs, by age, with what’s required, what’s optional and what to skip.', 'bag'),
  g('youth-bat-size-chart', 'Youth bat size chart (plus 3 at-home fit tests)', 'Length by height and weight, the right drop by age, and how to tell if a bat is too heavy.', 'bat'),
  g('youth-glove-size-chart', 'Youth glove size chart by age and position', 'Glove sizes from tee ball to high school, RHT vs LHT, and why bigger isn’t better.', 'glove'),
  g('bat-certifications-usa-usssa-bbcor', 'USA vs USSSA vs BBCOR bats, explained', 'The stamp on the bat decides where it’s legal. Here’s which one your kid needs.', 'tag'),
  g('batting-helmet-size-guide', 'How to measure for a batting helmet', 'Measure in 30 seconds, convert to hat size, and choose the right face guard.', 'helmet'),
  g('youth-baseball-cleats', 'Youth baseball cleats: molded vs metal, and sizing', 'When metal is allowed, how much room to leave, and whether soccer cleats work.', 'cleat'),
  g('how-to-break-in-a-glove', 'How to break in a glove (the kid-friendly way)', 'Get a new glove game-ready in about a week, without ovens, microwaves or myths.', 'oil'),
  g('catchers-gear-for-kids', 'Catcher’s gear for kids: sizing and rules', 'What a young catcher needs, how to measure for it, and when the team set is enough.', 'mask'),
  g('tee-ball-gear', 'Tee ball gear: what a 4–6 year old really needs', 'The short list for the littlest players, and the things you don’t need to buy yet.', 'tee'),
];

export const guideBySlug = (slug: string) => guides.find((x) => x.slug === slug)!;
