#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <EEPROM.h>

#define USE_BUZZER 0
const int LED_PIN = 2;

const char* AP_SSID = "TremorSim";
const char* AP_PASS = "12345678";

const int SERVO_PIN = 38;
const int ANGLE_A = 75;
const int ANGLE_B = 105;

const int SEVERITY_LOW = 0;
const int SEVERITY_MID = 1;
const int SEVERITY_HIGH = 2;

Servo servo;
WebServer server(80);

float freqHz = 5.0;
bool atA = true;
bool running = false;
int currentSeverity = SEVERITY_MID;

bool timerRunning = false;
uint32_t timerRemainingMs = 60000;
uint32_t timerStartedAtMs = 0;
uint16_t durationSec = 60;

unsigned long lastToggle = 0;

const uint16_t EEPROM_MAGIC = 0x5453;

struct Settings {
  uint16_t magic;
  uint16_t freq10;
  uint16_t durationSec;
  uint8_t running;
  uint8_t timerRunning;
  uint8_t severity;
  uint32_t remainingMs;
};

int getSeverityMinAngle() {
  switch (currentSeverity) {
    case SEVERITY_LOW: return 68;
    case SEVERITY_HIGH: return 60;
    default: return 75;
  }
}

int getSeverityMaxAngle() {
  switch (currentSeverity) {
    case SEVERITY_LOW: return 92;
    case SEVERITY_HIGH: return 120;
    default: return 105;
  }
}

int getCurrentServoAngle() {
  return atA ? getSeverityMinAngle() : getSeverityMaxAngle();
}

void writeCurrentMotorPosition() {
  servo.write(getCurrentServoAngle());
}

