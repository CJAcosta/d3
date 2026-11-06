/**
 * Entry for the one-file preview build (npm run build:preview). It runs the
 * real gear guide inside a sandboxed preview frame (for example a claude.ai
 * artifact), where downloads go through the host and internal links can't
 * navigate. Not used by the production site.
 */
import { initWizard } from '../scripts/wizard';

interface Downloads {
  save(req: { filename: string; data: Blob }): Promise<unknown>;
}
declare global {
  interface Window {
    claude?: { use(name: string): Promise<unknown> };
  }
}

// Save the rookie card through the host's download prompt when it has one.
window.rrSaveFile = async (blob, filename) => {
  const downloads = (await window.claude?.use('downloads').catch(() => null)) as Downloads | null | undefined;
  if (!downloads) return false;
  try {
    await downloads.save({ filename, data: blob });
  } catch (e) {
    // Declined is still "handled"; anything else falls back to press-and-hold.
    return (e as { code?: string })?.code === 'declined';
  }
  return true;
};

// Guides and other pages aren't part of the preview, so keep taps on internal links in place.
let note: HTMLElement | null = null;
document.addEventListener(
  'click',
  (e) => {
    const a = (e.target as Element | null)?.closest<HTMLAnchorElement>('a[href^="/"]');
    if (!a || a.hasAttribute('data-act')) return;
    e.preventDefault();
    if (!note) {
      note = document.createElement('div');
      note.className = 'toast';
      note.setAttribute('role', 'status');
      document.body.append(note);
    }
    note.textContent = 'The guides aren’t in this preview yet. They’ll be on the real site!';
    note.classList.add('show');
    setTimeout(() => note?.classList.remove('show'), 2800);
  },
  true,
);

initWizard();
