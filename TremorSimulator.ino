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

Servo servo;
WebServer server(80);

float freqHz = 5.0;
bool atA = true;
bool running = false;

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
  uint32_t remainingMs;
};

// ============= WEB PAGE - ÖN TARAFTA TANIMLANDI =============
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
      font-family: Arial, sans-serif;
      background: radial-gradient(circle at top, #12263f 0%, #071521 45%, #020b14 100%);
      color: #e2e8f0;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 18px;
    }

    .container {
      width: min(1100px, 100%);
      display: grid;
      grid-template-columns: 1.1fr 1.3fr;
      gap: 18px;
    }

    .card {
      background: rgba(15, 23, 42, 0.88);
      border: 1px solid #294968;
      border-radius: 26px;
      box-shadow: 0 18px 40px rgba(0, 0, 0, 0.38);
      padding: 22px;
      backdrop-filter: blur(8px);
    }

    .full {
      grid-column: 1 / -1;
    }

    .header {
      text-align: center;
      margin-bottom: 18px;
    }

    .icon {
      font-size: 42px;
      margin-bottom: 8px;
    }

    h1 {
      margin: 0;
      font-size: 28px;
      letter-spacing: 0.3px;
    }

    .subtitle {
      color: #9fb7ce;
      font-size: 14px;
      margin-top: 6px;
    }

    .frequency {
      text-align: center;
      font-size: 58px;
      font-weight: bold;
      color: #38bdf8;
      margin-top: 12px;
      margin-bottom: 4px;
    }

    .unit {
      text-align: center;
      color: #cbd5e1;
      margin-bottom: 18px;
      font-size: 14px;
    }

    input[type="range"] {
      width: 100%;
      accent-color: #38bdf8;
      cursor: pointer;
    }

    .range-labels {
      display: flex;
      justify-content: space-between;
      font-size: 12px;
      color: #a7bed4;
      margin-top: 8px;
    }

    .label {
      margin-top: 18px;
      margin-bottom: 8px;
      font-size: 12px;
      color: #b7c9d8;
    }

    input[type="number"] {
      width: 100%;
      padding: 12px 14px;
      background: rgba(30, 41, 59, 0.82);
      border: 1px solid #38577d;
      border-radius: 12px;
      color: #f8fafc;
      font-size: 16px;
      outline: none;
    }

    input[type="number"]:focus {
      border-color: #38bdf8;
      box-shadow: 0 0 0 2px rgba(56, 189, 248, 0.15);
    }

    .timer-box {
      margin-top: 12px;
      background: rgba(12, 28, 47, 0.8);
      border: 1px solid #405c7d;
      border-radius: 14px;
      padding: 12px;
      text-align: center;
    }

    .timer {
      font-size: 34px;
      font-weight: bold;
      color: #38bdf8;
      letter-spacing: 1px;
    }

    .timer.warning { color: #f59e0b; }
    .timer.critical { color: #ef4444; }

    .button-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
      margin-top: 18px;
    }

    button {
      border: none;
      border-radius: 14px;
      cursor: pointer;
      color: #fff;
      font-size: 16px;
      font-weight: bold;
      padding: 16px 12px;
      transition: transform 0.12s ease, opacity 0.15s ease;
      box-shadow: 0 8px 18px rgba(0,0,0,0.2);
    }

    button:active { transform: scale(0.98); }
    button:disabled { opacity: 0.55; cursor: not-allowed; }

    #toggle {
      background: linear-gradient(135deg, #ef4444, #dc2626);
      width: 100%;
    }

    #toggle.off {
      background: linear-gradient(135deg, #16a34a, #15803d);
    }

    #timerToggle {
      background: linear-gradient(135deg, #3b82f6, #2563eb);
    }

    #emergency {
      background: linear-gradient(135deg, #b91c1c, #991b1b);
      grid-column: 1 / -1;
    }

    .status {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
      margin-top: 20px;
      color: #e2e8f0;
      font-size: 14px;
    }

    .dot {
      width: 10px;
      height: 10px;
      border-radius: 50%;
      background: #22c55e;
      box-shadow: 0 0 12px rgba(34, 197, 94, 0.8);
    }

    .dot.off {
      background: #94a3b8;
      box-shadow: none;
    }

    .footer {
      text-align: center;
      color: #86a0ba;
      font-size: 12px;
      margin-top: 18px;
    }

    .chart-wrap {
      width: 100%;
      height: 360px;
      background: rgba(12, 28, 47, 0.8);
      border: 1px solid #294968;
      border-radius: 18px;
      padding: 10px;
    }

    canvas {
      width: 100% !important;
      height: 100% !important;
      display: block;
      border-radius: 12px;
      background: rgba(15, 23, 42, 0.6);
    }

    @media (max-width: 760px) {
      .container {
        grid-template-columns: 1fr;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="card">
      <div class="header">
        <div class="icon">〰️</div>
        <h1>Tremor Simülatörü</h1>
        <div class="subtitle">Servo motor kontrol paneli</div>
      </div>

      <div class="frequency" id="f">5.0</div>
      <div class="unit">Hz · Saniyedeki hareket döngüsü</div>

      <input id="s" type="range" min="10" max="80" value="50" />
      <div class="range-labels">
        <span>1.0 Hz</span>
        <span>8.0 Hz</span>
      </div>

      <div class="label">Süre (saniye)</div>
      <input id="duration" type="number" min="10" max="3600" value="60" />

      <div class="timer-box">
        <div class="timer" id="timer">--:--</div>
      </div>

      <div class="status">
        <span id="dot" class="dot off"></span>
        <span id="status">Sistem durduruldu</span>
      </div>

      <div class="button-grid">
        <button id="toggle" class="off" onclick="toggleMotor()">▶ Başlat</button>
        <button id="timerToggle" onclick="toggleTimer()">⏱️ Süre Başlat</button>
        <button id="emergency" onclick="emergencyStop()">🛑 ACİL DURDUR</button>
      </div>

      <div class="footer">ESP32 bağlantısı · TremorSim</div>
    </div>

    <div class="card full">
      <h2 style="margin:0 0 12px 0; color:#38bdf8;">📊 Canlı Grafik</h2>
      <div class="chart-wrap">
        <canvas id="graph"></canvas>
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

    let motorRunning = false;
    let timerRunning = false;
    let timerRemainingMs = 60000;

    const canvas = document.getElementById('graph');
    const ctx = canvas.getContext('2d');

    const angleData = [];
    const freqData = [];

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

    function alertStart() {
      vibrate([80, 40, 80]);
    }

    function alertStop() {
      vibrate([120, 60, 120]);
    }

    function alertEmergency() {
      vibrate([220, 80, 220, 80, 220]);
    }

    function drawChart() {
      const rect = canvas.getBoundingClientRect();
      const ratio = window.devicePixelRatio || 1;

      canvas.width = Math.floor(rect.width * ratio);
      canvas.height = Math.floor(rect.height * ratio);

      const w = rect.width;
      const h = rect.height;

      ctx.setTransform(1, 0, 0, 1, 0, 0);
      ctx.scale(ratio, ratio);

      ctx.clearRect(0, 0, w, h);
      ctx.fillStyle = 'rgba(15, 23, 42, 0.82)';
      ctx.fillRect(0, 0, w, h);

      ctx.strokeStyle = 'rgba(148,163,184,0.22)';
      ctx.lineWidth = 1;

      for (let i = 0; i <= 5; i++) {
        const y = (h / 5) * i;
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(w, y);
        ctx.stroke();
      }

      if (angleData.length < 2) return;

      const maxAngle = 120;
      const maxFreq = 8;

      ctx.beginPath();
      angleData.forEach((v, i) => {
        const x = (i / (angleData.length - 1)) * w;
        const y = h - ((v / maxAngle) * (h - 26)) - 10;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });
      ctx.strokeStyle = '#38bdf8';
      ctx.lineWidth = 2.2;
      ctx.stroke();

      ctx.beginPath();
      freqData.forEach((v, i) => {
        const x = (i / (freqData.length - 1)) * w;
        const y = h - ((v / maxFreq) * (h - 26)) - 10;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });
      ctx.strokeStyle = '#a78bfa';
      ctx.lineWidth = 2.0;
      ctx.stroke();
    }

    function pushChart(angle, freq) {
      angleData.push(angle);
      freqData.push(freq);

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

      const ok = await fetchOk('/run?state=' + (nextState ? '1' : '0') + '&duration=' + value);

      if (!ok) {
        setStatus('Bağlantı hatası', false);
        btn.disabled = false;
        return;
      }

      motorRunning = nextState;

      if (motorRunning) {
        btn.textContent = '⏸ Durdur';
        btn.classList.remove('off');
        setStatus('Sistem çalışıyor', true);
        alertStart();
      } else {
        btn.textContent = '▶ Başlat';
        btn.classList.add('off');
        setStatus('Sistem durduruldu', false);
        alertStop();
      }

      btn.disabled = false;
    }

    async function toggleTimer() {
      timerBtn.disabled = true;

      const nextState = !timerRunning;
      const ok = await fetchOk('/timer?state=' + (nextState ? '1' : '0'));

      if (!ok) {
        setStatus('Zamanlayıcı hatası', false);
        timerBtn.disabled = false;
        return;
      }

      timerRunning = nextState;

      if (timerRunning) {
        timerBtn.textContent = '⏸ Süre Durdur';
        setStatus(motorRunning ? 'Süre çalışıyor' : 'Süre başlatıldı', motorRunning || true);
      } else {
        timerBtn.textContent = '⏱️ Süre Başlat';
        setStatus(motorRunning ? 'Sistem çalışıyor' : 'Süre durduruldu', motorRunning);
      }

      timerBtn.disabled = false;
    }

    async function emergencyStop() {
      const ok = await fetchOk('/emergency');
      if (!ok) {
        setStatus('Bağlantı hatası', false);
        return;
      }

      motorRunning = false;
      timerRunning = false;

      btn.textContent = '▶ Başlat';
      btn.classList.add('off');
      timerBtn.textContent = '⏱️ Süre Başlat';
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
      await fetch('/set?f=' + value + '&d=' + d);
    });

    durationInput.addEventListener('change', async () => {
      const d = Number(durationInput.value) || 60;
      await fetch('/set?f=' + (s.value / 10).toFixed(1) + '&d=' + d);
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
          setStatus(motorRunning ? 'Sistem çalışıyor' : 'Sistem durduruldu', motorRunning);
        }

        if (data.timerRunning !== undefined) {
          timerRunning = !!data.timerRunning;
          timerBtn.textContent = timerRunning ? '⏸ Süre Durdur' : '⏱️ Süre Başlat';
        }

        if (data.remainingMs !== undefined) {
          timerRemainingMs = Number(data.remainingMs);
          updateTimerUi();
        }

        if (data.angle !== undefined && data.frequency !== undefined) {
          pushChart(Number(data.angle), Number(data.frequency));
        }
      } catch (error) {
        // sessiz mod
      }

      setTimeout(pollData, 300);
    }

    window.addEventListener('resize', drawChart);
    updateTimerUi();
    drawChart();
    pollData();
  </script>
</body>
</html>
)rawliteral";
// ============= WEB PAGE SONU =============

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
    return;
  }

  freqHz = (float)s.freq10 / 10.0f;
  durationSec = s.durationSec;
  running = s.running == 1;
  timerRunning = s.timerRunning == 1;
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
  String json = "{";
  json += "\"angle\":" + String(atA ? ANGLE_A : ANGLE_B) + ",";
  json += "\"frequency\":" + String(freqHz, 1) + ",";
  json += "\"running\":" + String(running ? "true" : "false") + ",";
  json += "\"timerRunning\":" + String(timerRunning ? "true" : "false") + ",";
  json += "\"remainingMs\":" + String(timerRemainingMs) + ",";
  json += "\"durationSec\":" + String(durationSec);
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
  servo.write(ANGLE_A);

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
    servo.write(atA ? ANGLE_A : ANGLE_B);
  }
}