const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="tr">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Tremor Simulator</title>
  <style>
    * { box-sizing: border-box; }
    html, body {
      margin: 0;
      min-height: 100vh;
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #0a1628 0%, #1a2a4e 50%, #0d1f3c 100%);
      color: #e2e8f0;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 18px;
    }

    .container {
      width: min(1200px, 100%);
      display: grid;
      grid-template-columns: 1.15fr 1.4fr;
      gap: 22px;
    }

    .card {
      background: linear-gradient(135deg, rgba(15, 23, 42, 0.92) 0%, rgba(20, 35, 55, 0.88) 100%);
      border: 1px solid rgba(56, 189, 248, 0.18);
      border-radius: 28px;
      box-shadow: 0 20px 60px rgba(0, 0, 0, 0.45), inset 0 1px 0 rgba(255, 255, 255, 0.1);
      padding: 28px;
      backdrop-filter: blur(12px);
    }

    .full { grid-column: 1 / -1; }

    .header { text-align: center; margin-bottom: 22px; }
    .icon { font-size: 48px; margin-bottom: 12px; animation: pulse 2s ease-in-out infinite; }
    
    @keyframes pulse {
      0%, 100% { opacity: 1; transform: scale(1); }
      50% { opacity: 0.7; transform: scale(1.05); }
    }

    h1 { margin: 0; font-size: 32px; letter-spacing: -0.5px; background: linear-gradient(135deg, #38bdf8 0%, #60a5fa 100%); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
    .subtitle { color: #a9bcd4; font-size: 14px; margin-top: 8px; font-weight: 300; }

    .frequency {
      text-align: center;
      font-size: 64px;
      font-weight: 700;
      background: linear-gradient(135deg, #38bdf8, #0ea5e9);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      margin-top: 14px;
      margin-bottom: 6px;
      letter-spacing: -1px;
    }

    .unit {
      text-align: center;
      color: #cbd5e1;
      margin-bottom: 22px;
      font-size: 13px;
      font-weight: 500;
    }

    input[type="range"] {
      width: 100%;
      accent-color: #38bdf8;
      cursor: pointer;
      height: 6px;
    }

    .range-labels {
      display: flex;
      justify-content: space-between;
      font-size: 11px;
      color: #a7bed4;
      margin-top: 10px;
      font-weight: 600;
    }

    .label {
      margin-top: 20px;
      margin-bottom: 10px;
      font-size: 12px;
      color: #b7c9d8;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }

    .row {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
    }

    input[type="number"], select {
      width: 100%;
      padding: 14px 16px;
      background: linear-gradient(135deg, rgba(30, 41, 59, 0.85) 0%, rgba(25, 35, 55, 0.9) 100%);
      border: 1px solid rgba(56, 189, 248, 0.25);
      border-radius: 14px;
      color: #f8fafc;
      font-size: 15px;
      outline: none;
      transition: all 0.3s ease;
      font-weight: 500;
    }

    input[type="number"]:focus, select:focus {
      border-color: #38bdf8;
      box-shadow: 0 0 0 3px rgba(56, 189, 248, 0.15);
      background: linear-gradient(135deg, rgba(56, 189, 248, 0.08) 0%, rgba(96, 165, 250, 0.05) 100%);
    }

    .timer-box {
      margin-top: 16px;
      background: linear-gradient(135deg, rgba(12, 28, 47, 0.9) 0%, rgba(15, 35, 60, 0.85) 100%);
      border: 1px solid rgba(56, 189, 248, 0.2);
      border-radius: 18px;
      padding: 16px;
      text-align: center;
      box-shadow: inset 0 2px 8px rgba(0, 0, 0, 0.3);
    }

    .timer { font-size: 42px; font-weight: 700; color: #38bdf8; letter-spacing: -0.5px; }
    .timer.warning { color: #fbbf24; }
    .timer.critical { color: #ff6b6b; }

    .button-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 14px;
      margin-top: 22px;
    }

    button {
      border: none;
      border-radius: 16px;
      cursor: pointer;
      color: #fff;
      font-size: 17px;
      font-weight: 700;
      padding: 18px 16px;
      transition: all 0.3s cubic-bezier(0.34, 1.56, 0.64, 1);
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.3);
      text-transform: uppercase;
      letter-spacing: 0.3px;
      position: relative;
      overflow: hidden;
    }

    button:before {
      content: '';
      position: absolute;
      top: 0;
      left: -100%;
      width: 100%;
      height: 100%;
      background: rgba(255, 255, 255, 0.2);
      transition: left 0.3s ease;
    }

    button:hover:before { left: 100%; }
    button:active { transform: scale(0.96); }
    button:disabled { opacity: 0.5; cursor: not-allowed; }

    #toggle {
      background: linear-gradient(135deg, #ef4444 0%, #dc2626 100%);
      width: 100%;
      font-size: 18px;
      padding: 20px;
    }

    #toggle.off {
      background: linear-gradient(135deg, #22c55e 0%, #16a34a 100%);
    }

    #timerToggle {
      background: linear-gradient(135deg, #3b82f6 0%, #2563eb 100%);
      font-size: 16px;
      padding: 18px;
    }

    #emergency {
      background: linear-gradient(135deg, #dc2626 0%, #991b1b 100%);
      grid-column: 1 / -1;
      font-size: 18px;
      padding: 20px;
    }

    .status {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 10px;
      margin-top: 22px;
      color: #e2e8f0;
      font-size: 15px;
      font-weight: 600;
    }

    .dot {
      width: 12px;
      height: 12px;
      border-radius: 50%;
      background: #22c55e;
      box-shadow: 0 0 16px rgba(34, 197, 94, 0.9);
      animation: blink 2s ease-in-out infinite;
    }

    @keyframes blink {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.5; }
    }

    .dot.off {
      background: #64748b;
      box-shadow: none;
      animation: none;
    }

    .footer {
      text-align: center;
      color: #86a0ba;
      font-size: 12px;
      margin-top: 22px;
      font-weight: 500;
    }

    .chart-container {
      display: flex;
      flex-direction: column;
      height: 100%;
    }

    .chart-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 14px;
      padding-bottom: 12px;
      border-bottom: 1px solid rgba(56, 189, 248, 0.15);
    }

    .chart-header h2 {
      margin: 0;
      font-size: 20px;
      background: linear-gradient(135deg, #38bdf8 0%, #60a5fa 100%);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      font-weight: 700;
    }

    .chart-legend {
      display: flex;
      gap: 16px;
      font-size: 12px;
      color: #a9bfd5;
    }

    .legend-item {
      display: flex;
      align-items: center;
      gap: 6px;
    }

    .legend-dot { width: 8px; height: 8px; border-radius: 2px; }

    .chart-wrap {
      flex: 1;
      background: linear-gradient(135deg, rgba(12, 28, 47, 0.95) 0%, rgba(10, 25, 40, 0.92) 100%);
      border: 1px solid rgba(56, 189, 248, 0.15);
      border-radius: 18px;
      padding: 14px;
      box-shadow: inset 0 2px 12px rgba(0, 0, 0, 0.4);
    }

    canvas {
      width: 100% !important;
      height: 100% !important;
      display: block;
      border-radius: 14px;
    }

    @media (max-width: 900px) {
      .container { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="card">
      <div class="header">
        <div class="icon">〰️</div>
        <h1>Tremor Sim</h1>
        <div class="subtitle">Servo Motor Kontrol Sistemi</div>
      </div>

      <div class="frequency" id="f">5.0</div>
      <div class="unit">Hz</div>

      <input id="s" type="range" min="10" max="80" value="50" />
      <div class="range-labels">
        <span>1.0 Hz</span>
        <span>8.0 Hz</span>
      </div>

      <div class="label">Parametreler</div>
      <div class="row">
        <div>
          <div class="label" style="margin-top: 0;">Şiddet</div>
          <select id="severity">
            <option value="0">🟢 Hafif</option>
            <option value="1" selected>🟡 Orta</option>
            <option value="2">🔴 Şiddetli</option>
          </select>
        </div>
        <div>
          <div class="label" style="margin-top: 0;">Süre (sn)</div>
          <input id="duration" type="number" min="10" max="3600" value="60" />
        </div>
      </div>

      <div class="timer-box">
        <div class="timer" id="timer">--:--</div>
      </div>

      <div class="status">
        <span id="dot" class="dot off"></span>
        <span id="status">Sistem Durduruldu</span>
      </div>

      <div class="button-grid">
        <button id="toggle" class="off" onclick="toggleMotor()">▶ Başlat</button>
        <button id="timerToggle" onclick="toggleTimer()">⏱️ Süre</button>
        <button id="emergency" onclick="emergencyStop()">🛑 ACİL DURDUR</button>
      </div>

      <div class="footer">ESP32 · TremorSim v1.0</div>
    </div>

    <div class="card full">
      <div class="chart-container">
        <div class="chart-header">
          <h2>📊 Canlı Hareket Analizi</h2>
          <div class="chart-legend">
            <div class="legend-item">
              <div class="legend-dot" style="background: #38bdf8;"></div>
              <span>Açı</span>
            </div>
            <div class="legend-item">
              <div class="legend-dot" style="background: #a78bfa;"></div>
              <span>Frekans</span>
            </div>
          </div>
        </div>
        <div class="chart-wrap">
          <canvas id="graph"></canvas>
        </div>
      </div>
    </div>
  </div>

  <script>
    const s = document.getElementById('s');
    const f = document.getElementById('f');
    const btn = document.getElementById('toggle');
    const timerBtn = document.getElementById('timerToggle');
    const statusText = document.getElementById('status');
    const dot = document.getElementById('dot');
    const durationInput = document.getElementById('duration');
    const timerDisplay = document.getElementById('timer');
    const severitySelect = document.getElementById('severity');

    let motorRunning = false;
    let timerRunning = false;
    let timerRemainingMs = 60000;
    let currentSeverity = 1;

    const canvas = document.getElementById('graph');
    const ctx = canvas.getContext('2d');
    const angleData = [];
    const freqData = [];

    function getSeverityConfig(level) {
      switch (Number(level)) {
        case 0:
          return { label: 'Hafif', multiplier: 0.75, line: '#34d399', glow: 'rgba(52, 211, 153, 0.25)' };
        case 2:
          return { label: 'Şiddetli', multiplier: 1.35, line: '#f87171', glow: 'rgba(248, 113, 113, 0.25)' };
        default:
          return { label: 'Orta', multiplier: 1.0, line: '#38bdf8', glow: 'rgba(56, 189, 248, 0.25)' };
      }
    }

    function applySeverity(level) {
      currentSeverity = Number(level);
      const cfg = getSeverityConfig(currentSeverity);
      document.documentElement.style.setProperty('--graph-line', cfg.line);
      fetch('/set?level=' + currentSeverity);
    }

    function formatTime(ms) {
      const totalSeconds = Math.max(0, Math.ceil(ms / 1000));
      const minutes = Math.floor(totalSeconds / 60);
      const seconds = totalSeconds % 60;
      return `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
    }

    function updateTimerUi() {
      timerDisplay.textContent = formatTime(timerRemainingMs);

      if (timerRemainingMs <= 10000) {
        timerDisplay.classList.add('critical');
        timerDisplay.classList.remove('warning');
      } else if (timerRemainingMs <= 30000) {
        timerDisplay.classList.add('warning');
        timerDisplay.classList.remove('critical');
      } else {
        timerDisplay.classList.remove('warning', 'critical');
      }
    }

    function setStatus(text, on) {
      statusText.textContent = text;
      dot.classList.toggle('off', !on);
    }

    function vibrate(pattern) {
      if ("vibrate" in navigator) {
        navigator.vibrate(pattern);
      }
    }

    function alertStart() { vibrate([80, 40, 80]); }
    function alertStop() { vibrate([120, 60, 120]); }
    function alertEmergency() { vibrate([220, 80, 220, 80, 220]); }

    function drawChart() {
      const rect = canvas.getBoundingClientRect();
      const ratio = window.devicePixelRatio || 1;
      canvas.width = Math.floor(rect.width * ratio);
      canvas.height = Math.floor(rect.height * ratio);

      const w = rect.width;
      const h = rect.height;
      const cfg = getSeverityConfig(currentSeverity);
      const maxVisual = 120 * cfg.multiplier;

      ctx.setTransform(1, 0, 0, 1, 0, 0);
      ctx.scale(ratio, ratio);
      ctx.clearRect(0, 0, w, h);
      ctx.fillStyle = 'rgba(15, 23, 42, 0.5)';
      ctx.fillRect(0, 0, w, h);

      ctx.strokeStyle = 'rgba(148, 163, 184, 0.15)';
      ctx.lineWidth = 1;
      for (let i = 0; i <= 5; i++) {
        const y = (h / 5) * i;
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(w, y);
        ctx.stroke();
      }

      if (angleData.length < 2) return;

      ctx.beginPath();
      angleData.forEach((v, i) => {
        const x = (i / (angleData.length - 1)) * w;
        const y = h - ((v / maxVisual) * (h - 26)) - 10;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });
      ctx.strokeStyle = cfg.line;
      ctx.lineWidth = 3;
      ctx.shadowColor = cfg.glow;
      ctx.shadowBlur = 16;
      ctx.stroke();
      ctx.shadowBlur = 0;

      ctx.beginPath();
      freqData.forEach((v, i) => {
        const x = (i / (freqData.length - 1)) * w;
        const y = h - ((v / 8) * (h - 26)) - 10;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });
      ctx.strokeStyle = '#a78bfa';
      ctx.lineWidth = 2.2;
      ctx.stroke();
    }

    function pushChart(angle, freq) {
      const cfg = getSeverityConfig(currentSeverity);
      const adjusted = Number(angle) * cfg.multiplier;
      angleData.push(adjusted);
      freqData.push(Number(freq));

      if (angleData.length > 60) {
        angleData.shift();
        freqData.shift();
      }
      drawChart();
    }

    async function fetchOk(url) {
      try {
        const response = await fetch(url);
        return response.ok;
      } catch (error) {
        return false;
      }
    }

    async function toggleMotor() {
      btn.disabled = true;
      const nextState = !motorRunning;
      const value = Number(durationInput.value) || 60;

      const ok = await fetchOk('/run?state=' + (nextState ? '1' : '0') + '&duration=' + value + '&level=' + currentSeverity);
      if (!ok) {
        setStatus('Bağlantı Hatası', false);
        btn.disabled = false;
        return;
      }

      motorRunning = nextState;

      if (motorRunning) {
        btn.textContent = '⏸ Durdur';
        btn.classList.remove('off');
        setStatus('Sistem Çalışıyor', true);
        alertStart();
      } else {
        btn.textContent = '▶ Başlat';
        btn.classList.add('off');
        setStatus('Sistem Durduruldu', false);
        alertStop();
      }

      btn.disabled = false;
    }

    async function toggleTimer() {
      timerBtn.disabled = true;
      const nextState = !timerRunning;
      const ok = await fetchOk('/timer?state=' + (nextState ? '1' : '0'));

      if (!ok) {
        setStatus('Zamanlayıcı Hatası', false);
        timerBtn.disabled = false;
        return;
      }

      timerRunning = nextState;

      if (timerRunning) {
        timerBtn.textContent = '⏸ Durdur';
        setStatus(motorRunning ? 'Süre Çalışıyor' : 'Süre Başlatıldı', true);
      } else {
        timerBtn.textContent = '⏱️ Başlat';
        setStatus(motorRunning ? 'Sistem Çalışıyor' : 'Süre Durduruldu', motorRunning);
      }

      timerBtn.disabled = false;
    }

    async function emergencyStop() {
      const ok = await fetchOk('/emergency');
      if (!ok) {
        setStatus('Bağlantı Hatası', false);
        return;
      }

      motorRunning = false;
      timerRunning = false;
      btn.textContent = '▶ Başlat';
      btn.classList.add('off');
      timerBtn.textContent = '⏱️ Başlat';
      setStatus('⚠️ ACİL DURDURMA', false);
      timerDisplay.textContent = '--:--';
      alertEmergency();
    }

    s.addEventListener('input', () => {
      const value = (s.value / 10).toFixed(1);
      f.textContent = value;
    });

    s.addEventListener('change', async () => {
      const value = (s.value / 10).toFixed(1);
      const d = Number(durationInput.value) || 60;
      await fetch('/set?f=' + value + '&d=' + d + '&level=' + currentSeverity);
    });

    durationInput.addEventListener('change', async () => {
      const d = Number(durationInput.value) || 60;
      await fetch('/set?f=' + (s.value / 10).toFixed(1) + '&d=' + d + '&level=' + currentSeverity);
    });

    severitySelect.addEventListener('change', () => {
      applySeverity(severitySelect.value);
    });

    async function pollData() {
      try {
        const response = await fetch('/data');
        if (!response.ok) return;

        const data = await response.json();

        if (data.running !== undefined) {
          motorRunning = !!data.running;
          btn.textContent = motorRunning ? '⏸ Durdur' : '▶ Başlat';
          btn.classList.toggle('off', !motorRunning);
          setStatus(motorRunning ? 'Sistem Çalışıyor' : 'Sistem Durduruldu', motorRunning);
        }

        if (data.timerRunning !== undefined) {
          timerRunning = !!data.timerRunning;
          timerBtn.textContent = timerRunning ? '⏸ Durdur' : '⏱️ Başlat';
        }

        if (data.remainingMs !== undefined) {
          timerRemainingMs = Number(data.remainingMs);
          updateTimerUi();
        }

        if (data.severity !== undefined) {
          currentSeverity = Number(data.severity);
          severitySelect.value = String(currentSeverity);
        }

        if (data.angle !== undefined && data.frequency !== undefined) {
          pushChart(Number(data.angle), Number(data.frequency));
        }
      } catch (error) {
      }

      setTimeout(pollData, 1000);
    }

    window.addEventListener('resize', drawChart);
    severitySelect.value = String(currentSeverity);
    updateTimerUi();
    drawChart();
    pollData();
  </script>
</body>
</html>
)rawliteral";

void blinkLed(int count, int delayMs) {
  for (int i = 0; i < count; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(delayMs);
    digitalWrite(LED_PIN, LOW);
    delay(delayMs);
  }
}

void playStartAlert() {
  if (USE_BUZZER) {
    // tone(BUZZER_PIN, 1800, 120);
  } else {
    blinkLed(2, 120);
  }
}

void playStopAlert() {
  if (USE_BUZZER) {
    // tone(BUZZER_PIN, 700, 180);
  } else {
    blinkLed(3, 100);
  }
}

void playEmergencyAlert() {
  if (USE_BUZZER) {
    // tone(BUZZER_PIN, 300, 200);
  } else {
    for (int i = 0; i < 5; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(80);
      digitalWrite(LED_PIN, LOW);
      delay(80);
    }
  }
}

void saveSettings() {
  Settings s;
  s.magic = EEPROM_MAGIC;
  s.freq10 = (uint16_t)(freqHz * 10.0f + 0.5f);
  s.durationSec = durationSec;
  s.running = running ? 1 : 0;
  s.timerRunning = timerRunning ? 1 : 0;
  s.severity = (uint8_t)currentSeverity;
  s.remainingMs = timerRemainingMs;

  EEPROM.put(0, s);
  EEPROM.commit();
}

void loadSettings() {
  Settings s;
  EEPROM.get(0, s);

  if (s.magic != EEPROM_MAGIC) {
    freqHz = 5.0f;
    durationSec = 60;
    timerRemainingMs = 60000;
    running = false;
    timerRunning = false;
    currentSeverity = SEVERITY_MID;
    return;
  }

  freqHz = (float)s.freq10 / 10.0f;
  durationSec = s.durationSec;
  running = s.running == 1;
  timerRunning = s.timerRunning == 1;
  currentSeverity = (int)s.severity;
  if (currentSeverity < SEVERITY_LOW || currentSeverity > SEVERITY_HIGH) {
    currentSeverity = SEVERITY_MID;
  }
  timerRemainingMs = s.remainingMs;

  if (timerRemainingMs == 0) {
    timerRemainingMs = (uint32_t)durationSec * 1000UL;
  }

  if (timerRunning) {
    timerStartedAtMs = millis();
  } else {
    timerStartedAtMs = 0;
  }
}

void handleRoot() {
  server.send_P(200, "text/html", PAGE);
}

void handleSet() {
  if (server.hasArg("f")) {
    float f = server.arg("f").toFloat();
    if (f >= 1.0f && f <= 8.0f) {
      freqHz = f;
      Serial.print("Frekans: ");
      Serial.print(freqHz, 1);
      Serial.println(" Hz");
    }
  }

  if (server.hasArg("d")) {
    int d = server.arg("d").toInt();
    if (d > 0 && d <= 3600) {
      durationSec = (uint16_t)d;
      if (!timerRunning) {
        timerRemainingMs = (uint32_t)durationSec * 1000UL;
      }
    }
  }

  if (server.hasArg("level")) {
    int lvl = server.arg("level").toInt();
    if (lvl >= SEVERITY_LOW && lvl <= SEVERITY_HIGH) {
      currentSeverity = lvl;
    }
  }

  saveSettings();
  server.send(200, "text/plain", "ok");
}

void handleRun() {
  if (server.hasArg("state")) {
    String state = server.arg("state");

    if (state == "1") {
      running = true;
      lastToggle = millis();
      playStartAlert();
      Serial.println("Servo baslatildi");
    } else if (state == "0") {
      running = false;
      playStopAlert();
      Serial.println("Servo durduruldu");
    }
  }

  if (server.hasArg("duration")) {
    int d = server.arg("duration").toInt();
    if (d > 0 && d <= 3600) {
      durationSec = (uint16_t)d;
      if (!timerRunning) {
        timerRemainingMs = (uint32_t)durationSec * 1000UL;
      }
    }
  }

  if (server.hasArg("level")) {
    int lvl = server.arg("level").toInt();
    if (lvl >= SEVERITY_LOW && lvl <= SEVERITY_HIGH) {
      currentSeverity = lvl;
    }
  }

  saveSettings();
  server.send(200, "text/plain", "ok");
}

void handleTimer() {
  if (server.hasArg("state")) {
    String state = server.arg("state");

    if (state == "1") {
      if (!timerRunning) {
        timerRunning = true;
        timerStartedAtMs = millis();
        timerRemainingMs = (uint32_t)durationSec * 1000UL;
        Serial.println("Timer baslatildi");
      }
    } else if (state == "0") {
      if (timerRunning) {
        uint32_t elapsed = millis() - timerStartedAtMs;
        if (elapsed > timerRemainingMs) elapsed = timerRemainingMs;
        timerRemainingMs -= elapsed;
        timerRunning = false;
        timerStartedAtMs = 0;
        Serial.println("Timer durduruldu");
      }
    }
  }

  saveSettings();
  server.send(200, "text/plain", "ok");
}

void handleEmergency() {
  running = false;
  timerRunning = false;
  timerRemainingMs = (uint32_t)durationSec * 1000UL;
  timerStartedAtMs = 0;

  servo.write(ANGLE_A);
  playEmergencyAlert();

  Serial.println("ACIL DURDURMA ETKIN!");
  saveSettings();
  server.send(200, "text/plain", "ok");
}

void handleData() {
  uint32_t remainingForClient = timerRemainingMs;
  if (timerRunning) {
    uint32_t elapsed = millis() - timerStartedAtMs;
    if (elapsed >= remainingForClient) {
      remainingForClient = 0;
    } else {
      remainingForClient -= elapsed;
    }
  }

  String json = "{";
  json += "\"angle\":" + String(getCurrentServoAngle()) + ",";
  json += "\"frequency\":" + String(freqHz, 1) + ",";
  json += "\"running\":" + String(running ? "true" : "false") + ",";
  json += "\"timerRunning\":" + String(timerRunning ? "true" : "false") + ",";
  json += "\"remainingMs\":" + String(remainingForClient) + ",";
  json += "\"durationSec\":" + String(durationSec) + ",";
  json += "\"severity\":" + String(currentSeverity);
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  EEPROM.begin(64);
  loadSettings();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  writeCurrentMotorPosition();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.print("Wi-Fi: ");
  Serial.println(AP_SSID);
  Serial.print("Adres: http://");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/run", handleRun);
  server.on("/timer", handleTimer);
  server.on("/emergency", handleEmergency);
  server.on("/data", handleData);

  server.begin();
  lastToggle = millis();
}

void loop() {
  server.handleClient();

  if (timerRunning) {
    uint32_t elapsed = millis() - timerStartedAtMs;
    if (elapsed >= timerRemainingMs) {
      timerRemainingMs = 0;
      timerRunning = false;
      timerStartedAtMs = 0;
      running = false;
      playEmergencyAlert();
      Serial.println("SURE SONA ERDI - Sistem durduruldu");
      saveSettings();
      return;
    }
  }

  if (!running) return;

  unsigned long halfPeriod = (unsigned long)(500.0f / freqHz);
  unsigned long now = millis();

  if (now - lastToggle >= halfPeriod) {
    lastToggle = now;
    atA = !atA;
    writeCurrentMotorPosition();
  }
}
