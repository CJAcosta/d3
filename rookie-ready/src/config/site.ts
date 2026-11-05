/**
 * Site-wide settings. Edit this file to rebrand, connect analytics, or
 * connect an email list. Nothing here is secret: it all ships to the browser.
 */
export const site = {
  name: 'Rookie Ready',
  tagline: 'First-season baseball gear, sized right.',
  description:
    'A free, step-by-step guide for parents buying their kid’s first baseball gear. Enter age, height and throwing hand, and get the exact bat, glove, helmet and cleat sizes, plus what to skip.',
  /** Shown in the footer and on the About page. */
  owner: 'The Rookie Ready family',
  contactEmail: 'hello@rookieready.example',
  /** Date the sizing content was last checked against league rules. Shown on guides. */
  rulesReviewed: '2026-11-05',
  /** Default social share image (1200x630) in /public. */
  ogImage: '/og-default.png',
  locale: 'en_US',

  /**
   * Analytics. Leave blank to disable. Plausible is recommended because it is
   * cookieless, so you do not need a cookie banner for it.
   * Example: plausibleDomain: 'rookieready.com'
   */
  analytics: {
    plausibleDomain: '',
    plausibleSrc: 'https://plausible.io/js/script.tagged-events.js',
    /** Optional GA4 measurement ID (e.g. 'G-XXXXXXX'). Needs a consent banner in some regions. */
    ga4Id: '',
  },

  /**
   * Email list. Paste the form "action" URL from your provider (Kit/ConvertKit,
   * beehiiv, Mailchimp, Buttondown...). Leave blank to hide the signup box.
   */
  newsletter: {
    action: '',
    emailFieldName: 'email',
    /** Optional hidden fields some providers require, e.g. { u: '...', id: '...' } for Mailchimp. */
    hiddenFields: {} as Record<string, string>,
  },
} as const;

export type Site = typeof site;
