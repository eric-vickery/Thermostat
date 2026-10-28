import { useEffect, useState } from 'react';

// Minimal hash router: "#/config" -> "/config". Hash links need no server support and
// work with the ESP32 serving a single index.html.
export function useRoute(): string {
  const read = () => window.location.hash.replace(/^#/, '') || '/';
  const [route, setRoute] = useState(read);

  useEffect(() => {
    const onChange = () => setRoute(read());
    window.addEventListener('hashchange', onChange);
    return () => window.removeEventListener('hashchange', onChange);
  }, []);

  return route;
}
