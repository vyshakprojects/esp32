#include <WiFi.h>
#include <WebServer.h>      // Standard WebServer library
#include <ElegantOTA.h>     // ElegantOTA library
#include <ArduinoJson.h>    // For handling JSON data for status updates
#include <esp_system.h>     // For ESP.getFreeHeap(), ESP.getChipId(), etc.
#include <esp32-hal-cpu.h>  // For temperatureRead()

// --- Configuration ---
const char* ssid = "wifiname";          // REPLACE with your WiFi SSID
const char* password = "password"; // REPLACE with your WiFi password

// OTA Authentication (Highly Recommended!)
const char* ota_username = "admin";
const char* ota_password = "password"; // CHANGE THIS TO A SECURE PASSWORD!

// --- Global Variables ---
const int ledPin = 2; // Most ESP32 boards have an onboard LED on GPIO 2
bool ledState = LOW;  // Initial LED state

// Create WebServer object on port 80
WebServer server(80);

// --- System Metrics Variables for CPU Utilization ---
// These need to be volatile or accessed in a thread-safe manner if using multiple tasks,
// but for a simple loop(), non-volatile is generally okay.
unsigned long prevLoopMicros = 0; // Time at the start of the loop
unsigned long idleMicros = 0;     // Total microseconds spent in idle hook
float cpuUtilization = 0.0;       // Approximation of CPU utilization (0-100%)

