import React, { createContext, useContext, useEffect, useMemo, useState } from 'react';
import { z } from 'zod';

type TelemetryState = ReturnType<typeof createEmptyTelemetry>;

export type TelemetryFrame = ReturnType<typeof telemetrySchema.parse>;

const telemetrySchema = z.object({
  ts: z.number().optional(),
  heat_c: z.number().default(0),
  rtc_c: z.string().default('--:--'),
  smps_v: z.number().default(0),
  input: z.enum(['BT', 'AUX']).default('AUX'),
  state: z.enum(['ON', 'STANDBY']).default('STANDBY'),
  error_flags: z.array(z.string()).default([]),
  fan_mode: z.enum(['AUTO', 'CUSTOM', 'FAILSAFE']).default('AUTO'),
  fan_duty: z.number().min(0).max(100).default(0),
  bt_auto_off_s: z.number().default(0),
  selector: z.enum(['BIG', 'SMALL']).default('SMALL'),
  bars: z.array(z.number()).length(16).default(new Array(16).fill(0)),
});

const TelemetryContext = createContext<TelemetryState | undefined>(undefined);

function createEmptyTelemetry() {
  return {
    frame: telemetrySchema.parse({}),
    updatedAt: 0,
  };
}

export const TelemetryProvider: React.FC<React.PropsWithChildren> = ({ children }) => {
  const [state, setState] = useState(createEmptyTelemetry);

  useEffect(() => {
    const serial = window.panelSerial;
    if (!serial) {
      return;
    }
    const dispose = serial.onTelemetry((raw) => {
      try {
        const parsed = telemetrySchema.parse(raw);
        setState({ frame: parsed, updatedAt: Date.now() });
      } catch (err) {
        console.warn('Invalid telemetry frame', err);
      }
    });
    return () => {
      dispose && dispose();
    };
  }, []);

  const value = useMemo(() => state, [state]);
  return <TelemetryContext.Provider value={value}>{children}</TelemetryContext.Provider>;
};

export const useTelemetry = () => {
  const ctx = useContext(TelemetryContext);
  if (!ctx) {
    throw new Error('useTelemetry must be used within TelemetryProvider');
  }
  return ctx;
};
