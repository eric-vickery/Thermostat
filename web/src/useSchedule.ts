import { useCallback, useEffect, useState } from 'react';
import {
  DaySchedule,
  DayName,
  addScheduleEvent,
  clearScheduleDay,
  cloneScheduleDay,
  fetchSchedule,
  removeScheduleEvent,
  saveSchedule,
} from './api';

function describe(e: unknown): string {
  return e instanceof Error ? e.message : 'offline';
}

// Schedule edits only change the device's memory. `dirty` tracks whether there is anything
// that still needs save() to be written to the device's flash.
export function useSchedule() {
  const [days, setDays] = useState<DaySchedule[] | null>(null);
  const [dirty, setDirty] = useState(false);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    fetchSchedule()
      .then(setDays)
      .catch((e) => setError(describe(e)));
  }, []);

  const replaceDay = (changed: DaySchedule) =>
    setDays((prev) => prev && prev.map((d) => (d.day === changed.day ? changed : d)));

  // Runs one change against the device. Resolves to whether it worked.
  const run = useCallback(async (change: () => Promise<void>, edits = true) => {
    setBusy(true);
    try {
      await change();
      setError(null);
      return true;
    } catch (e) {
      setError(describe(e));
      return false;
    } finally {
      if (edits) setDirty(true);
      setBusy(false);
    }
  }, []);

  const addEvent = (day: DayName, time: string, setpoint: number) =>
    run(async () => replaceDay(await addScheduleEvent(day, time, setpoint)));

  const removeEvent = (day: DayName, index: number) =>
    run(async () => replaceDay(await removeScheduleEvent(day, index)));

  const clearDay = (day: DayName) => run(async () => replaceDay(await clearScheduleDay(day)));

  const cloneDay = (from: DayName, targets: DayName[]) =>
    run(async () => {
      for (const to of targets) {
        replaceDay(await cloneScheduleDay(from, to));
      }
    });

  const save = async () => {
    if (await run(saveSchedule, false)) {
      setDirty(false);
    }
  };

  return { days, dirty, busy, error, addEvent, removeEvent, clearDay, cloneDay, save };
}
