import type { CSSProperties } from 'react';

// Half circle over the top, from 9 o'clock to 3 o'clock. Angles run clockwise from 12 o'clock.
const SIZE = 200;
const CENTER = SIZE / 2;
const RADIUS = 88;
const START_ANGLE = -90;
const END_ANGLE = 90;

function pointAt(angle: number): string {
  const rad = (angle * Math.PI) / 180;
  return `${CENTER + RADIUS * Math.sin(rad)} ${CENTER - RADIUS * Math.cos(rad)}`;
}

const ARC_PATH = `M ${pointAt(START_ANGLE)} A ${RADIUS} ${RADIUS} 0 0 1${pointAt(END_ANGLE)}`;

interface GaugeProps {
  min: number;
  max: number;
  current: number | null;
  setpoint: number | null;
}

function fraction(value: number | null, min: number, max: number): number {
  if (value === null) return 0;
  return Math.min(1, Math.max(0, (value - min) / (max - min)));
}

// Only geometry lives here. Every color, width and size comes from main-screen.css
// through the classes below and the --fraction/--angle custom properties.
export default function Gauge({ min, max, current, setpoint }: GaugeProps) {
  const currentFraction = fraction(current, min, max);
  const setpointFraction = fraction(setpoint, min, max);
  const angleOf = (f: number) => START_ANGLE + f * (END_ANGLE - START_ANGLE);

  const style = (f: number) =>
    ({ '--fraction': f, '--angle': `${angleOf(f)}deg` }) as CSSProperties;

  return (
    <svg className="gauge" viewBox={`0 0 ${SIZE} ${SIZE}`} aria-hidden="true">
      <path className="gauge-track" d={ARC_PATH} pathLength={100} />
      <path className="gauge-fill" d={ARC_PATH} pathLength={100} style={style(currentFraction)} />
      {/* r is also set as an attribute because not every browser applies the CSS r property;
          main-screen.css overrides it where it does */}
      {/* The setpoint ring is drawn last, on top, so it stays visible around the current knob */}
      <g className="gauge-marker gauge-marker--current" style={style(currentFraction)}>
        <circle cx={CENTER} cy={CENTER - RADIUS} r={9.5} />
      </g>
      <g className="gauge-marker gauge-marker--setpoint" style={style(setpointFraction)}>
        <circle cx={CENTER} cy={CENTER - RADIUS} r={10} />
      </g>
    </svg>
  );
}
