import React from 'react';
import { TelemetryFrame } from '../../lib/telemetry';

const StatusCards: React.FC<{ frame: TelemetryFrame }> = ({ frame }) => {
  const cards = [
    { label: 'Heat', value: `${frame.heat_c.toFixed(1)}°C` },
    { label: 'RTC', value: frame.rtc_c },
    { label: 'SMPS', value: `${frame.smps_v.toFixed(1)} V` },
    { label: 'Input', value: frame.input },
    { label: 'State', value: frame.state },
  ];
  return (
    <div className="grid grid-cols-5 gap-4">
      {cards.map((card) => (
        <div key={card.label} className="bg-black/40 rounded-2xl border border-primary/10 p-4">
          <div className="text-primary/60 uppercase text-xs tracking-[0.4em]">{card.label}</div>
          <div className="text-2xl font-semibold text-primary">{card.value}</div>
        </div>
      ))}
      <div className="col-span-5 bg-black/40 rounded-2xl border border-primary/10 p-4">
        <div className="text-primary/60 uppercase text-xs tracking-[0.4em] mb-2">Errors</div>
        <div className="flex flex-wrap gap-2 text-xs">
          {frame.error_flags.length === 0 && <span className="text-primary/40">No faults</span>}
          {frame.error_flags.map((flag) => (
            <span key={flag} className="px-3 py-1 rounded-full bg-error/40 text-error uppercase tracking-[0.3em]">
              {flag}
            </span>
          ))}
        </div>
      </div>
    </div>
  );
};

export default StatusCards;