// --- HTML Templates ---
// Base HTML structure to be used by all pages, placeholders for content and title
const char base_html_template[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <title>%TITLE%</title>
  <style>
    :root {
      --primary-bg: #f4f7f6;
      --secondary-bg: #ffffff;
      --text-color: #333333;
      --accent-color: #007bff;
      --accent-hover: #0056b3;
      --border-color: #e0e0e0;
      --shadow-color: rgba(0,0,0,0.1);
      --button-on-bg: #28a745;
      --button-on-hover: #218838;
      --button-off-bg: #dc3545;
      --button-off-hover: #c82333;
      --chart-border-color: #ccc;
      --chart-grid-color: #eee;
    }
    html.dark-mode {
      --primary-bg: #282c34;
      --secondary-bg: #3c4048;
      --text-color: #e0e0e0;
      --accent-color: #61afef;
      --accent-hover: #50a3dd;
      --border-color: #4a4e54;
      --shadow-color: rgba(0,0,0,0.3);
      --button-on-bg: #98c379;
      --button-on-hover: #8ac06b;
      --button-off-bg: #e06c75;
      --button-off-hover: #d25a62;
      --chart-border-color: #555;
      --chart-grid-color: #444;
    }
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      margin: 0;
      padding: 0;
      background-color: var(--primary-bg);
      color: var(--text-color);
      transition: background-color 0.3s, color 0.3s;
      line-height: 1.6;
    }
    .header {
      background-color: var(--secondary-bg);
      padding: 15px 30px;
      box-shadow: 0 2px 5px var(--shadow-color);
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 20px;
    }
    .header h2 { margin: 0; font-size: 1.8rem; color: var(--accent-color); }
    .nav { margin-right: 20px; }
    .nav a {
      color: var(--text-color);
      text-decoration: none;
      padding: 10px 15px;
      margin: 0 5px;
      border-radius: 5px;
      transition: background-color 0.3s, color 0.3s;
    }
    .nav a:hover {
      background-color: var(--primary-bg);
      color: var(--accent-color);
    }
    .content {
      max-width: 960px; /* Wider content for graphs */
      margin: 20px auto;
      padding: 30px;
      background-color: var(--secondary-bg);
      border-radius: 10px;
      box-shadow: 0 4px 10px var(--shadow-color);
    }
    .card {
      background-color: var(--primary-bg);
      border-radius: 8px;
      padding: 20px;
      margin-bottom: 20px;
      box-shadow: 0 2px 5px var(--shadow-color);
      text-align: center;
    }
    h3 {
      color: var(--accent-color);
      margin-top: 0;
      font-size: 1.5rem;
    }
    .button {
      padding: 12px 25px;
      font-size: 1.1rem;
      border: none;
      border-radius: 6px;
      color: white;
      cursor: pointer;
      transition: background-color 0.2s, transform 0.1s;
      box-shadow: 0 3px var(--accent-hover);
    }
    .button:active {
      transform: translateY(1px);
      box-shadow: 0 1px var(--accent-hover);
    }
    #ledButton.on { background-color: var(--button-on-bg); }
    #ledButton.on:hover { background-color: var(--button-on-hover); }
    #ledButton.off { background-color: var(--button-off-bg); }
    #ledButton.off:hover { background-color: var(--button-off-hover); }

    /* New styles for a more organized status display */
    .status-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
      gap: 15px;
      margin-top: 20px;
    }
    .status-item {
      background-color: var(--secondary-bg);
      border-radius: 8px;
      padding: 15px;
      box-shadow: 0 2px 5px var(--shadow-color);
      text-align: left;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .status-label {
      font-size: 1.1rem;
      color: var(--text-color);
    }
    .status-value {
      font-size: 1.4rem;
      font-weight: bold;
      color: var(--accent-color);
    }
    /* Chart specific styling for dark mode compatibility */
    canvas {
      background-color: var(--primary-bg);
      border-radius: 8px;
      padding: 10px;
      margin-bottom: 20px; /* Space between charts */
    }

    /* Dark Mode Toggle Switch */
    .switch {
      position: relative;
      display: inline-block;
      width: 50px;
      height: 24px;
      margin-left: 15px;
    }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider-round {
      position: absolute;
      cursor: pointer;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background-color: #ccc;
      -webkit-transition: .4s;
      transition: .4s;
      border-radius: 24px;
    }
    .slider-round:before {
      position: absolute;
      content: "";
      height: 18px;
      width: 18px;
      left: 3px;
      bottom: 3px;
      background-color: white;
      -webkit-transition: .4s;
      transition: .4s;
      border-radius: 50%;
    }
    input:checked + .slider-round { background-color: var(--accent-color); }
    input:checked + .slider-round:before { -webkit-transform: translateX(26px); -ms-transform: translateX(26px); transform: translateX(26px); }

    /* Responsive adjustments */
    @media (max-width: 600px) {
        .header {
            flex-direction: column;
            align-items: flex-start;
            padding: 15px;
        }
        .nav {
            margin-top: 10px;
            margin-right: 0;
            width: 100%;
            text-align: center;
        }
        .nav a {
            display: block;
            margin: 5px 0;
        }
        .status-grid {
            grid-template-columns: 1fr; /* Stack items on small screens */
        }
    }
  </style>
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.2/dist/chart.umd.min.js"></script>
</head>
<body>
  <div class="header">
    <h2>ESP32 Dashboard</h2>
    <div style="display: flex; align-items: center;">
        <nav class="nav">
            <a href="/">Home</a>
            <a href="/status">Status</a> <a href="/settings">Settings</a>
            <a href="/about">About</a>
        </nav>
        <label class="switch">
            <input type="checkbox" id="darkModeToggle">
            <span class="slider-round"></span>
        </label>
    </div>
  </div>
  <div class="content">
    %PAGE_CONTENT%
  </div>

  <script>
    // Global chart instances
    let rssiChart, cpuChart, heapChart;
    const MAX_DATA_POINTS = 30; // Max points on the graph

    // --- General JavaScript for Dark Mode and common functions ---
    document.addEventListener('DOMContentLoaded', (event) => {
      const darkModeToggle = document.getElementById('darkModeToggle');
      const html = document.documentElement;

      // Initialize dark mode based on localStorage
      if (localStorage.getItem('darkMode') === 'enabled') {
        html.classList.add('dark-mode');
        darkModeToggle.checked = true;
      }

      // Toggle dark mode on switch change
      darkModeToggle.addEventListener('change', () => {
        if (darkModeToggle.checked) {
          html.classList.add('dark-mode');
          localStorage.setItem('darkMode', 'enabled');
        } else {
          html.classList.remove('dark-mode');
          localStorage.setItem('darkMode', 'disabled');
        }
        updateChartTheme(); // Always update chart theme on toggle
      });

      // Initialize charts only if on the status page
      if (window.location.pathname === '/status') {
          initializeCharts();
      }

      // Fetch status initially and periodically if on the home or status page
      if (window.location.pathname === '/' || window.location.pathname === '/status') {
          fetchStatus(); // Initial fetch
          setInterval(fetchStatus, 3000); // Update every 3 seconds
      }
    });

    // --- Specific functions for the home page ---
    function toggleLED() {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/toggleLED", true);
      xhr.send();
    }

    function fetchStatus() {
      var xhr = new XMLHttpRequest();
      xhr.onreadystatechange = function() {
        if (xhr.readyState == 4 && xhr.status == 200) {
          try {
            var data = JSON.parse(xhr.responseText);
            updateUI(data);
          } catch (e) {
            console.error("Error parsing status JSON or non-JSON response: ", e, xhr.responseText);
          }
        }
      };
      // THIS IS THE CRITICAL CHANGE: Requesting JSON from /api/status
      xhr.open("GET", "/api/status", true);
      xhr.send();
    }

    // --- Chart.js related variables and functions ---

    function getChartColors() {
      const isDarkMode = document.documentElement.classList.contains('dark-mode');
      return {
        lineColor: isDarkMode ? '#61afef' : '#007bff', // Accent color
        textColor: isDarkMode ? '#e0e0e0' : '#333333',
        gridColor: isDarkMode ? '#4a4e54' : '#e0e0e0', // Adjust grid line color
        borderColor: isDarkMode ? '#4a4e54' : '#e0e0e0', // Adjust axis line color
      };
    }

    function updateChartTheme() {
      const colors = getChartColors();
      const newOptions = {
        scales: {
          x: {
            grid: { color: colors.gridColor },
            ticks: { color: colors.textColor },
            border: { color: colors.borderColor }
          },
          y: {
            grid: { color: colors.gridColor },
            ticks: { color: colors.textColor },
            border: { color: colors.borderColor }
          }
        },
        plugins: {
          legend: {
            labels: { color: colors.textColor }
          }
        }
      };

      if (rssiChart) {
        rssiChart.data.datasets[0].borderColor = colors.lineColor;
        rssiChart.data.datasets[0].backgroundColor = colors.lineColor + '22'; // Slight transparency for fill
        Object.assign(rssiChart.options, newOptions);
        rssiChart.update();
      }
      if (cpuChart) {
        cpuChart.data.datasets[0].borderColor = colors.lineColor;
        cpuChart.data.datasets[0].backgroundColor = colors.lineColor + '22';
        Object.assign(cpuChart.options, newOptions);
        cpuChart.update();
      }
      if (heapChart) {
        heapChart.data.datasets[0].borderColor = colors.lineColor;
        heapChart.data.datasets[0].backgroundColor = colors.lineColor + '22';
        Object.assign(heapChart.options, newOptions);
        heapChart.update();
      }
    }

    function initializeCharts() {
        const colors = getChartColors();

        // Destroy existing charts if re-initializing (e.g., navigating away and back)
        if (rssiChart) rssiChart.destroy();
        if (cpuChart) cpuChart.destroy();
        if (heapChart) heapChart.destroy();

        // RSSI Chart
        const ctxRssi = document.getElementById('rssiChart');
        if (ctxRssi) { // Ensure canvas element exists
            rssiChart = new Chart(ctxRssi.getContext('2d'), {
                type: 'line',
                data: { labels: [], datasets: [{ label: 'WiFi RSSI (dBm)', data: [], borderColor: colors.lineColor, backgroundColor: colors.lineColor + '22', tension: 0.3, fill: true, pointRadius: 2 }] },
                options: {
                    animation: false,
                    responsive: true,
                    maintainAspectRatio: true,
                    scales: {
                        x: { grid: { color: colors.gridColor }, ticks: { color: colors.textColor }, border: { color: colors.borderColor } },
                        y: { grid: { color: colors.gridColor }, ticks: { color: colors.textColor }, min: -90, max: -30, border: { color: colors.borderColor } } // Typical RSSI range
                    },
                    plugins: { legend: { labels: { color: colors.textColor } } }
                }
            });
        }

        // CPU Utilization Chart
        const ctxCpu = document.getElementById('cpuChart');
        if (ctxCpu) {
            cpuChart = new Chart(ctxCpu.getContext('2d'), {
                type: 'line',
                data: { labels: [], datasets: [{ label: 'CPU Utilization (%)', data: [], borderColor: colors.lineColor, backgroundColor: colors.lineColor + '22', tension: 0.3, fill: true, pointRadius: 2 }] },
                options: {
                    animation: false,
                    responsive: true,
                    maintainAspectRatio: true,
                    scales: {
                        x: { grid: { color: colors.gridColor }, ticks: { color: colors.textColor }, border: { color: colors.borderColor } },
                        y: { grid: { color: colors.gridColor }, ticks: { color: colors.textColor }, min: 0, max: 100, beginAtZero: true, border: { color: colors.borderColor } }
                    },
                    plugins: { legend: { labels: { color: colors.textColor } } }
                }
            });
        }

        // Free Heap Memory Chart
        const ctxHeap = document.getElementById('heapChart');
        if (ctxHeap) {
            heapChart = new Chart(ctxHeap.getContext('2d'), {
                type: 'line',
                data: { labels: [], datasets: [{ label: 'Free Heap (KB)', data: [], borderColor: colors.lineColor, backgroundColor: colors.lineColor + '22', tension: 0.3, fill: true, pointRadius: 2 }] },
                options: {
                    animation: false,
                    responsive: true,
                    maintainAspectRatio: true,
                    scales: {
                        x: { grid: { color: colors.gridColor }, ticks: { color: colors.textColor }, border: { color: colors.borderColor } },
                        y: { grid: { color: colors.gridColor }, ticks: { color: colors.textColor }, beginAtZero: true, border: { color: colors.borderColor } } // Auto scale max
                    },
                    plugins: { legend: { labels: { color: colors.textColor } } }
                }
            });
        }
        updateChartTheme(); // Apply initial theme after creating charts
    }

    function addDataToChart(chart, label, newData) {
        if (!chart) return; // Chart might not be initialized on all pages
        chart.data.labels.push(label);
        chart.data.datasets[0].data.push(newData);
        if (chart.data.labels.length > MAX_DATA_POINTS) {
            chart.data.labels.shift();
            chart.data.datasets[0].data.shift();
        }
        chart.update();
    }

    function updateUI(data) {
        const timestamp = new Date().toLocaleTimeString();

        // Update elements that might be present on either home or status page
        if (data.ledState !== undefined) {
            const button = document.getElementById('ledButton');
            const stateText = document.getElementById('ledState');
            if (button && stateText) { // Only attempt update if elements exist on current page
                if (data.ledState === "ON") {
                    button.textContent = 'Turn LED OFF';
                    button.className = 'button on';
                    stateText.textContent = 'ON';
                } else {
                    button.textContent = 'Turn LED ON';
                    button.className = 'button off';
                    stateText.textContent = 'OFF';
                }
            }
        }
        if (data.uptime !== undefined) {
            const uptimeElem = document.getElementById('uptime');
            if (uptimeElem) uptimeElem.textContent = data.uptime;
        }
        if (data.rssi !== undefined) {
            const rssiElem = document.getElementById('rssi');
            if (rssiElem) rssiElem.textContent = data.rssi + ' dBm';
        }

        // Update elements specific to the Status page
        if (window.location.pathname === '/status') {
            if (data.chipId !== undefined) {
                const chipIdElem = document.getElementById('chipId');
                if (chipIdElem) chipIdElem.textContent = data.chipId;
            }
            if (data.flashSize !== undefined) {
                const flashSizeElem = document.getElementById('flashSize');
                if (flashSizeElem) flashSizeElem.textContent = (data.flashSize / (1024 * 1024)).toFixed(0) + ' MB';
            }
            if (data.heapFree !== undefined) {
                const heapFreeElem = document.getElementById('heapFree');
                if (heapFreeElem) heapFreeElem.textContent = (data.heapFree / 1024).toFixed(1) + ' KB';
                addDataToChart(heapChart, timestamp, (data.heapFree / 1024));
            }
            if (data.heapTotal !== undefined) { // heapTotal is displayed on status page, not graphed
                const heapTotalElem = document.getElementById('heapTotal');
                if (heapTotalElem) heapTotalElem.textContent = (data.heapTotal / 1024).toFixed(1) + ' KB';
            }
            if (data.coreTemp !== undefined) {
                const coreTempElem = document.getElementById('coreTemp');
                if (coreTempElem) coreTempElem.textContent = data.coreTemp.toFixed(1) + ' °C';
            }
            if (data.cpuUtilization !== undefined) {
                const cpuUtilElem = document.getElementById('cpuUtilization');
                if (cpuUtilElem) cpuUtilElem.textContent = data.cpuUtilization.toFixed(1) + ' %';
                addDataToChart(cpuChart, timestamp, data.cpuUtilization);
            }
            if (data.sdkVersion !== undefined) {
                // SDK version is replaced server-side for the status page, no need to update via JS
                // const sdkVersionElem = document.getElementById('sdkVersion');
                // if (sdkVersionElem) sdkVersionElem.textContent = data.sdkVersion;
            }

            // Always add RSSI to chart if on status page
            if (data.rssi !== undefined) {
                addDataToChart(rssiChart, timestamp, data.rssi);
            }
        }
    }

  </script>
</body>
</html>
)rawliteral";

