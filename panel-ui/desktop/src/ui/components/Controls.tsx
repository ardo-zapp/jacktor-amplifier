import React, { useState } from 'react';
import { TelemetryFrame } from '../../lib/telemetry';

const buttonClass = 'px-4 py-2 rounded-xl uppercase tracking-[0.25em] border border-primary/30 hover:border-primary/60';

type Props = {
  frame: TelemetryFrame;
};

const Controls: React.FC<Props> = ({ frame }) => {
  const [fanDuty, setFanDuty] = useState(frame.fan_duty);

  const send = async (line: string) => {
    await window.panelSerial?.sendCli(line);
  };

  const togglePower = async () => {
    await send(`power ${frame.state === 'ON' ? 'off' : 'on'}`);
  };

  const toggleBt = async () => {
    await send(`bt ${frame.input === 'BT' ? 'disable' : 'enable'}`);
  };

  const sendTransport = async (cmd: 'play' | 'pause' | 'next' | 'prev') => {
    await send(`bt ${cmd}`);
  };

  const toggleSelector = async () => {
    await send(`set speaker-selector ${frame.selector === 'BIG' ? 'small' : 'big'}`);
  };

  const setFanMode = async (mode: 'AUTO' | 'CUSTOM' | 'FAILSAFE') => {
    await send(`fan mode ${mode.toLowerCase()}`);
  };

  const applyFanDuty = async (value: number) => {
    setFanDuty(value);
    await send(`fan duty ${value}`);
  };

  const toggleSmpsProtect = async () => {
    await send('smps protect toggle');
  };

  return (
    <div className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <button className={`${buttonClass} ${frame.state === 'ON' ? 'bg-primary/30' : ''}`} onClick={togglePower}>
          Power {frame.state === 'ON' ? 'ON' : 'STBY'}
        </button>
        <button className={`${buttonClass} ${frame.input === 'BT' ? 'bg-accent/30' : ''}`} onClick={toggleBt}>
          BT {frame.input === 'BT' ? 'Enabled' : 'Disabled'}
        </button>
      </div>
      <div className="grid grid-cols-4 gap-3 text-xs">
        {['play', 'pause', 'next', 'prev'].map((action) => (
          <button key={action} className={buttonClass} onClick={() => sendTransport(action as any)}>
            {action}
          </button>
        ))}
      </div>
      <div className="grid grid-cols-2 gap-4">
        <button className={buttonClass} onClick={toggleSelector}>
          Speaker {frame.selector === 'BIG' ? 'BIG' : 'SMALL'}
        </button>
        <button className={buttonClass} onClick={toggleSmpsProtect}>
          SMPS Protect
        </button>
      </div>
      <div className="space-y-2">
        <div className="flex gap-3">
          {(['AUTO', 'CUSTOM', 'FAILSAFE'] as const).map((mode) => (
            <button
              key={mode}
              className={`${buttonClass} ${frame.fan_mode === mode ? 'bg-primary/30' : ''}`}
              onClick={() => setFanMode(mode)}
            >
              Fan {mode}
            </button>
          ))}
        </div>
        {frame.fan_mode === 'CUSTOM' && (
          <div className="flex items-center gap-4">
            <input
              type="range"
              min={0}
              max={100}
              value={fanDuty}
              onChange={(e) => setFanDuty(Number(e.target.value))}
              onMouseUp={(e) => applyFanDuty(Number((e.target as HTMLInputElement).value))}
              className="w-full"
            />
            <span className="text-sm w-16 text-right">{fanDuty}%</span>
          </div>
        )}
      </div>
      <button className={buttonClass} onClick={() => send('smps protect status')}>
        SMPS Protect Status
      </button>
    </div>
  );
};

export default Controls;
