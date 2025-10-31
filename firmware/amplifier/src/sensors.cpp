#include "sensors.h"
#include "config.h"

#include <Wire.h>
#include <RTClib.h>
#include <cstring>

// ====== ADS1115 (voltmeter) ======
#include <Adafruit_ADS1X15.h>
static Adafruit_ADS1115 ads;

// Helper konversi ADC→Volt riil via divider
static inline float adcToRealVolt(float vAdc) {
  // vReal = vAdc * (R1+R2)/R2
  return vAdc * ((R1_OHMS + R2_OHMS) / R2_OHMS);
}

// Nilai terakhir (langsung, tanpa smoothing)
static float gVoltInstant = 0.0f;

// ====== DS18B20 (heatsink) ======
#include <OneWire.h>
#include <DallasTemperature.h>

static OneWire         oneWire(DS18B20_PIN);
static DallasTemperature dallas(&oneWire);
static float           gHeatC = NAN;
static uint32_t        lastTempMs = 0;

// ====== RTC DS3231 ======
static RTC_DS3231 rtc;
static bool       rtcReady = false;
static volatile bool rtcSqwTick = false;
static float      rtcTempC = NAN;

static void IRAM_ATTR onRtcSqw() {
  rtcSqwTick = true;
}

// ====== Analyzer (I²S ADC internal → FFT) ======
#include <driver/i2s.h>
#include <arduinoFFT.h>

static ArduinoFFT<double> FFT;
static bool     i2sReady = false;
static bool     gAnalyzerEn = true;

static double   vReal[ANA_N];
static double   vImag[ANA_N];
static uint16_t sampCount = 0;

static uint8_t  bandsOut[ANA_BANDS];   // 0..255
static bool     bandsInit = false;

static int      bandBins[ANA_BANDS + 1];
static uint32_t lastFftMs = 0;

// Window Hann
static inline double hann(uint16_t n, uint16_t N) {
  return 0.5 * (1.0 - cos((2.0 * PI * n) / (double)(N - 1)));
}

static inline int freqToBin(double f, double fs, int n) {
  int b = (int) round((f * n) / fs);
  if (b < 1) b = 1;
  if (b > n/2 - 1) b = n/2 - 1;
  return b;
}

static void makeBandBoundaries() {
  double f0 = (double)ANA_F_LO_HZ;
  double f1 = (double)ANA_F_HI_HZ;
  for (int i = 0; i <= ANA_BANDS; ++i) {
    double t = (double)i / (double)ANA_BANDS;
    double f = f0 * pow(f1 / f0, t);      // log-spaced
    bandBins[i] = freqToBin(f, (double)ANA_FS_HZ, ANA_N);
  }
  // jaga monotonik dan batas
  for (int i = 1; i <= ANA_BANDS; ++i) {
    if (bandBins[i] <= bandBins[i-1]) bandBins[i] = bandBins[i-1] + 1;
    if (bandBins[i] > ANA_N/2 - 1)    bandBins[i] = ANA_N/2 - 1;
  }
  bandsInit = true;
}

// I²S ADC internal setup
static bool i2sSetup() {
#if I2S_USE_BUILTIN_ADC
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN);
  cfg.sample_rate = ANA_FS_HZ;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT; // mono
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = false;
  cfg.fixed_mclk = 0;

  if (i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) != ESP_OK) return false;
  if (i2s_set_adc_mode(ADC_UNIT_1, ADC1_CHANNEL_0) != ESP_OK)   return false; // GPIO36
  if (i2s_adc_enable(I2S_PORT) != ESP_OK)                       return false;

  // ADC atten default; gunakan default 11dB internal agar headroom luas
  // (konfigurasi atten ADC internal via driver ADC jika diperlukan)
  return true;
#else
  return false;
#endif
}

