import { maxButtonsPerItem, merchants, type Merchant } from '../config/affiliates';

export interface ShopLink {
  merchant: string;
  label: string;
  href: string;
}

/** Add tracking params and wrap in the network deep link, if configured. */
export function affiliateUrl(m: Merchant, destination: string): string {
  const url = new URL(destination);
  for (const [k, v] of Object.entries(m.params)) {
    if (v) url.searchParams.set(k, v);
  }
  const finished = url.toString();
  return m.deepLinkTemplate ? m.deepLinkTemplate.replace('{url}', encodeURIComponent(finished)) : finished;
}

export function searchUrl(m: Merchant, query: string): string {
  return affiliateUrl(m, m.searchUrl.replace('{q}', encodeURIComponent(query).replace(/%20/g, '+')));
}

export function enabledMerchants(): Merchant[] {
  return merchants.filter((m) => m.enabled);
}

/** Store buttons for a spec-matched search, in the order merchants are listed in the config. */
export function shopLinks(query: string, limit = maxButtonsPerItem): ShopLink[] {
  return enabledMerchants()
    .slice(0, limit)
    .map((m) => ({ merchant: m.id, label: m.label, href: searchUrl(m, query) }));
}

/** A link to a specific product page you chose (see src/data/picks.ts). */
export function productLink(merchantId: string, url: string): ShopLink | null {
  const m = merchants.find((x) => x.id === merchantId && x.enabled);
  if (!m) return null;
  return { merchant: m.id, label: m.label, href: affiliateUrl(m, url) };
}

/** rel values for every paid link. Google asks for "sponsored" on affiliate links. */
export const SPONSORED_REL = 'sponsored nofollow noopener';
