import { MODES, Mode, SETPOINT_MAX, SETPOINT_MIN } from '../api';
import { useThermostat } from '../useThermostat';
import Gauge from './Gauge';

const MODE_LABELS: Record<Mode, string> = {
  off: 'Off',
  manual: 'Manual',
  schedule: 'Schedule',
};

export default function MainScreen() {
  const { state, error, setSetpoint, setMode } = useThermostat();
  const dash = '--';

  return (
    <main className="main-screen">
      <section
        className={[
          'dial',
          state ? `dial--${state.mode}` : '',
          state?.heater_on ? 'dial--heating' : '',
        ].join(' ')}
      >
        <Gauge
          min={SETPOINT_MIN}
          max={SETPOINT_MAX}
          current={state?.current_temp ?? null}
          setpoint={state?.setpoint ?? null}
        />
        <div className="dial-content">
          <div className="reading reading--current">
            <span className="reading-value">{state?.current_temp ?? dash}</span>
            <span className="reading-label">Current Temp</span>
          </div>

          <div className="reading reading--setpoint">
            <span className="reading-value">{state?.setpoint ?? dash}</span>
            <span className="reading-label">Setpoint</span>
          </div>

          <div className="sensors">
            <div className="sensor">
              <span className="sensor-label">Humidity %</span>
              <span className="sensor-value">{state?.humidity ?? dash}</span>
            </div>
            <div className="sensor">
              <span className="sensor-label">Pressure inHg</span>
              <span className="sensor-value">{state?.pressure ?? dash}</span>
            </div>
          </div>
        </div>
      </section>

      <div className="controls">
        <button
          className="step-button"
          aria-label="Lower setpoint"
          disabled={!state || state.setpoint <= SETPOINT_MIN}
          onClick={() => state && setSetpoint(state.setpoint - 1)}
        >
          −
        </button>

        <div className="mode-switch" role="group" aria-label="Mode">
          {MODES.map((mode) => (
            <button
              key={mode}
              className="mode-button"
              aria-pressed={state?.mode === mode}
              disabled={!state}
              onClick={() => setMode(mode)}
            >
              {MODE_LABELS[mode]}
            </button>
          ))}
        </div>

        <button
          className="step-button"
          aria-label="Raise setpoint"
          disabled={!state || state.setpoint >= SETPOINT_MAX}
          onClick={() => state && setSetpoint(state.setpoint + 1)}
        >
          +
        </button>
      </div>

      <p className={error ? 'status status--error' : 'status'} role="status">
        {error ? `Can't reach the thermostat (${error})` : state?.heater_on ? 'Heating' : ''}
      </p>
    </main>
  );
}
