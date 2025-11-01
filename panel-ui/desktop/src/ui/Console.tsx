import React, { useEffect, useRef, useState } from 'react';
import LinkLeds from './components/LinkLeds';

type Props = {
  log: string[];
  onSend: (line: string) => void;
  linkIndicators: {
    link: boolean;
    rx: boolean;
    tx: boolean;
  };
};

const ConsoleView: React.FC<Props> = ({ log, onSend, linkIndicators }) => {
  const [input, setInput] = useState('');
  const containerRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    containerRef.current?.scrollTo({ top: containerRef.current.scrollHeight, behavior: 'smooth' });
  }, [log]);

  const saveLog = () => {
    const blob = new Blob([log.join('\n')], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = `jacktor-console-${Date.now()}.txt`;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
  };

  const submit = (evt: React.FormEvent) => {
    evt.preventDefault();
    onSend(input);
    setInput('');
  };

  return (
    <div className="h-full flex flex-col bg-background text-primary">
      <div className="flex items-center justify-between px-6 py-4 border-b border-primary/20">
        <h1 className="font-display tracking-[0.4em] text-sm uppercase text-primary/70">Diagnostics Console</h1>
        <div className="flex gap-4 items-center">
          <LinkLeds indicators={linkIndicators} />
          <button
            onClick={saveLog}
            className="bg-primary/20 hover:bg-primary/40 px-4 py-2 rounded-xl uppercase tracking-[0.3em]"
          >
            Save Log
          </button>
        </div>
      </div>
      <div ref={containerRef} className="flex-1 overflow-y-auto p-6 space-y-1 font-mono text-xs bg-black/40 scrollbar-thin">
        {log.map((line, idx) => (
          <div key={idx} className={line.startsWith('>') ? 'text-accent' : 'text-primary/80'}>
            {line}
          </div>
        ))}
      </div>
      <form onSubmit={submit} className="p-4 flex gap-4 border-t border-primary/20">
        <input
          className="flex-1 bg-black/40 px-4 py-2 rounded-xl border border-primary/30 text-primary"
          value={input}
          onChange={(e) => setInput(e.target.value)}
          placeholder="Ketik perintah CLI..."
        />
        <button type="submit" className="bg-primary/30 hover:bg-primary/50 px-6 py-2 rounded-xl uppercase tracking-[0.3em]">
          Send
        </button>
      </form>
    </div>
  );
};

export default ConsoleView;
