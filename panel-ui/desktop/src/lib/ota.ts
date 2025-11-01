import dayjs from 'dayjs';

export type OtaProgress = {
  status: 'idle' | 'uploading' | 'verifying' | 'done' | 'error';
  detail?: string;
  startedAt?: string;
};

export class OtaManager {
  private progress: OtaProgress = { status: 'idle' };
  private listeners = new Set<(p: OtaProgress) => void>();

  get state() {
    return this.progress;
  }

  onChange(listener: (progress: OtaProgress) => void) {
    this.listeners.add(listener);
    listener(this.progress);
    return () => this.listeners.delete(listener);
  }

  async pushFirmware(target: 'panel' | 'amplifier', filePath: string) {
    this.update({ status: 'uploading', detail: `${target} → ${filePath}`, startedAt: dayjs().toISOString() });
    try {
      await window.panelSerial?.sendCli(`ota start ${target} ${filePath}`);
      this.update({ status: 'verifying' });
      await window.panelSerial?.sendCli('ota commit');
      this.update({ status: 'done' });
    } catch (err) {
      this.update({ status: 'error', detail: String(err) });
    }
  }

  reset() {
    this.update({ status: 'idle' });
  }

  private update(progress: OtaProgress) {
    this.progress = progress;
    this.listeners.forEach((listener) => listener(progress));
  }
}
