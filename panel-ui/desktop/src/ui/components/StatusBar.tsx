import React, { useEffect, useState } from 'react';
import dayjs from 'dayjs';
import { TelemetryFrame } from '../../lib/telemetry';
import { OtaManager } from '../../lib/ota';

const otaManager = new OtaManager();

const StatusBar: React.FC<{ frame: TelemetryFrame }> = ({ frame }) => {
  const [clock, setClock] = useState(() => dayjs());
  const [ota, setOta] = useState(otaManager.state);

  useEffect(() => {
    const id = setInterval(() => setClock(dayjs()), 1000);
    const unsub = otaManager.onChange(setOta);
    return () => {
      clearInterval(id);
      unsub();
    };
  }, []);

  return (
    <div className="bg-white/5 border border-primary/10 rounded-2xl p-4 h-full flex flex-col justify-between">
      <div className="flex justify-between text-sm">
        <div>
          <div className="uppercase text-xs tracking-[0.4em] text-primary/60">RTC</div>
          <div className="text-xl">{frame.rtc_c}</div>
        </div>
        <div>
          <div className="uppercase text-xs tracking-[0.4em] text-primary/60">Fan Mode</div>
          <div className="text-xl">{frame.fan_mode}{frame.fan_mode === 'CUSTOM' ? ` (${frame.fan_duty}%)` : ''}</div>
        </div>
      </div>
      <div className="flex justify-between text-sm">
        <div>
          <div className="uppercase text-xs tracking-[0.4em] text-primary/60">OTA</div>
          <div className="text-xl">{ota.status.toUpperCase()}</div>
        </div>
        <div>
          <div className="uppercase text-xs tracking-[0.4em] text-primary/60">BT Auto-Off</div>
          <div className="text-xl">{frame.bt_auto_off_s}s</div>
        </div>
      </div>
      <div className="flex justify-between text-sm text-primary/60">
        <span>{clock.format('HH:mm:ss')}</span>
        <button
          onClick={() => otaManager.reset()}
          className="text-xs uppercase tracking-[0.3em] bg-primary/20 px-3 py-1 rounded-full hover:bg-primary/40"
        >
          Reset OTA
        </button>
      </div>
    </div>
  );
};

export default StatusBar;
