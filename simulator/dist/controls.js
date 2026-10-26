// Numpad codes remain stable when Num Lock changes the key to End/ArrowDown/etc.
export function padNumberForKey(event) {
  const numpad = /^Numpad([1-8])$/.exec(event.code || '');
  if (numpad) return Number(numpad[1]);
  return /^[1-8]$/.test(event.key) ? Number(event.key) : null;
}
