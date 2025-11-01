const { contextBridge, ipcRenderer } = require('electron');

const subscribe = (channel, cb) => {
  const handler = (_event, payload) => cb(payload);
  ipcRenderer.on(channel, handler);
  return () => ipcRenderer.removeListener(channel, handler);
};

contextBridge.exposeInMainWorld('panelSerial', {
  onTelemetry: (cb) => subscribe('serial:rx', cb),
  onCli: (cb) => subscribe('serial:cli', cb),
  onLink: (cb) => subscribe('serial:link', cb),
  sendCli: (line) => ipcRenderer.invoke('serial:tx', line),
  readCache: () => ipcRenderer.invoke('nvCache:read'),
  writeCache: (payload) => ipcRenderer.invoke('nvCache:write', payload)
});
