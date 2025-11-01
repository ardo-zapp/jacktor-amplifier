import React, { useEffect, useRef } from 'react';

type Props = {
  bars: number[];
};

const Analyzer: React.FC<Props> = ({ bars }) => {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) {
      return;
    }
    const ctx = canvas.getContext('2d');
    if (!ctx) {
      return;
    }
    const width = canvas.width;
    const height = canvas.height;
    ctx.clearRect(0, 0, width, height);
    const barWidth = width / bars.length;
    bars.forEach((value, index) => {
      const barHeight = Math.max(2, (value / 100) * height);
      const x = index * barWidth;
      const y = height - barHeight;
      const gradient = ctx.createLinearGradient(x, y, x, height);
      gradient.addColorStop(0, '#00E6FF');
      gradient.addColorStop(1, '#00CFFF33');
      ctx.fillStyle = gradient;
      ctx.fillRect(x + 2, y, barWidth - 4, barHeight);
    });
  }, [bars]);

  return <canvas ref={canvasRef} width={960} height={320} className="w-full h-64" />;
};

export default Analyzer;