// --- Page Specific Content ---

const char home_page_content[] PROGMEM = R"rawliteral(
    <div class="card">
      <h3>💡 LED Control</h3>
      <p>Current State: <span id="ledState">--</span></p>
      <button id="ledButton" class="button off" onclick="toggleLED()">Toggle LED</button>
    </div>
    <div class="card">
      <h3>📊 Basic Device Info</h3>
      <div class="status-grid">
        <div class="status-item">
          <span class="status-label">Uptime:</span>
          <span id="uptime" class="status-value">--</span>
        </div>
        <div class="status-item">
          <span class="status-label">WiFi RSSI:</span>
          <span id="rssi" class="status-value">-- dBm</span>
        </div>
      </div>
    </div>
)rawliteral";

const char status_page_content[] PROGMEM = R"rawliteral(
    <div class="card">
      <h3>⚡ Live ESP32 Status</h3>
      <div class="status-grid">
          <div class="status-item">
              <span class="status-label">Uptime:</span>
              <span id="uptime" class="status-value">--</span>
          </div>
          <div class="status-item">
              <span class="status-label">WiFi RSSI:</span>
              <span id="rssi" class="status-value">-- dBm</span>
          </div>
          <div class="status-item">
              <span class="status-label">Free Heap:</span>
              <span id="heapFree" class="status-value">-- KB</span>
          </div>
          <div class="status-item">
              <span class="status-label">Total Heap:</span> <span id="heapTotal" class="status-value">-- KB</span>
          </div>
          <div class="status-item">
              <span class="status-label">CPU Usage:</span>
              <span id="cpuUtilization" class="status-value">-- %</span>
          </div>
          <div class="status-item">
              <span class="status-label">Core Temp:</span>
              <span id="coreTemp" class="status-value">-- °C</span>
          </div>
          <div class="status-item">
              <span class="status-label">Chip ID:</span>
              <span id="chipId" class="status-value">--</span>
          </div>
          <div class="status-item">
              <span class="status-label">Flash Size:</span>
              <span id="flashSize" class="status-value">-- MB</span>
          </div>
           <div class="status-item">
              <span class="status-label">SDK Version:</span>
              <span id="sdkVersion" class="status-value">%SDK_VERSION%</span>
          </div>
      </div>
    </div>
    <div class="card">
        <h3>📈 Performance Graphs</h3>
        <p>Live data updates every 3 seconds.</p>
        <div style="width: 100%;">
            <canvas id="rssiChart"></canvas>
        </div>
        <div style="width: 100%;">
            <canvas id="cpuChart"></canvas>
        </div>
        <div style="width: 100%;">
            <canvas id="heapChart"></canvas>
        </div>
    </div>
)rawliteral";


