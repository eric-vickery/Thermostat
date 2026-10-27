import { useCallback, useEffect, useRef, useState } from 'react';
import {
  Mode,
  SETPOINT_MAX,
  SETPOINT_MIN,
  ThermostatState,
  fetchThermostat,
  updateThermostat,
} from './api';

const POLL_MS = 2000;
// Like the device, wait until the setpoint stops changing before sending it
const SETPOINT_SEND_DELAY_MS = 600;

export function useThermostat() {
  const [state, setState] = useState<ThermostatState | null>(null);
  const [error, setError] = useState<string | null>(null);
  const sendTimer = useRef<number | undefined>(undefined);

  useEffect(() => {
    let cancelled = false;
    const poll = async () => {
      try {
        const next = await fetchThermostat();
        if (cancelled) return;
        setError(null);
        // Don't let a poll overwrite a setpoint the user is still adjusting
        setState((prev) =>
          prev && sendTimer.current !== undefined ? { ...next, setpoint: prev.setpoint } : next,
        );
      } catch (e) {
        if (!cancelled) setError(e instanceof Error ? e.message : 'offline');
      }
    };
    poll();
    const id = window.setInterval(poll, POLL_MS);
    return () => {
      cancelled = true;
      window.clearInterval(id);
      window.clearTimeout(sendTimer.current);
    };
  }, []);

  const setSetpoint = useCallback((value: number) => {
    const setpoint = Math.min(SETPOINT_MAX, Math.max(SETPOINT_MIN, Math.round(value)));
    setState((prev) => (prev ? { ...prev, setpoint } : prev));
    window.clearTimeout(sendTimer.current);
    sendTimer.current = window.setTimeout(async () => {
      sendTimer.current = undefined;
      try {
        setState(await updateThermostat({ setpoint }));
        setError(null);
      } catch (e) {
        setError(e instanceof Error ? e.message : 'offline');
      }
    }, SETPOINT_SEND_DELAY_MS);
  }, []);

  const setMode = useCallback(async (mode: Mode) => {
    setState((prev) => (prev ? { ...prev, mode } : prev));
    try {
      setState(await updateThermostat({ mode }));
      setError(null);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'offline');
    }
  }, []);

  return { state, error, setSetpoint, setMode };
}
