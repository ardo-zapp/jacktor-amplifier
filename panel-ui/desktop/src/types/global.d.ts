declare global {
  interface Window {
    panelSerial?: {
      onTelemetry: (cb: (frame: any) => void) => (() => void) | void;
      onCli: (cb: (line: string) => void) => (() => void) | void;
      onLink: (cb: (state: { online: boolean }) => void) => (() => void) | void;
      sendCli: (line: string) => void;
      readCache: () => Promise<unknown>;
      writeCache: (payload: unknown) => Promise<void>;
    };
  }
}

export {};
