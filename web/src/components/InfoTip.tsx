import { useId, useState } from 'react';

interface InfoTipProps {
  label: string;
  children: string;
}

// Information icon with a tooltip. Hover and keyboard focus show it through CSS;
// the click toggle is for touch screens, where hover doesn't exist.
export default function InfoTip({ label, children }: InfoTipProps) {
  const [open, setOpen] = useState(false);
  const id = useId();

  return (
    <span className={open ? 'info-tip info-tip--open' : 'info-tip'}>
      <button
        type="button"
        className="info-tip-button"
        aria-label={`About ${label}`}
        aria-describedby={id}
        aria-expanded={open}
        onClick={() => setOpen((o) => !o)}
        onBlur={() => setOpen(false)}
      >
        <svg viewBox="0 0 24 24" aria-hidden="true">
          <circle cx="12" cy="12" r="10" />
          <path d="M12 11v6" />
          <circle cx="12" cy="7.5" r="0.6" />
        </svg>
      </button>
      <span id={id} role="tooltip" className="info-tip-text">
        {children}
      </span>
    </span>
  );
}