const char settings_page_content[] PROGMEM = R"rawliteral(
    <div class="card">
      <h3>⚙️ Device Settings</h3>
      <p>This is a placeholder for your device settings.</p>
      <p>You could add input fields, sliders, or other controls here to configure your ESP32's behavior.</p>
      <p>Example: Set a temperature threshold, change measurement units, etc.</p>
    </div>
)rawliteral";

const char about_page_content[] PROGMEM = R"rawliteral(
    <div class="card">
      <h3>ℹ️ About This Device</h3>
      <p>This is an ESP32 micro-controller running custom firmware.</p>
      <p>Firmware Version: 1.0.0</p>
      <p>Developed by: Your Name/Company</p>
      <p>Learn more about ESP32 development: <a href="https://docs.espressif.com/projects/arduino-esp32/en/latest/" target="_blank" style="color: var(--accent-color);">Espressif Arduino Core</a></p>
    </div>
)rawliteral";

// --- Helper: Format Uptime ---
String formatUptime(unsigned long millis) {
    unsigned long seconds = millis / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    hours %= 24; minutes %= 60; seconds %= 60;
    char buf[50];
    if (days > 0) {
        sprintf(buf, "%lu d %02lu:%02lu:%02lu", days, hours, minutes, seconds);
    } else {
        sprintf(buf, "%02lu:%02lu:%02lu", hours, minutes, seconds);
    }
    return String(buf);
}

