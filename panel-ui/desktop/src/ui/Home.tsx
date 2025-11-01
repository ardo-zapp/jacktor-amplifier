import React from 'react';
import Analyzer from './components/Analyzer';
import Controls from './components/Controls';
import StatusCards from './components/StatusCards';
import LinkLeds from './components/LinkLeds';
import StatusBar from './components/StatusBar';
import { useTelemetry } from '../lib/telemetry';

type Props = {
  linkIndicators: {
    link: boolean;
    rx: boolean;
    tx: boolean;
  };
};

const Home: React.FC<Props> = ({ linkIndicators }) => {
  const { frame } = useTelemetry();

  return (
    <div className="w-full h-full grid grid-cols-12 gap-6 p-6 bg-background text-primary">
      <section className="col-span-8 bg-white/5 border border-primary/10 rounded-2xl p-4">
        <h2 className="font-display tracking-[0.4em] text-sm mb-4 text-primary/70 uppercase">Analyzer</h2>
        <Analyzer bars={frame.bars} />
      </section>
      <section className="col-span-4 flex flex-col gap-4">
        <div className="bg-white/5 border border-primary/10 rounded-2xl p-4 flex-1">
          <h2 className="font-display tracking-[0.4em] text-sm mb-4 text-primary/70 uppercase">Controls</h2>
          <Controls frame={frame} />
        </div>
        <div className="bg-white/5 border border-primary/10 rounded-2xl p-4">
          <h2 className="font-display tracking-[0.4em] text-sm mb-4 text-primary/70 uppercase">Link</h2>
          <LinkLeds indicators={linkIndicators} />
        </div>
      </section>
      <section className="col-span-8 bg-white/5 border border-primary/10 rounded-2xl p-4">
        <StatusCards frame={frame} />
      </section>
      <section className="col-span-4">
        <StatusBar frame={frame} />
      </section>
    </div>
  );
};

export default Home;
