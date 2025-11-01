import React, { useState } from 'react';
import { useNvSync } from '../lib/nvSync';

const pinRegex = /^[0-9]{4,6}$/;

type Props = {
  openConsole: () => void;
};

const Settings: React.FC<Props> = ({ openConsole }) => {
  const { refresh } = useNvSync();
  const [pin, setPin] = useState('');
  const [confirmPin, setConfirmPin] = useState('');
  const [toneMode, setToneMode] = useState<'simple' | 'sequence' | 'musical' | 'randomizer'>('simple');
  const [status, setStatus] = useState('');

  const handleFactoryReset = async () => {
    if (!pinRegex.test(pin) || pin !== confirmPin) {
      setStatus('PIN harus 4-6 digit dan sama pada kedua field');
      return;
    }
    await window.panelSerial?.sendCli(`reset nvs --force --pin ${pin}`);
    setStatus('Factory reset dikirim');
  };

  const handleTone = async () => {
    const payload = JSON.stringify({ type: toneMode, ts: Date.now() });
    await window.panelSerial?.sendCli(`tone ${payload}`);
    setStatus(`Tone ${toneMode} dikirim`);
  };

  const handleRtcSync = async () => {
    const now = new Date();
    await window.panelSerial?.sendCli(`rtc sync ${now.toISOString()}`);
    setStatus('RTC sync diminta');
  };

  return (
    <div className="p-6 grid grid-cols-12 gap-6 bg-background text-primary min-h-full">
      <section className="col-span-6 bg-white/5 border border-primary/10 rounded-2xl p-6 space-y-4">
        <h2 className="font-display tracking-[0.4em] text-sm text-primary/70 uppercase">Factory Reset</h2>
        <p className="text-primary/60 text-sm">PIN wajib, 4-6 digit. Reset menghapus NVS amplifier & panel.</p>
        <div className="flex gap-4">
          <input
            className="bg-black/40 px-4 py-2 rounded-xl border border-primary/20 text-primary"
            placeholder="PIN"
            value={pin}
            onChange={(e) => setPin(e.target.value)}
          />
          <input
            className="bg-black/40 px-4 py-2 rounded-xl border border-primary/20 text-primary"
            placeholder="Konfirmasi PIN"
            value={confirmPin}
            onChange={(e) => setConfirmPin(e.target.value)}
          />
        </div>
        <button
          onClick={handleFactoryReset}
          className="bg-error/80 hover:bg-error text-black font-semibold px-6 py-2 rounded-xl uppercase tracking-widest"
        >
          Reset NVS
        </button>
      </section>
      <section className="col-span-6 bg-white/5 border border-primary/10 rounded-2xl p-6 space-y-4">
        <h2 className="font-display tracking-[0.4em] text-sm text-primary/70 uppercase">RTC Sync</h2>
        <button
          onClick={handleRtcSync}
          className="bg-primary/20 hover:bg-primary/40 px-6 py-3 rounded-xl uppercase tracking-[0.4em]"
        >
          Sync Sekarang
        </button>
      </section>
      <section className="col-span-6 bg-white/5 border border-primary/10 rounded-2xl p-6 space-y-4">
        <h2 className="font-display tracking-[0.4em] text-sm text-primary/70 uppercase">Tone Lab</h2>
        <div className="flex gap-4">
          {(['simple', 'sequence', 'musical', 'randomizer'] as const).map((mode) => (
            <button
              key={mode}
              className={`px-4 py-2 rounded-xl uppercase tracking-[0.3em] border ${
                toneMode === mode ? 'bg-accent/50 border-accent' : 'border-primary/20'
              }`}
              onClick={() => setToneMode(mode)}
            >
              {mode}
            </button>
          ))}
        </div>
        <button
          onClick={handleTone}
          className="bg-accent/40 hover:bg-accent/60 px-6 py-2 rounded-xl uppercase tracking-[0.3em]"
        >
          Kirim Tone
        </button>
      </section>
      <section className="col-span-6 bg-white/5 border border-primary/10 rounded-2xl p-6 space-y-4">
        <h2 className="font-display tracking-[0.4em] text-sm text-primary/70 uppercase">Diagnostics</h2>
        <div className="flex gap-4">
          <button
            onClick={refresh}
            className="bg-primary/20 hover:bg-primary/40 px-6 py-2 rounded-xl uppercase tracking-[0.3em]"
          >
            Refresh Cache
          </button>
          <button
            onClick={openConsole}
            className="bg-primary/40 hover:bg-primary/60 px-6 py-2 rounded-xl uppercase tracking-[0.3em]"
          >
            Open Console
          </button>
        </div>
      </section>
      {status && (
        <div className="col-span-12 text-sm text-accent/80">{status}</div>
      )}
    </div>
  );
};

export default Settings;