// Ambil sampel dari I2S ke vReal/vImag; non-blocking-ish
static void analyzerSample() {
  if (!i2sReady) return;
  if (!gAnalyzerEn) return;
  if (sampCount >= ANA_N) return;

  // Baca chunk kecil
  int16_t buf[128];
  size_t  br = 0;
  if (i2s_read(I2S_PORT, (void*)buf, sizeof(buf), &br, 0) != ESP_OK) return;
  int n16 = br / sizeof(int16_t);
  for (int i = 0; i < n16 && sampCount < ANA_N; ++i) {
    // Nilai raw ADC 12-bit terekspansi ke 16-bit; pusatkan di 0
    // Skala ke double agar FFT stabil
    double s = (double)buf[i]; // sudah signed
    vReal[sampCount] = s;
    vImag[sampCount] = 0.0;
    ++sampCount;
  }
}

// Proses FFT → isi bandsOut (0..255)
static void analyzerProcess(uint32_t now) {
  if (!bandsInit) makeBandBoundaries();
  if (sampCount < ANA_N) return;
  if (now - lastFftMs < ANA_UPDATE_MS) return;
  lastFftMs = now;

  // Window Hann
  for (uint16_t i = 0; i < ANA_N; ++i) {
    vReal[i] *= hann(i, ANA_N);
  }

  FFT = ArduinoFFT<double>(vReal, vImag, ANA_N, (double)ANA_FS_HZ);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();

  // Agregasi band log-spaced (average magnitude)
  for (int b = 0; b < ANA_BANDS; ++b) {
    int k1 = bandBins[b];
    int k2 = bandBins[b + 1];
    if (k2 <= k1) k2 = k1 + 1;

    double sum = 0.0;
    int    cnt = 0;
    for (int k = k1; k <= k2; ++k) {
      sum += vReal[k];
      ++cnt;
    }
    double avg = (cnt > 0) ? (sum / (double)cnt) : 0.0;

    // Kompresi log sederhana (tanpa “noise floor” kustom)
    double mag = log10(1.0 + avg) * 64.0;   // skala empiris
    if (mag < 0.0) mag = 0.0;
    if (mag > 255.0) mag = 255.0;

    bandsOut[b] = (uint8_t) (mag + 0.5);
  }

  // Siap siklus berikutnya
  sampCount = 0;
}

// Hitung VU mono dari energi keseluruhan (RMS → log)
static uint8_t computeVuMono() {
  // Gunakan magnitudo bins 1..N/2
  double sum2 = 0.0;
  int bins = 0;
  for (int k = 1; k < ANA_N/2; ++k) {
    double m = vReal[k];
    sum2 += m * m;
    ++bins;
  }
  if (bins == 0) return 0;
  double rms = sqrt(sum2 / (double)bins);
  double vu = log10(1.0 + rms) * 64.0;
  if (vu < 0.0) vu = 0.0;
  if (vu > 255.0) vu = 255.0;
  return (uint8_t)(vu + 0.5);
}

// ====== Public API ======
void sensorsInit() {
  // I2C backbone
  Wire.begin(I2C_SDA, I2C_SCL);

  // ADS1115 (gain ±4.096 V → cocok untuk divider 65V → ~3V di ADC)
  ads.begin(ADS_I2C_ADDR, &Wire);
  ads.setGain(GAIN_ONE); // ±4.096 V

  // DS18B20
  dallas.begin();

  // RTC DS3231
  rtcReady = rtc.begin(&Wire);
  if (rtcReady) {
    rtc.disable32K();
    rtc.writeSqwPinMode(DS3231_SquareWave1Hz);
    if (rtc.lostPower()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }
  pinMode(RTC_SQW_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RTC_SQW_PIN), onRtcSqw, RISING);

  // I2S Analyzer
  i2sReady = i2sSetup();
  sampCount = 0;
  lastFftMs = 0;
  bandsInit = false;
  memset(bandsOut, 0, sizeof(bandsOut));

  gVoltInstant = 0.0f;
  gHeatC = NAN;
  lastTempMs = 0;
  rtcTempC = NAN;
  rtcSqwTick = false;
}

