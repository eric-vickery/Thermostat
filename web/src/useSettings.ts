import { useCallback, useEffect, useRef, useState } from 'react';
import { Settings, fetchSettings, updateSettings } from './api';

// Changes are applied to the screen at once and sent to the device once they stop for a moment
const SEND_DELAY_MS = 500;

function describe(e: unknown): string {
  return e instanceof Error ? e.message : 'offline';
}

export function useSettings() {
  const [settings, setSettings] = useState<Settings | null>(null);
  const [error, setError] = useState<string | null>(null);
  const pending = useRef<Partial<Settings>>({});
  const timer = useRef<number | undefined>(undefined);

  useEffect(() => {
    fetchSettings()
      .then(setSettings)
      .catch((e) => setError(describe(e)));
    return () => window.clearTimeout(timer.current);
  }, []);

  const update = useCallback((patch: Partial<Settings>) => {
    setSettings((prev) => (prev ? { ...prev, ...patch } : prev));
    pending.current = { ...pending.current, ...patch };
    window.clearTimeout(timer.current);
    timer.current = window.setTimeout(async () => {
      const body = pending.current;
      pending.current = {};
      try {
        const saved = await updateSettings(body);
        setError(null);
        // Don't yank a control the user has already moved again
        if (Object.keys(pending.current).length === 0) {
          setSettings(saved);
        }
      } catch (e) {
        setError(describe(e));
      }
    }, SEND_DELAY_MS);
  }, []);

  return { settings, error, update };
}
