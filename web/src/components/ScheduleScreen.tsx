import { useEffect, useState } from 'react';
import {
  DAY_NAMES,
  DayName,
  MAX_EVENTS_PER_DAY,
  SETPOINT_MAX,
  SETPOINT_MIN,
} from '../api';
import { useSchedule } from '../useSchedule';

const shortName = (day: DayName) => day.slice(0, 3);
const longName = (day: DayName) => day[0].toUpperCase() + day.slice(1);

// "06:30" -> "6:30 AM" (or 06:30 in 24 hour locales)
function formatTime(time: string): string {
  const [hours, minutes] = time.split(':').map(Number);
  return new Date(2000, 0, 1, hours, minutes).toLocaleTimeString([], {
    hour: 'numeric',
    minute: '2-digit',
  });
}

export default function ScheduleScreen() {
  const { days, dirty, busy, error, addEvent, removeEvent, clearDay, cloneDay, save } =
    useSchedule();

  const today = DAY_NAMES[new Date().getDay()];
  const [selected, setSelected] = useState<DayName>(today);

  const [newTime, setNewTime] = useState('06:00');
  const [newSetpoint, setNewSetpoint] = useState(68);

  const [copying, setCopying] = useState(false);
  const [targets, setTargets] = useState<DayName[]>([]);

  // Close the copy panel when moving to another day
  useEffect(() => {
    setCopying(false);
    setTargets([]);
  }, [selected]);

  // Browser reload/close warning; leaving through the back link is handled in its click handler
  useEffect(() => {
    if (!dirty) return;
    const warn = (e: BeforeUnloadEvent) => e.preventDefault();
    window.addEventListener('beforeunload', warn);
    return () => window.removeEventListener('beforeunload', warn);
  }, [dirty]);

  const day = days?.find((d) => d.day === selected);
  const events = day?.events ?? [];
  const full = events.length >= MAX_EVENTS_PER_DAY;

  const toggleTarget = (target: DayName) =>
    setTargets((t) => (t.includes(target) ? t.filter((x) => x !== target) : [...t, target]));

  const copy = async () => {
    if (await cloneDay(selected, targets)) {
      setCopying(false);
      setTargets([]);
    }
  };

  return (
    <main className="schedule-screen">
      <header className="screen-header">
        <a
          className="back-link"
          href="#/"
          aria-label="Back to thermostat"
          onClick={(e) => {
            if (dirty && !window.confirm('Leave without saving the schedule?')) {
              e.preventDefault();
            }
          }}
        >
          <svg viewBox="0 0 24 24" aria-hidden="true">
            <path d="M15 5 8 12l7 7" />
          </svg>
        </a>
        <h1>Schedule</h1>
        <button className="save-button" disabled={!dirty || busy} onClick={save}>
          Save
        </button>
      </header>

      <div className="day-tabs" role="tablist" aria-label="Day of the week">
        {DAY_NAMES.map((name) => {
          const count = days?.find((d) => d.day === name)?.events.length ?? 0;
          return (
            <button
              key={name}
              role="tab"
              className={name === today ? 'day-tab day-tab--today' : 'day-tab'}
              aria-selected={name === selected}
              aria-label={longName(name)}
              onClick={() => setSelected(name)}
            >
              <span className="day-tab-name">{shortName(name)}</span>
              <span className="day-tab-count">{count || '·'}</span>
            </button>
          );
        })}
      </div>

      <section className="day-panel" aria-label={`${longName(selected)} events`}>
        <h2>{longName(selected)}</h2>

        {days === null ? (
          <p className="empty">Loading…</p>
        ) : events.length === 0 ? (
          <p className="empty">No events. The setpoint isn't changed on this day.</p>
        ) : (
          <ul className="event-list">
            {events.map((event) => (
              <li className="event" key={event.time}>
                <span className="event-time">{formatTime(event.time)}</span>
                <span className="event-setpoint">{event.setpoint}°</span>
                <button
                  className="icon-button"
                  aria-label={`Remove the ${formatTime(event.time)} event`}
                  disabled={busy}
                  onClick={() => removeEvent(selected, event.index)}
                >
                  <svg viewBox="0 0 24 24" aria-hidden="true">
                    <path d="M6 6l12 12M18 6 6 18" />
                  </svg>
                </button>
              </li>
            ))}
          </ul>
        )}

        <form
          className="add-event"
          onSubmit={(e) => {
            e.preventDefault();
            addEvent(selected, newTime, newSetpoint);
          }}
        >
          <input
            className="time-input"
            type="time"
            required
            aria-label="Event time"
            value={newTime}
            onChange={(e) => setNewTime(e.target.value)}
          />
          <div className="setpoint-stepper">
            <button
              type="button"
              className="icon-button"
              aria-label="Lower setpoint"
              disabled={newSetpoint <= SETPOINT_MIN}
              onClick={() => setNewSetpoint((s) => s - 1)}
            >
              −
            </button>
            <span className="setpoint-stepper-value" aria-label="Setpoint">
              {newSetpoint}°
            </span>
            <button
              type="button"
              className="icon-button"
              aria-label="Raise setpoint"
              disabled={newSetpoint >= SETPOINT_MAX}
              onClick={() => setNewSetpoint((s) => s + 1)}
            >
              +
            </button>
          </div>
          <button className="primary-button" type="submit" disabled={full || busy || !days}>
            Add
          </button>
        </form>
        {full && <p className="hint">A day can have up to {MAX_EVENTS_PER_DAY} events.</p>}

        <div className="day-actions">
          <button
            className="secondary-button"
            disabled={events.length === 0 || busy}
            onClick={() => setCopying((c) => !c)}
            aria-expanded={copying}
          >
            Copy to…
          </button>
          <button
            className="secondary-button"
            disabled={events.length === 0 || busy}
            onClick={() => clearDay(selected)}
          >
            Clear day
          </button>
        </div>

        {copying && (
          <div className="copy-panel">
            <p>Replace the events of these days with {longName(selected)}'s:</p>
            <div className="copy-targets">
              {DAY_NAMES.filter((name) => name !== selected).map((name) => (
                <label className="copy-target" key={name}>
                  <input
                    type="checkbox"
                    checked={targets.includes(name)}
                    onChange={() => toggleTarget(name)}
                  />
                  <span>{shortName(name)}</span>
                </label>
              ))}
            </div>
            <button
              className="primary-button"
              disabled={targets.length === 0 || busy}
              onClick={copy}
            >
              Copy
            </button>
          </div>
        )}
      </section>

      <p className={error ? 'status status--error' : 'status'} role="status">
        {error
          ? `Couldn't update the schedule (${error})`
          : dirty
            ? 'Changes are not saved to the thermostat yet. Press Save.'
            : ''}
      </p>
    </main>
  );
}