void sensorsTick(uint32_t now) {
  // --- Baca voltmeter (boleh 10–20 Hz; di sini kita jalankan setiap tick) ---
  int16_t raw = ads.readADC_SingleEnded(ADS_CHANNEL);
  float   vAdc = ads.computeVolts(raw);   // Volt di pin ADS
  float   vReal = adcToRealVolt(vAdc);
  gVoltInstant = (vReal >= VOLT_MIN_VALID_V) ? vReal : 0.0f;

  // --- Heatsink temp (1 Hz cukup) ---
  if (now - lastTempMs >= 1000) {
    lastTempMs = now;
    dallas.requestTemperatures();
    float t = dallas.getTempCByIndex(0);
    // DallasTemperature kembalikan 85.0 / DEVICE_DISCONNECTED_C saat gagal
    if (t <= -127.0f || t >= 125.0f) {
      // invalid → pertahankan nilai lama (biarkan NAN jika belum pernah valid)
    } else {
      if (FEAT_FILTER_DS18B20_SOFT && !isnan(gHeatC)) {
        gHeatC = 0.7f * gHeatC + 0.3f * t;
      } else {
        gHeatC = t;
      }
    }

    if (rtcReady && FEAT_RTC_TEMP_TELEMETRY) {
      rtcTempC = rtc.getTemperature();
    } else {
      rtcTempC = NAN;
    }
  }

  // --- Analyzer (nonaktif saat standby dikelola di modul power/main) ---
  // Di sini selalu sample & process; jika ingin OFF saat standby, panggil
  // fungsi ini secara kondisional dari main (power state aware).
  analyzerSample();
  analyzerProcess(now);
}

// Voltmeter instant (tanpa smoothing)
float getVoltageInstant() {
  return gVoltInstant;
}

// Heatsink temp (Celsius)
float getHeatsinkC() {
  return gHeatC; // bisa NAN jika belum valid
}

float sensorsGetRtcTempC() {
  if (!FEAT_RTC_TEMP_TELEMETRY) {
    return NAN;
  }
  return rtcTempC;
}

bool sensorsGetTimeISO(char* out, size_t n) {
  if (!out || n == 0) return false;
  if (!rtcReady) {
    out[0] = '\0';
    return false;
  }
  DateTime now = rtc.now();
  snprintf(out, n, "%04u-%02u-%02uT%02u:%02u:%02uZ",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
  return true;
}

bool sensorsSqwConsumeTick() {
  if (rtcSqwTick) {
    rtcSqwTick = false;
    return true;
  }
  return false;
}

// Salin band analyzer (0..255)
void analyzerGetBytes(uint8_t outBands[], size_t nBands) {
  size_t n = (nBands < ANA_BANDS) ? nBands : ANA_BANDS;
  for (size_t i = 0; i < n; ++i) outBands[i] = bandsOut[i];
}

// VU mono 0..255
void analyzerGetVu(uint8_t &monoVu) {
  monoVu = computeVuMono();
}

// Enable/disable analyzer (hemat beban saat STANDBY)
void sensorsSetAnalyzerEnabled(bool en) {
  gAnalyzerEn = en;
#if I2S_USE_BUILTIN_ADC
  if (i2sReady) {
    if (en) {
      i2s_adc_enable(I2S_PORT);
    } else {
      i2s_adc_disable(I2S_PORT);
    }
  }
#endif
}

bool sensorsGetUnixTime(uint32_t& epochOut) {
  if (!rtcReady) return false;
  DateTime now = rtc.now();
  epochOut = now.unixtime();
  return true;
}

bool sensorsSetUnixTime(uint32_t epoch) {
  if (!rtcReady) return false;
  rtc.adjust(DateTime(epoch));
  return true;
}
