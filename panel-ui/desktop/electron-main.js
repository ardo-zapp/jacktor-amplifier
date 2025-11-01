import { app, BrowserWindow, ipcMain } from 'electron';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import path from 'path';
import { SerialGateway } from './electron-serial.js';

let mainWindow;
const serialGateway = new SerialGateway();

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const cacheFile = () => path.join(app.getPath('userData'), 'nv-cache.json');

const createWindow = () => {
  mainWindow = new BrowserWindow({
    width: 1280,
    height: 720,
    backgroundColor: '#080B0E',
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: path.join(__dirname, 'src', 'preload.cjs')
    }
  });

  const startUrl = app.isPackaged
    ? `file://${path.join(__dirname, 'dist', 'index.html')}`
    : 'http://localhost:5173';

  mainWindow.loadURL(startUrl);
  mainWindow.on('closed', () => {
    mainWindow = null;
  });
};

app.whenReady().then(() => {
  createWindow();
  serialGateway.start();
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) {
    createWindow();
  }
});

ipcMain.handle('serial:tx', async (_event, payload) => {
  await serialGateway.sendCli(payload);
});

ipcMain.handle('nvCache:read', async () => {
  try {
    if (fs.existsSync(cacheFile())) {
      return JSON.parse(fs.readFileSync(cacheFile(), 'utf-8'));
    }
  } catch (err) {
    console.warn('Failed to read cache', err);
  }
  return null;
});

ipcMain.handle('nvCache:write', async (_event, payload) => {
  try {
    fs.writeFileSync(cacheFile(), JSON.stringify(payload, null, 2));
  } catch (err) {
    console.warn('Failed to write cache', err);
  }
});

serialGateway.on('telemetry', (frame) => {
  mainWindow?.webContents.send('serial:rx', frame);
});

serialGateway.on('cli', (line) => {
  mainWindow?.webContents.send('serial:cli', line);
});

serialGateway.on('link', (state) => {
  mainWindow?.webContents.send('serial:link', state);
});
