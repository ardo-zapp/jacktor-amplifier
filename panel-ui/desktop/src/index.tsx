import React, { useEffect, useMemo, useState } from 'react';
import ReactDOM from 'react-dom/client';
import './app.css';
import Home from './ui/Home';
import Settings from './ui/Settings';
import ConsoleView from './ui/Console';
import { NvSyncProvider } from './lib/nvSync';
import { ThemeContext, baseTheme } from './lib/theme';
import { TelemetryFrame, TelemetryProvider } from './lib/telemetry';

type Page = 'home' | 'settings' | 'console';

const App: React.FC = () => {
  const [page, setPage] = useState<Page>('home');
  const [cliLog, setCliLog] = useState<string[]>([]);
  const [linkState, setLinkState] = useState({ online: false, lastRx: 0 });
  const [rxBlink, setRxBlink] = useState(false);
  const [txBlink, setTxBlink] = useState(false);

  const serial = window.panelSerial;

  useEffect(() => {
    if (!serial) {
      return;
    }

    const disposeCli = serial.onCli((line) => {
      setCliLog((log) => [...log, line].slice(-500));
      setRxBlink(true);
      setTimeout(() => setRxBlink(false), 200);
    });

    const disposeLink = serial.onLink((state) => {
      setLinkState({ online: state.online, lastRx: Date.now() });
    });
    return () => {
      disposeCli && disposeCli();
      disposeLink && disposeLink();
    };
  }, [serial]);

  const handleSendCli = (line: string) => {
    if (!line.trim()) {
      return;
    }
    serial?.sendCli(line);
    setCliLog((log) => [...log, `> ${line}`].slice(-500));
    setTxBlink(true);
    setTimeout(() => setTxBlink(false), 200);
  };

  const linkIndicators = useMemo(
    () => ({
      link: linkState.online && Date.now() - linkState.lastRx < 3000,
      rx: rxBlink,
      tx: txBlink,
    }),
    [linkState, rxBlink, txBlink]
  );

  return (
    <ThemeContext.Provider value={baseTheme}>
      <NvSyncProvider>
        <TelemetryProvider>
          <div className="min-h-screen flex flex-col bg-background text-primary">
            <nav className="flex justify-between items-center px-6 py-4 border-b border-primary/20">
              <div className="flex gap-4 text-primary/70 uppercase tracking-widest text-sm">
                <button
                  className={`hover:text-primary transition ${page === 'home' ? 'text-primary' : ''}`}
                  onClick={() => setPage('home')}
                >
                  Home
                </button>
                <button
                  className={`hover:text-primary transition ${page === 'settings' ? 'text-primary' : ''}`}
                  onClick={() => setPage('settings')}
                >
                  Settings
                </button>
                <button
                  className={`hover:text-primary transition ${page === 'console' ? 'text-primary' : ''}`}
                  onClick={() => setPage('console')}
                >
                  Console
                </button>
              </div>
            </nav>
            <main className="flex-1 overflow-hidden">
              {page === 'home' && <Home linkIndicators={linkIndicators} />}
              {page === 'settings' && <Settings openConsole={() => setPage('console')} />}
              {page === 'console' && (
                <ConsoleView
                  log={cliLog}
                  onSend={handleSendCli}
                  linkIndicators={linkIndicators}
                />
              )}
            </main>
          </div>
        </TelemetryProvider>
      </NvSyncProvider>
    </ThemeContext.Provider>
  );
};

ReactDOM.createRoot(document.getElementById('root') as HTMLElement).render(<App />);
