import React from 'react';

type Props = {
  indicators: {
    link: boolean;
    rx: boolean;
    tx: boolean;
  };
};

const LinkLeds: React.FC<Props> = ({ indicators }) => (
  <div className="flex gap-4 items-center">
    <div className="flex items-center gap-2">
      <span className="text-xs uppercase tracking-[0.3em] text-primary/60">Link</span>
      <span className={`w-4 h-4 rounded-full border border-primary/40 ${indicators.link ? 'bg-primary shadow-[0_0_10px_#00CFFF]' : ''}`} />
    </div>
    <div className="flex items-center gap-2">
      <span className="text-xs uppercase tracking-[0.3em] text-primary/60">RX</span>
      <span className={`w-4 h-4 rounded-full border border-accent/40 ${indicators.rx ? 'bg-accent shadow-[0_0_10px_#00E6FF]' : ''}`} />
    </div>
    <div className="flex items-center gap-2">
      <span className="text-xs uppercase tracking-[0.3em] text-primary/60">TX</span>
      <span className={`w-4 h-4 rounded-full border border-accent/40 ${indicators.tx ? 'bg-accent shadow-[0_0_10px_#00E6FF]' : ''}`} />
    </div>
  </div>
);

export default LinkLeds;
