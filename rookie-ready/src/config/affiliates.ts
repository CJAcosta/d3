/**
 * Affiliate merchants.
 *
 * Every shop button on the site is built from this file, so you only paste your
 * IDs once. A merchant with `enabled: false` never shows up.
 *
 * How a link is built:
 *   1. `searchUrl` gets the spec-matched query (e.g. "youth baseball glove 10.5 inch right hand throw").
 *   2. Any non-empty `params` are appended (Amazon's `tag`, for example).
 *   3. If `deepLinkTemplate` is set, the finished URL is URL-encoded into its `{url}`
 *      placeholder. Impact, CJ, Rakuten, AvantLink and others all give you a template like this.
 *
 * Until you fill in your IDs the links still work; they just do not earn anything.
 * Check each `searchUrl` in a browser before launch. Retailers change their URLs.
 */
export interface Merchant {
  id: string;
  /** Name shown on the button. Always name the store so shoppers know where they are going. */
  label: string;
  enabled: boolean;
  searchUrl: string;
  params: Record<string, string>;
  deepLinkTemplate: string;
}

export const merchants: Merchant[] = [
  {
    id: 'amazon',
    label: 'Amazon',
    enabled: true,
    searchUrl: 'https://www.amazon.com/s?k={q}',
    // Your Associates store ID, e.g. 'rookieready-20'.
    params: { tag: '' },
    deepLinkTemplate: '',
  },
  {
    id: 'dicks',
    label: 'DICK’S',
    enabled: true,
    searchUrl: 'https://www.dickssportinggoods.com/search/SearchDisplay?searchTerm={q}',
    params: {},
    // From DICK'S affiliate program on your network dashboard, e.g. 'https://example.network/c/123/456/789?u={url}'.
    deepLinkTemplate: '',
  },
  {
    id: 'walmart',
    label: 'Walmart',
    enabled: false,
    searchUrl: 'https://www.walmart.com/search?q={q}',
    params: {},
    deepLinkTemplate: '',
  },
];

/** Amazon requires this exact sentence (or substantially similar) on any site with Amazon links. */
export const amazonDisclosure = 'As an Amazon Associate we earn from qualifying purchases.';

/** How many store buttons to show per gear item. Two keeps the choice simple. */
export const maxButtonsPerItem = 2;