// --- Request Handlers ---

// Function to serve any page by replacing placeholders
void servePage(const char* pageContent, const char* title) {
  String html = base_html_template;
  html.replace("%TITLE%", title);

  // Replace SDK version specifically on the status page
  String content = pageContent;
  if (strcmp(title, "Status | ESP32 Dashboard") == 0) {
      content.replace("%SDK_VERSION%", ESP.getSdkVersion());
  }

  html.replace("%PAGE_CONTENT%", content);
  server.send(200, "text/html", html);
}

// Handle root URL "/"
void handleRoot() {
  servePage(home_page_content, "Home | ESP32 Dashboard");
}

// Handle "/status" for the HTML page
void handleStatusPage() {
  servePage(status_page_content, "Status | ESP32 Dashboard");
}

// Handle "/settings"
void handleSettings() {
  servePage(settings_page_content, "Settings | ESP32 Dashboard");
}

// Handle "/about"
void handleAbout() {
  servePage(about_page_content, "About | ESP32 Dashboard");
}

// Handle "/toggleLED"
void handleToggleLED() {
  ledState = !ledState;
  digitalWrite(ledPin, ledState);
  Serial.print("LED state: ");
  Serial.println(ledState ? "ON" : "OFF");
  server.send(200, "text/plain", "OK");
}

