/**
 * Tiny analytics shim. Works with Plausible or GA4 if either is configured
 * in src/config/site.ts; does nothing otherwise. Never sends personal data.
 */
type Props = Record<string, string | number | boolean>;

declare global {
  interface Window {
    plausible?: (event: string, opts?: { props?: Props }) => void;
    gtag?: (...args: unknown[]) => void;
  }
}

export function track(event: string, props: Props = {}): void {
  try {
    window.plausible?.(event, { props });
    window.gtag?.('event', event, props);
  } catch {
    /* analytics must never break the page */
  }
}

/** Site-wide: track affiliate clicks and CTA clicks. */
export function installClickTracking(): void {
  document.addEventListener(
    'click',
    (e) => {
      const el = (e.target as Element | null)?.closest('a[data-aff], [data-track]');
      if (!el) return;
      if (el.hasAttribute('data-aff')) {
        track('Affiliate Click', {
          merchant: el.getAttribute('data-aff') ?? '',
          item: el.getAttribute('data-item') ?? '',
          page: location.pathname,
        });
      } else {
        track('CTA Click', { id: el.getAttribute('data-track') ?? '', page: location.pathname });
      }
    },
    { capture: true },
  );
}
