import { EventEmitter } from 'events';
import { SerialPort } from 'serialport';
import { ReadlineParser } from '@serialport/parser-readline';

const TELEMETRY_PREFIX = 'telemetry:';

export class SerialGateway extends EventEmitter {
  constructor() {
    super();
    this.port = null;
    this.parser = null;
    this.lastRx = 0;
    this.heartbeatTimer = undefined;
  }

  async start() {
    await this.connect();
    this.heartbeatTimer = setInterval(() => {
      const online = Date.now() - this.lastRx < 3000;
      this.emit('link', { online });
    }, 500);
  }

  async connect() {
    const ports = await SerialPort.list();
    const candidate = ports.find((p) => /panel/i.test(p.productId ?? '') || /panel/i.test(p.manufacturer ?? '')) || ports[0];
    if (!candidate || !candidate.path) {
      return;
    }
    this.port = new SerialPort({ path: candidate.path, baudRate: 115200 });
    this.parser = this.port.pipe(new ReadlineParser({ delimiter: '\n' }));
    this.parser.on('data', (line) => this.handleLine(line));
    this.port.on('error', (err) => {
      console.error('Serial error', err);
      this.reset();
      setTimeout(() => this.connect(), 2000);
    });
  }

  handleLine(line) {
    this.lastRx = Date.now();
    const trimmed = line.trim();
    if (!trimmed) {
      return;
    }
    if (trimmed.startsWith(TELEMETRY_PREFIX)) {
      try {
        const payload = JSON.parse(trimmed.slice(TELEMETRY_PREFIX.length));
        this.emit('telemetry', payload);
      } catch (err) {
        console.warn('Bad telemetry payload', err);
      }
      return;
    }
    if (trimmed.startsWith('{') || trimmed.startsWith('[')) {
      try {
        this.emit('telemetry', JSON.parse(trimmed));
      } catch (err) {
        console.warn('Malformed JSON line', err);
      }
      return;
    }
    this.emit('cli', trimmed);
  }

  async sendCli(line) {
    if (!this.port || !this.port.writable) {
      throw new Error('Serial port not ready');
    }
    await new Promise((resolve, reject) => {
      this.port.write(`${line.trim()}\n`, (err) => (err ? reject(err) : resolve()));
    });
    this.emit('link', { online: true });
  }

  reset() {
    this.port?.removeAllListeners();
    this.parser?.removeAllListeners();
    this.port?.destroy();
    this.port = null;
    this.parser = null;
  }

  stop() {
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer);
    }
    this.reset();
  }
}