// Handle "/api/status" for JSON data (used by AJAX polling)
void handleStatusJson() {
  StaticJsonDocument<256> doc; // Increased size slightly to accommodate all new data
  doc["ledState"] = ledState ? "ON" : "OFF";
  doc["uptime"] = formatUptime(millis());
  doc["rssi"] = WiFi.RSSI();
  // Using ESP.getEfuseMac() for a more unique Chip ID
  doc["chipId"] = String((uint32_t)(ESP.getEfuseMac() >> 24), HEX); // High 3 bytes for concise ID
  doc["flashSize"] = ESP.getFlashChipSize();
  doc["heapFree"] = ESP.getFreeHeap();
  doc["heapTotal"] = ESP.getHeapSize(); // Added total heap
  doc["coreTemp"] = temperatureRead();
  doc["cpuUtilization"] = cpuUtilization; // Our calculated CPU utilization
  doc["sdkVersion"] = ESP.getSdkVersion();

  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, ledState);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { // 20 attempts x 500ms = 10 seconds timeout
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi. Please check SSID/Password. Halting.");
    while(true); // Halt execution if WiFi fails
  }

  // Define HTTP routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatusPage);    // HTML page for status
  server.on("/api/status", HTTP_GET, handleStatusJson); // JSON data endpoint
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/about", HTTP_GET, handleAbout);
  server.on("/toggleLED", HTTP_GET, handleToggleLED);


  // ElegantOTA setup (for standard WebServer)
  ElegantOTA.begin(&server, ota_username, ota_password);

  // Handle 404 Not Found
  server.onNotFound([](){
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    server.send(404, "text/plain", message);
  });

  // Start the web server
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("OTA available at http://" + WiFi.localIP().toString() + "/update");

  // Initialize prevLoopMicros for CPU calculation
  prevLoopMicros = micros();
}

void loop() {
  // CPU Utilization calculation (approximation)
  // This needs to be at the very top of loop() for accuracy
  // Calculate total time elapsed since last loop measurement
  unsigned long currentLoopMicros = micros();
  unsigned long loopDurationMicros = currentLoopMicros - prevLoopMicros;
  prevLoopMicros = currentLoopMicros;

  // Calculate work duration (total loop duration - idle duration)
  // Ensure we don't divide by zero if loopDurationMicros is 0
  if (loopDurationMicros > 0) {
      float workDurationMicros = (float)loopDurationMicros - idleMicros;
      cpuUtilization = (workDurationMicros / loopDurationMicros) * 100.0;
  } else {
      cpuUtilization = 0.0; // No loop duration, assume 0% usage
  }

  // Clamp values between 0 and 100
  if (cpuUtilization < 0.0) cpuUtilization = 0.0;
  if (cpuUtilization > 100.0) cpuUtilization = 100.0;

  idleMicros = 0; // Reset idle counter for next iteration

  // This is crucial for the WebServer and ElegantOTA to process requests
  server.handleClient();
  ElegantOTA.loop(); // Must be called in loop for OTA to work
}

// This function is a FreeRTOS hook that gets called when the current task (your loop) is idle.
// We use it to measure how much time the CPU spends doing "nothing".
extern "C" void esp_idf_hook_idle(void) {
    static unsigned long lastIdleCheckMicros = 0;
    unsigned long currentMicros = micros();
    // Only accumulate idle time if it's been a reasonable duration since last check
    // This avoids accumulating tiny times if the hook is called extremely frequently
    // and also handles potential micros() rollovers gracefully within a short window.
    if (currentMicros >= lastIdleCheckMicros) { // Handle potential rollover if currentMicros < lastIdleCheckMicros
        idleMicros += (currentMicros - lastIdleCheckMicros);
    }
    lastIdleCheckMicros = currentMicros;
}