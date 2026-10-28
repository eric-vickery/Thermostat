import { useEffect, useState } from 'react';
import type { Settings } from '../api';
import { useSettings } from '../useSettings';
import InfoTip from './InfoTip';

interface SettingDef {
  key: keyof Settings;
  label: string;
  help: string;
  control: 'slider' | 'number';
  min: number;
  max: number;
  unit: string;
}

const SETTINGS: SettingDef[] = [
  {
    key: 'temperature_hysteresis',
    label: 'Temperature hysteresis',
    help:
      'Heat turns on when the room is this many degrees below the setpoint and turns off when ' +
      'it is this many degrees above. Higher values switch the heater less often but let the ' +
      'temperature swing more.',
    control: 'slider',
    min: 0,
    max: 4,
    unit: '°',
  },
  {
    key: 'inactivity_timeout_s',
    label: 'Inactivity timeout',
    help:
      'How long the thermostat screen can go untouched before it switches to the idle ' +
      'screen and dims. 0 means it never goes idle.',
    control: 'number',
    min: 0,
    max: 3600,
    unit: 's',
  },
  {
    key: 'active_brightness',
    label: 'Active brightness',
    help: 'Screen brightness while the thermostat is in use.',
    control: 'slider',
    min: 1,
    max: 100,
    unit: '%',
  },
  {
    key: 'idle_brightness',
    label: 'Idle brightness',
    help: 'Screen brightness while the idle screen is showing. 0 turns the backlight off.',
    control: 'slider',
    min: 0,
    max: 100,
    unit: '%',
  },
  {
    key: 'altitude_ft',
    label: 'Altitude',
    help:
      'Your elevation above sea level. The pressure reading is corrected to sea level using ' +
      'it, so it matches weather reports instead of showing the lower pressure of high ground.',
    control: 'number',
    min: -1500,
    max: 20000,
    unit: 'ft',
  },
];

interface NumberFieldProps {
  id: string;
  value: number;
  min: number;
  max: number;
  onCommit: (value: number) => void;
}

// Keeps what the user is typing as text and only commits a valid, in-range number
function NumberField({ id, value, min, max, onCommit }: NumberFieldProps) {
  const [draft, setDraft] = useState(String(value));

  useEffect(() => setDraft(String(value)), [value]);

  const commit = () => {
    const parsed = Math.round(Number(draft));
    if (draft.trim() === '' || Number.isNaN(parsed)) {
      setDraft(String(value));
      return;
    }
    const clamped = Math.min(max, Math.max(min, parsed));
    setDraft(String(clamped));
    if (clamped !== value) onCommit(clamped);
  };

  return (
    <input
      id={id}
      className="setting-number"
      type="number"
      inputMode="numeric"
      min={min}
      max={max}
      value={draft}
      onChange={(e) => setDraft(e.target.value)}
      onBlur={commit}
      onKeyDown={(e) => e.key === 'Enter' && e.currentTarget.blur()}
    />
  );
}

export default function ConfigScreen() {
  const { settings, error, update } = useSettings();

  return (
    <main className="config-screen">
      <header className="screen-header">
        <a className="back-link" href="#/" aria-label="Back to thermostat">
          <svg viewBox="0 0 24 24" aria-hidden="true">
            <path d="M15 5 8 12l7 7" />
          </svg>
        </a>
        <h1>Settings</h1>
      </header>

      <ul className="setting-list">
        {SETTINGS.map((def) => {
          const id = `setting-${def.key}`;
          const value = settings?.[def.key];
          return (
            <li className="setting" key={def.key}>
              <div className="setting-heading">
                <label htmlFor={id}>{def.label}</label>
                <InfoTip label={def.label}>{def.help}</InfoTip>
              </div>

              <div className="setting-control">
                {def.control === 'slider' ? (
                  <input
                    id={id}
                    className="setting-slider"
                    type="range"
                    min={def.min}
                    max={def.max}
                    step={1}
                    disabled={value === undefined}
                    value={value ?? def.min}
                    onChange={(e) => update({ [def.key]: Number(e.target.value) })}
                  />
                ) : value === undefined ? (
                  <span className="setting-number setting-number--empty">--</span>
                ) : (
                  <NumberField
                    id={id}
                    value={value}
                    min={def.min}
                    max={def.max}
                    onCommit={(v) => update({ [def.key]: v })}
                  />
                )}
                <span className="setting-value">
                  {def.control === 'slider' ? `${value ?? '--'}${def.unit}` : def.unit}
                </span>
              </div>
            </li>
          );
        })}
      </ul>

      <p className={error ? 'status status--error' : 'status'} role="status">
        {error ? `Can't reach the thermostat (${error})` : ''}
      </p>
    </main>
  );
}
