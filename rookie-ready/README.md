# Rookie Ready

A guided, affiliate-supported website that helps parents buy their kid's first baseball gear.
Parents answer a few questions (age, height/weight, throwing hand, batting side, league, position,
budget, what they already own) and get a personalized plan:

- Exact sizes: bat length + drop + **required certification stamp**, glove size + **RHT/LHT**, helmet size + face guard side, cleat type, pants, batting gloves, catcher's gear
- A prioritized gear list (must-have / recommended / nice to have / already covered) with a fit check, what to look for and mistakes to skip for every item
- Spec-matched shopping buttons (e.g. "youth baseball glove 10.75 inch right hand throw") for each retailer you're an affiliate of
- A copy-and-send message for the coach, a budget estimate, a countdown to the first practice and a bag checklist
- Print / save as PDF, and a share link for grandparents and family (the child's name is never put in the link)

Plus nine SEO guides (bat and glove size charts, USA vs USSSA vs BBCOR, helmets, cleats, catcher's gear, tee ball, glove break-in, and the full first-season checklist).

## Quick start

```bash
cd rookie-ready
npm install
npm run dev      # http://localhost:4321
npm test         # sizing + plan unit tests (vitest)
npm run check    # type check
npm run build    # static site in dist/
```

Node 22.23.4 or newer is required (Astro 7).

## Where things live

| What | File |
| --- | --- |
| Site name, contact email, analytics, email list | `src/config/site.ts` |
| **Affiliate IDs and store links** | `src/config/affiliates.ts` |
| Sizing rules (bats, gloves, helmets, cleats, pants, catcher's gear) | `src/lib/sizing.ts` |
| Turning answers into a plan (priorities, budget, coach message) | `src/lib/plan.ts` |
| Teaching content for each gear item (why, fit check, mistakes, price ranges) | `src/data/gear.ts` |
| Hand-picked products (optional) | `src/data/picks.ts` |
| Wizard UI (browser) | `src/pages/gear-guide.astro`, `src/scripts/wizard.ts`, `src/styles/wizard.css` |
| Guides | `src/pages/guides/*.astro` (list in `src/data/guides.ts`) |

Guide tables are generated from `sizing.ts`, the same code the wizard uses, so the guides and the
wizard can't disagree. If you change a rule, run `npm test`.

## Affiliate setup

1. **Join programs.** Start with Amazon Associates (you need 3 qualifying sales within 180 days to stay in)
   and DICK'S Sporting Goods (through its affiliate network). Other good fits: Walmart, Academy,
   JustBats, Baseball Monkey, and brand programs.
2. **Paste your IDs** into `src/config/affiliates.ts`:
   - Amazon: `params: { tag: 'yourtag-20' }`
   - Network merchants (Impact, CJ, Rakuten, AvantLink...): paste the deep-link template with `{url}` where the destination goes, into `deepLinkTemplate`.
   - Set `enabled: true/false` per store. The first two enabled stores show on each gear card.
3. **Check every `searchUrl` in a browser** before launch. The Amazon and Walmart patterns are standard; the DICK'S
   search URL could not be verified from here (their site blocks automated requests), so open
   one generated link to confirm it lands on search results.
4. **Optional: add tested picks** in `src/data/picks.ts`. They appear as "Our picks" on matching gear cards.

### Compliance already built in

- **FTC:** a clear disclosure appears **before** the first paid link on every page with links (results page, guides), shop buttons are labeled "Paid links", and there's a full `/disclosure/` page.
- **Amazon Associates:** the required sentence ("As an Amazon Associate we earn from qualifying purchases") is in the disclosure banner and footer. **No prices are shown for products**. The budget box uses general category ranges, which is why nothing pulls live prices. Don't add scraped or hand-typed Amazon prices. Don't put Amazon links in emails or downloadable PDFs.
- **Google:** every paid link has `rel="sponsored nofollow noopener"`.
- **Privacy / COPPA:** the site is for parents. Wizard answers never leave the browser (localStorage only), and share links omit the child's name.

## Analytics (recommended: Plausible, which is cookieless)

Set `analytics.plausibleDomain` in `src/config/site.ts`. These custom events are already wired up:

| Event | When |
| --- | --- |
| `Wizard Start`, `Wizard Step` (`step`) | Funnel through the questions |
| `Plan Complete` (`age`, `team`, `stage`, `budget`, `position`) | Plan generated |
| `Affiliate Click` (`merchant`, `item`, `page`) | Any paid link |
| `Plan Shared`, `Plan Printed`, `Coach Message Copied`, `Shared Plan Viewed` | Engagement |
| `CTA Click` (`id`) | Buttons into the wizard |
| `Newsletter Signup` | Email form submitted |

GA4 is also supported (`analytics.ga4Id`), but needs a cookie banner in some regions.

## Email list

Paste your provider's form action URL into `newsletter.action` (Kit/ConvertKit, beehiiv, Mailchimp, Buttondown).
The signup box then shows on every plan ("Get a pre-season reminder"). The best email for this site is a pre-season
reminder in January/February, when parents actually buy.

## Deploying

It's a static site, so any static host works (Cloudflare Pages, Netlify, Vercel, GitHub Pages).

- Base / root directory: `rookie-ready`
- Build command: `npm run build`
- Output directory: `dist`
- Environment variable: `SITE_URL=https://www.yourdomain.com` (drives canonical URLs, sitemap, social cards)

`public/_headers` sets caching and security headers on Netlify and Cloudflare Pages.

## Launch checklist

- [ ] Pick the final name and domain (check trademark and domain availability for "Rookie Ready"). Update `site.name`, `contactEmail`, `SITE_URL`.
- [ ] **Personalize `/about/`.** Add your family's real story, names and a photo. Firsthand experience is the biggest trust (and Google "E-E-A-T") signal an affiliate site has.
- [ ] Add affiliate IDs and verify each store link.
- [ ] Turn on analytics.
- [ ] Connect the email list.
- [ ] Have `/privacy/` and `/terms/` reviewed (they're templates, not legal advice), and fill in the bracketed sections.
- [ ] Re-check league rules each off-season and bump `site.rulesReviewed` (shown on every guide).
- [ ] Submit `sitemap-index.xml` to Google Search Console and Bing Webmaster Tools.

## Growing it

- **Timing:** youth baseball buying peaks January–March (spring season) with a smaller bump in July–August (fall ball). Publish and promote new content in November–January.
- **Content that ranks and converts:** long-tail "best X for Y" pages built on hands-on testing, e.g. *best tee ball bats*, *best glove for a 7-year-old*, *best USA bats for 10U*, *best lefty youth gloves*. Link each to the wizard.
- **Distribution:** local league registration emails and newsletters (offer the tool free to league boards), parent Facebook groups, Pinterest (checklists and size charts do well there), short videos of your kid doing the bat fit tests.
- **Watch the numbers:** Plan Complete ÷ Wizard Start (funnel health) and Affiliate Click ÷ Plan Complete (how well the plan converts). Improve the weakest step first.
