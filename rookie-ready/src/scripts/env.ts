/** True when the page runs inside another page (an embed or a preview frame). */
export const framed = (() => {
  try {
    return window.self !== window.top;
  } catch {
    return true;
  }
})();

/**
 * History that can't break the page. Some sandboxed frames and in-app browsers
 * throw on pushState/replaceState; then `ok` turns false and callers fall back.
 */
export const nav = {
  ok: true,
  push(state: unknown, url: string) {
    try {
      history.pushState(state, '', url);
    } catch {
      nav.ok = false;
    }
  },
  replace(state: unknown, url: string) {
    try {
      history.replaceState(state, '', url);
    } catch {
      nav.ok = false;
    }
  },
};
