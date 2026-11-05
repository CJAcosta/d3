// @ts-check
import { defineConfig } from 'astro/config';
import sitemap from '@astrojs/sitemap';

// Set SITE_URL in your hosting provider's environment (e.g. https://www.rookieready.com).
// It drives canonical URLs, Open Graph tags and the sitemap.
const site = process.env.SITE_URL || 'https://www.rookieready.example';

export default defineConfig({
  site,
  trailingSlash: 'always',
  // Keep source whitespace so text next to inline links never runs together.
  compressHTML: false,
  integrations: [sitemap()],
  build: { inlineStylesheets: 'auto' },
});
