#include <WiFi.h>
#include <WebServer.h>      // Standard WebServer library
#include <ElegantOTA.h>     // ElegantOTA library
#include <ArduinoJson.h>    // For handling JSON data
#include <esp_system.h>     // For ESP.getFreeHeap(), ESP.getChipId(), etc.
#include <SPIFFS.h>         // For storing configuration files

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

// Configuration file path on SPIFFS
const char* configFilePath = "/config.json";

// --- HTML Templates ---
// Base HTML structure with a futuristic theme
const char base_html_template[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22><text y=%22.9em%22 font-size=%2290%22>⚡</text></svg>">
  <title>%TITLE%</title>
  <style>
    :root {
      --primary-bg: #1a1a2e;
      --secondary-bg: #16213e;
      --card-bg: #0f3460;
      --text-color: #e0e0e0;
      --accent-color: #e94560;
      --highlight-color: #53bf9d;
      --border-color: #3a476a;
      --shadow-color: rgba(0, 0, 0, 0.4);
      --font-main: 'Roboto', sans-serif;
      --font-tech: 'Orbitron', sans-serif;
    }
    @import url('https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700&family=Roboto:wght@300;400;700&display=swap');
    body {
      font-family: var(--font-main);
      margin: 0; padding: 0;
      background-color: var(--primary-bg);
      color: var(--text-color);
      transition: background-color 0.3s, color 0.3s;
      font-weight: 300;
      overflow-x: hidden;
    }
    .header {
      background: linear-gradient(90deg, var(--secondary-bg) 0%, var(--card-bg) 100%);
      padding: 15px 30px;
      box-shadow: 0 4px 15px var(--shadow-color);
      display: flex; justify-content: space-between; align-items: center;
      border-bottom: 1px solid var(--accent-color);
      position: sticky; top: 0; z-index: 1000;
    }
    .header h2 {
      margin: 0; font-size: 1.8rem;
      font-family: var(--font-tech); color: var(--accent-color);
      text-shadow: 0 0 5px var(--accent-color);
    }
    .nav a {
      color: var(--text-color); text-decoration: none;
      padding: 10px 18px; margin: 0 5px; border-radius: 5px;
      transition: background-color 0.3s, color 0.3s, transform 0.2s;
      border: 1px solid transparent;
      font-family: var(--font-tech); font-size: 0.9rem;
    }
    .nav a:hover, .nav a.active {
      background-color: var(--accent-color); color: var(--primary-bg);
      transform: translateY(-2px);
      box-shadow: 0 2px 8px rgba(233, 69, 96, 0.5);
      border-color: var(--accent-color);
    }
    .content {
      max-width: 1100px; margin: 30px auto; padding: 30px;
      background-color: var(--secondary-bg);
      border-radius: 15px; box-shadow: 0 8px 25px var(--shadow-color);
      border: 1px solid var(--border-color);
    }
    .card {
      background-color: var(--card-bg); border-radius: 10px;
      padding: 25px; margin-bottom: 25px;
      box-shadow: inset 0 0 10px rgba(0,0,0,0.3), 0 3px 6px var(--shadow-color);
      border-left: 5px solid var(--accent-color);
      transition: transform 0.2s ease-in-out;
    }
    .card:hover { transform: translateY(-3px); }
    h3 {
      color: var(--highlight-color); margin-top: 0;
      font-family: var(--font-tech); font-size: 1.6rem;
      border-bottom: 1px solid var(--border-color); padding-bottom: 10px; margin-bottom: 20px;
    }
    .button, button {
      padding: 12px 30px; font-size: 1.1rem; border: none;
      border-radius: 8px; color: white; cursor: pointer;
      transition: background-color 0.3s, transform 0.2s, box-shadow 0.3s;
      background: linear-gradient(145deg, var(--accent-color), #c0392b);
      box-shadow: 0 4px 8px rgba(233, 69, 96, 0.3);
      font-family: var(--font-tech); text-transform: uppercase;
    }
    .button:hover, button:hover {
      transform: translateY(-2px) scale(1.02);
      box-shadow: 0 6px 12px rgba(233, 69, 96, 0.5);
    }
    .button:active, button:active {
        transform: translateY(0px) scale(1);
        box-shadow: 0 2px 4px rgba(233, 69, 96, 0.3);
    }
    .btn-secondary { background: linear-gradient(145deg, var(--highlight-color), #46a086); box-shadow: 0 4px 8px rgba(83, 191, 157, 0.3); }
    .btn-secondary:hover { box-shadow: 0 6px 12px rgba(83, 191, 157, 0.5); }
    .btn-danger { background: linear-gradient(145deg, #e74c3c, #c0392b); box-shadow: 0 4px 8px rgba(231, 76, 60, 0.3); }
    .btn-danger:hover { box-shadow: 0 6px 12px rgba(231, 76, 60, 0.5); }
    .status-grid, .home-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
      gap: 20px; margin-top: 20px;
    }
    .status-item {
      background-color: var(--secondary-bg); border-radius: 8px;
      padding: 20px; box-shadow: 0 2px 5px var(--shadow-color);
      display: flex; justify-content: space-between; align-items: center;
      border: 1px solid var(--border-color);
    }
    .status-label { font-size: 1.1rem; color: var(--text-color); opacity: 0.8; }
    .status-value { font-size: 1.4rem; font-weight: bold; color: var(--highlight-color); font-family: var(--font-tech); }
    .home-button-card {
        background-color: var(--card-bg); border-radius: 10px;
        padding: 25px; text-align: center;
        box-shadow: 0 4px 10px var(--shadow-color);
        border: 1px solid var(--border-color);
        transition: transform 0.3s, box-shadow 0.3s;
        cursor: pointer; display: flex; flex-direction: column;
        justify-content: center; align-items: center; min-height: 150px;
    }
    .home-button-card:hover { transform: scale(1.05); box-shadow: 0 8px 20px var(--shadow-color); }
    .home-button-card h4 { margin: 10px 0 0; font-size: 1.3rem; color: var(--highlight-color); font-family: var(--font-tech); }
    .home-button-card p { margin: 5px 0 0; font-size: 0.9rem; opacity: 0.7; }
    .home-button-card i { font-size: 2.5rem; color: var(--accent-color); margin-bottom: 10px; } /* For icons */
    input[type="text"], input[type="number"], select {
        width: calc(100% - 24px); padding: 12px; margin-bottom: 15px;
        border: 1px solid var(--border-color); border-radius: 5px;
        background-color: var(--primary-bg); color: var(--text-color);
        font-size: 1rem;
    }
    label { display: block; margin-bottom: 8px; font-weight: bold; color: var(--highlight-color); }
    #settings-buttons-list li {
        background-color: var(--secondary-bg); padding: 15px; margin-bottom: 10px;
        border-radius: 5px; display: flex; justify-content: space-between;
        align-items: center; border-left: 3px solid var(--highlight-color);
    }
    #settings-buttons-list button { padding: 8px 15px; font-size: 0.9rem; margin-left: 5px; }
    .modal {
        display: none; position: fixed; z-index: 2000; left: 0; top: 0;
        width: 100%; height: 100%; overflow: auto;
        background-color: rgba(0,0,0,0.7);
    }
    .modal-content {
        background-color: var(--secondary-bg); margin: 10% auto; padding: 30px;
        border: 1px solid var(--accent-color); width: 80%; max-width: 500px;
        border-radius: 10px; box-shadow: 0 0 25px var(--accent-color);
        animation: slideIn 0.5s ease;
    }
    @keyframes slideIn { from { transform: translateY(-50px); opacity: 0; } to { transform: translateY(0); opacity: 1; } }
    .close-button { color: #aaa; float: right; font-size: 28px; font-weight: bold; }
    .close-button:hover, .close-button:focus { color: var(--accent-color); text-decoration: none; cursor: pointer; }
    @media (max-width: 768px) {
        .header { flex-direction: column; align-items: center; }
        .nav { margin-top: 15px; width: 100%; text-align: center; }
        .nav a { display: inline-block; margin: 5px; }
        .content { padding: 15px; }
        .status-grid, .home-grid { grid-template-columns: 1fr; }
    }
  </style>
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.1/css/all.min.css"> </head>
<body>
  <div class="header">
    <h2><i class="fas fa-microchip"></i> ESP32 Nexus</h2>
    <nav class="nav">
      <a href="/" id="nav-home">Home</a>
      <a href="/status" id="nav-status">Status</a>
      <a href="/settings" id="nav-settings">Settings</a>
      <a href="/update" target="_blank">OTA</a>
    </nav>
  </div>
  <div class="content">
    %PAGE_CONTENT%
  </div>
  <script>
    // --- General JavaScript ---
    document.addEventListener('DOMContentLoaded', (event) => {
      // Set active nav link
      const path = window.location.pathname;
      if (path === '/') document.getElementById('nav-home').classList.add('active');
      else if (path === '/status') document.getElementById('nav-status').classList.add('active');
      else if (path === '/settings') document.getElementById('nav-settings').classList.add('active');

      // Load page-specific script
      if (path === '/') loadHomePageScript();
      else if (path === '/status') loadStatusPageScript();
      else if (path === '/settings') loadSettingsPageScript();
    });

    function showNotification(message, type = 'success') {
        const notif = document.createElement('div');
        notif.style.position = 'fixed';
        notif.style.bottom = '20px';
        notif.style.right = '20px';
        notif.style.padding = '15px 25px';
        notif.style.borderRadius = '8px';
        notif.style.color = 'white';
        notif.style.zIndex = '3000';
        notif.style.transition = 'opacity 0.5s, transform 0.5s';
        notif.style.fontFamily = 'var(--font-main)';
        notif.style.boxShadow = '0 5px 15px rgba(0,0,0,0.3)';

        if (type === 'success') {
            notif.style.backgroundColor = 'var(--highlight-color)';
        } else {
            notif.style.backgroundColor = 'var(--accent-color)';
        }
        notif.textContent = message;
        document.body.appendChild(notif);

        setTimeout(() => {
            notif.style.opacity = '0';
            notif.style.transform = 'translateX(20px)';
        }, 3000);

        setTimeout(() => {
            document.body.removeChild(notif);
        }, 3500);
    }

    // --- Home Page Script ---
    function loadHomePageScript() {
        const homeGrid = document.getElementById('home-grid');
        if (!homeGrid) return;

        fetch('/api/config')
            .then(response => response.json())
            .then(config => {
                homeGrid.innerHTML = ''; // Clear existing
                if (config && config.buttons) { // Check if config and buttons exist
                    config.buttons.forEach(button => {
                        const card = document.createElement('div');
                        card.className = 'home-button-card';
                        card.dataset.action = button.action;
                        card.innerHTML = `
                            <i class="${button.icon || 'fas fa-question-circle'}"></i>
                            <h4>${button.name}</h4>
                            <p>${button.action}</p>
                        `;
                        card.addEventListener('click', () => {
                            fetch('/api/action', {
                                method: 'POST',
                                headers: { 'Content-Type': 'application/json' },
                                body: JSON.stringify({ action: button.action })
                            })
                            .then(response => response.json())
                            .then(data => {
                                console.log('Action response:', data);
                                showNotification(`Action '${button.name}' executed: ${data.status}`);
                            })
                            .catch(err => {
                                console.error('Action failed:', err);
                                showNotification(`Action '${button.name}' failed!`, 'error');
                            });
                        });
                        homeGrid.appendChild(card);
                    });
                } else {
                    homeGrid.innerHTML = '<p>No buttons configured or error loading configuration.</p>';
                }
            })
            .catch(err => {
                console.error('Failed to load home config:', err);
                homeGrid.innerHTML = '<p>Error loading button configuration.</p>';
            });
    }

    // --- Status Page Script ---
    function loadStatusPageScript() {
        const uptimeElem = document.getElementById('uptime');
        const rssiElem = document.getElementById('rssi');
        const heapElem = document.getElementById('heap');
        const chipIdElem = document.getElementById('chipId');
        const ipElem = document.getElementById('ip');

        function fetchStatus() {
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    if (uptimeElem) uptimeElem.textContent = data.uptime;
                    if (rssiElem) rssiElem.textContent = data.rssi + ' dBm';
                    if (heapElem) heapElem.textContent = (data.heapFree / 1024).toFixed(1) + ' KB';
                    if (chipIdElem) chipIdElem.textContent = data.chipId;
                    if (ipElem) ipElem.textContent = data.ip;
                })
                .catch(err => console.error('Failed to fetch status:', err));
        }
        fetchStatus(); // Initial fetch
        setInterval(fetchStatus, 5000); // Update every 5 seconds
    }

    // --- Settings Page Script ---
    function loadSettingsPageScript() {
        const buttonsList = document.getElementById('settings-buttons-list');
        const addButton = document.getElementById('add-button');
        const modal = document.getElementById('edit-modal');
        const closeBtn = document.querySelector('.close-button');
        const saveButton = document.getElementById('save-button');
        const saveAllButton = document.getElementById('save-all-button');
        const form = document.getElementById('edit-form');
        let currentConfig = { buttons: [] };
        let editingIndex = -1;

        function renderButtons() {
            buttonsList.innerHTML = '';
            if (currentConfig && currentConfig.buttons) { // Check if config and buttons exist
                currentConfig.buttons.forEach((btn, index) => {
                    const li = document.createElement('li');
                    li.innerHTML = `
                        <span>
                            <i class="${btn.icon || 'fas fa-cogs'}"></i>
                            <strong>${btn.name}</strong> (${btn.action})
                        </span>
                        <div>
                            <button class="btn-secondary" onclick="editButton(${index})"><i class="fas fa-edit"></i> Edit</button>
                            <button class="btn-danger" onclick="deleteButton(${index})"><i class="fas fa-trash"></i> Delete</button>
                        </div>
                    `;
                    buttonsList.appendChild(li);
                });
            }
        }

        window.editButton = function(index) {
            editingIndex = index;
            const btn = currentConfig.buttons[index];
            document.getElementById('btn-id').value = btn.id;
            document.getElementById('btn-name').value = btn.name;
            document.getElementById('btn-action').value = btn.action;
            document.getElementById('btn-icon').value = btn.icon || 'fas fa-cogs';
            modal.style.display = 'block';
        }

        window.deleteButton = function(index) {
            if (confirm('Are you sure you want to delete this button?')) {
                currentConfig.buttons.splice(index, 1);
                renderButtons();
                showNotification('Button deleted. Click "Save All" to make it permanent.', 'success');
            }
        }

        addButton.onclick = () => {
            editingIndex = -1; // New button
            form.reset();
            document.getElementById('btn-id').value = 'btn_' + Date.now(); // Unique ID
            document.getElementById('btn-icon').value = 'fas fa-plus-circle';
            modal.style.display = 'block';
        };

        closeBtn.onclick = () => {
            modal.style.display = 'none';
        };

        window.onclick = (event) => {
            if (event.target == modal) {
                modal.style.display = 'none';
            }
        };

        saveButton.onclick = () => {
            const newButton = {
                id: document.getElementById('btn-id').value,
                name: document.getElementById('btn-name').value,
                action: document.getElementById('btn-action').value,
                icon: document.getElementById('btn-icon').value
            };
            if (!newButton.name || !newButton.action) {
                alert('Name and Action are required!');
                return;
            }
            if (!currentConfig.buttons) { // Initialize if buttons array is null/undefined
                currentConfig.buttons = [];
            }
            if (editingIndex === -1) {
                currentConfig.buttons.push(newButton);
            } else {
                currentConfig.buttons[editingIndex] = newButton;
            }
            modal.style.display = 'none';
            renderButtons();
            showNotification('Button saved locally. Click "Save All" to update the device.', 'success');
        };

        saveAllButton.onclick = () => {
            fetch('/api/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(currentConfig)
            })
            .then(response => {
                if (response.ok) {
                    showNotification('Configuration saved successfully!', 'success');
                } else {
                    response.text().then(text => { // Get more error details
                        showNotification(`Failed to save configuration: ${text}`, 'error');
                        console.error('Save failed response:', text);
                    });
                }
            })
            .catch(err => {
                console.error('Save failed:', err);
                showNotification('Error during save!', 'error');
            });
        };

        // Load initial config
        fetch('/api/config')
            .then(response => response.json())
            .then(config => {
                currentConfig = config;
                if (!currentConfig.buttons) { // Ensure buttons array exists
                   currentConfig.buttons = [];
                }
                renderButtons();
            })
            .catch(err => {
                console.error('Failed to load settings config:', err);
                currentConfig = { buttons: [] }; // Default to empty if load fails
                renderButtons();
            });
    }
  </script>
</body>
</html>
)rawliteral";

// --- Page Specific Content ---

const char home_page_content[] PROGMEM = R"rawliteral(
  <div class="card">
    <h3><i class="fas fa-rocket"></i> Quick Actions</h3>
    <p>Your configured actions appear below. Click any button to trigger its function.</p>
    <div class="home-grid" id="home-grid">
      <p>Loading buttons...</p>
    </div>
  </div>
)rawliteral";

const char status_page_content[] PROGMEM = R"rawliteral(
  <div class="card">
    <h3><i class="fas fa-tachometer-alt"></i> Device Status</h3>
    <div class="status-grid">
      <div class="status-item">
        <span class="status-label"><i class="fas fa-clock"></i> Uptime:</span>
        <span id="uptime" class="status-value">--</span>
      </div>
      <div class="status-item">
        <span class="status-label"><i class="fas fa-wifi"></i> WiFi RSSI:</span>
        <span id="rssi" class="status-value">-- dBm</span>
      </div>
      <div class="status-item">
        <span class="status-label"><i class="fas fa-memory"></i> Free Heap:</span>
        <span id="heap" class="status-value">-- KB</span>
      </div>
       <div class="status-item">
        <span class="status-label"><i class="fas fa-network-wired"></i> IP Address:</span>
        <span id="ip" class="status-value">--</span>
      </div>
      <div class="status-item">
        <span class="status-label"><i class="fas fa-id-badge"></i> Chip ID:</span>
        <span id="chipId" class="status-value">--</span>
      </div>
      <div class="status-item">
        <span class="status-label"><i class="fas fa-code-branch"></i> SDK Version:</span>
        <span class="status-value">%SDK_VERSION%</span>
      </div>
    </div>
  </div>
  <div class="card">
      <h3><i class="fas fa-info-circle"></i> About</h3>
      <p>This ESP32 is running a custom dashboard firmware.</p>
      <p>You can manage home page buttons from the <a href="/settings" style="color:var(--highlight-color)">Settings</a> page.</p>
      <p>Over-the-Air updates are available via the 'OTA' link in the navigation bar.</p>
  </div>
)rawliteral";

const char settings_page_content[] PROGMEM = R"rawliteral(
  <div class="card">
    <h3><i class="fas fa-tools"></i> Configure Home Buttons</h3>
    <p>Add, edit, or remove the action buttons displayed on the home page.
       Define a 'Name' (what you see), an 'Action ID' (what the ESP32 does),
       and a Font Awesome icon class (e.g., 'fas fa-lightbulb').</p>
    <ul id="settings-buttons-list" style="list-style: none; padding: 0;">
      </ul>
    <button id="add-button" class="btn-secondary" style="margin-top: 20px;"><i class="fas fa-plus"></i> Add New Button</button>
    <button id="save-all-button" style="margin-top: 20px; float: right;"><i class="fas fa-save"></i> Save All Changes</button>
  </div>

  <div id="edit-modal" class="modal">
    <div class="modal-content">
      <span class="close-button">&times;</span>
      <h3>Edit / Add Button</h3>
      <form id="edit-form">
        <input type="hidden" id="btn-id">
        <label for="btn-name">Button Name:</label>
        <input type="text" id="btn-name" placeholder="e.g., Toggle LED" required>
        <label for="btn-action">Action ID:</label>
        <input type="text" id="btn-action" placeholder="e.g., toggle_led (Must match C++ code)" required>
        <label for="btn-icon">Font Awesome Icon:</label>
        <input type="text" id="btn-icon" placeholder="e.g., fas fa-lightbulb">
        <button type="button" id="save-button" class="btn-secondary" style="margin-top: 10px;">Save Button</button>
      </form>
    </div>
  </div>
)rawliteral";

// --- Helper: Format Uptime ---
String formatUptime(unsigned long currentMillis) { // Changed parameter name to avoid conflict
    unsigned long seconds = currentMillis / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    hours %= 24; minutes %= 60; seconds %= 60;
    char buf[50];
    sprintf(buf, "%lu d %02lu:%02lu:%02lu", days, hours, minutes, seconds);
    return String(buf);
}

// --- FORWARD DECLARATION for saveConfiguration ---
// This tells the compiler that a function named saveConfiguration exists
// and what its signature (parameters and return type) looks like.
// The actual implementation (body) of the function is defined later.
void saveConfiguration(JsonDocument& doc);

// --- SPIFFS & Configuration Handling ---
void loadConfiguration(JsonDocument& doc) {
    File configFile = SPIFFS.open(configFilePath, "r");
    if (!configFile || configFile.size() == 0) {
        Serial.println("Failed to open config file or it's empty. Creating default.");
        // Create a default configuration
        // Ensure doc is clear before creating defaults
        doc.clear(); 
        JsonArray buttons = doc.createNestedArray("buttons");
        
        JsonObject button1 = buttons.createNestedObject();
        button1["id"] = "btn_led_default"; // Unique ID
        button1["name"] = "Toggle Onboard LED";
        button1["action"] = "toggle_led";
        button1["icon"] = "fas fa-lightbulb";
        
        JsonObject button2 = buttons.createNestedObject();
        button2["id"] = "btn_reboot_default"; // Unique ID
        button2["name"] = "Reboot ESP32";
        button2["action"] = "reboot";
        button2["icon"] = "fas fa-power-off";
        
        // Call saveConfiguration to write these defaults to SPIFFS
        saveConfiguration(doc); 
        // After saving, we don't need to return immediately. 
        // The 'doc' now holds the default configuration.
        // If we returned, the caller wouldn't have the defaults if the file was just created.
    } else {
        // File exists and is not empty, attempt to parse it
        DeserializationError error = deserializeJson(doc, configFile);
        if (error) {
            Serial.print("Failed to parse config file: ");
            Serial.println(error.c_str());
            // If parsing fails, load defaults into 'doc' as a fallback
            doc.clear();
            JsonArray buttons = doc.createNestedArray("buttons");
            JsonObject button1 = buttons.createNestedObject();
            button1["id"] = "btn_led_fallback";
            button1["name"] = "Toggle LED (Fallback)";
            button1["action"] = "toggle_led";
            button1["icon"] = "fas fa-lightbulb";
        }
        configFile.close();
    }
}

// Definition of saveConfiguration
void saveConfiguration(JsonDocument& doc) {
    File configFile = SPIFFS.open(configFilePath, "w");
    if (!configFile) {
        Serial.println("Failed to open config file for writing");
        return;
    }
    if (serializeJson(doc, configFile) == 0) {
        Serial.println("Failed to write to config file");
    } else {
        Serial.println("Configuration saved to SPIFFS.");
    }
    configFile.close();
}

// --- Action Handler ---
// *** IMPORTANT: Add your custom action logic here! ***
void handleAction(const String& action) {
    Serial.print("Executing action: ");
    Serial.println(action);

    if (action == "toggle_led") {
        ledState = !ledState;
        digitalWrite(ledPin, ledState);
        server.send(200, "application/json", "{\"status\":\"LED Toggled\"}");
    } else if (action == "reboot") {
        server.send(200, "application/json", "{\"status\":\"Rebooting...\"}");
        delay(1000); // Give time for the response to be sent
        ESP.restart();
    }
    // --- ADD MORE ACTIONS HERE ---
    // else if (action == "your_action_id") {
    //   // Do something else
    //   server.send(200, "application/json", "{\"status\":\"Your action done\"}");
    // }
    else {
        server.send(400, "application/json", "{\"status\":\"Unknown action\"}");
    }
}


// --- Request Handlers ---
void servePage(const char* pageContent, const char* title) {
    String html = base_html_template;
    html.replace("%TITLE%", title);
    String content = pageContent; // Make a mutable copy
    content.replace("%SDK_VERSION%", ESP.getSdkVersion());
    html.replace("%PAGE_CONTENT%", content);
    server.send(200, "text/html", html);
}

void handleRoot() { servePage(home_page_content, "Home | ESP32 Nexus"); }
void handleStatusPage() { servePage(status_page_content, "Status | ESP32 Nexus"); }
void handleSettingsPage() { servePage(settings_page_content, "Settings | ESP32 Nexus"); }

void handleStatusJson() {
    StaticJsonDocument<256> doc; // Sufficient for status data
    doc["uptime"] = formatUptime(millis());
    doc["rssi"] = WiFi.RSSI();
    doc["heapFree"] = ESP.getFreeHeap();
    doc["chipId"] = String((uint32_t)(ESP.getEfuseMac() >> 24), HEX); // Use ESP.getEfuseMac() for a more unique ID
    doc["ip"] = WiFi.localIP().toString();

    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

void handleGetConfig() {
    // Load the current configuration into a JsonDocument first
    StaticJsonDocument<1024> doc; // Ensure this is large enough for your config
    loadConfiguration(doc); // This will load from SPIFFS or create defaults

    // Now serialize the content of 'doc' to the client
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

void handlePostConfig() {
    StaticJsonDocument<1024> doc; // Ensure this matches or exceeds client-side expectations
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        Serial.print("deserializeJson() failed during POST config: ");
        Serial.println(error.c_str());
        server.send(400, "text/plain", "Invalid JSON received");
        return;
    }

    // Optional: Add some validation to the received JSON structure if needed
    // For example, check if 'buttons' array exists
    if (!doc.containsKey("buttons") || !doc["buttons"].is<JsonArray>()) {
        Serial.println("Received config JSON is missing 'buttons' array or it's not an array.");
        // server.send(400, "text/plain", "Malformed config: 'buttons' array missing or invalid.");
        // return; // Or attempt to fix/use defaults
    }


    saveConfiguration(doc); // Save the new configuration received from the client
    server.send(200, "text/plain", "Configuration Saved");
}

void handlePostAction() {
    StaticJsonDocument<128> doc; // Small doc for simple action commands
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        server.send(400, "text/plain", "Invalid JSON for action");
        return;
    }

    const char* action = doc["action"]; // Extract the "action" field
    if (action) {
        handleAction(String(action));
    } else {
        server.send(400, "text/plain", "Action not specified in JSON");
    }
}


void setup() {
    Serial.begin(115200);
    delay(100); // Wait for serial to initialize

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, ledState); // Set initial LED state

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) { // true formats SPIFFS if mount fails
        Serial.println("An Error has occurred while mounting SPIFFS. Halting.");
        while (true); // Halt if SPIFFS can't be initialized
    }
    Serial.println("SPIFFS mounted successfully.");

    // Load or create default configuration on startup
    // StaticJsonDocument<1024> tempDoc; // Temporary doc for initial load/creation
    // loadConfiguration(tempDoc); // This ensures config.json exists and is valid

    Serial.print("Connecting to WiFi SSID: ");
    Serial.println(ssid);
    WiFi.mode(WIFI_STA); // Set ESP32 to Wi-Fi station mode
    WiFi.begin(ssid, password);

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime < 20000)) { // 20-second timeout
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected successfully!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nFailed to connect to WiFi. Please check credentials or network. Halting.");
        // Optionally, start an AP mode here as a fallback
        while (true); // Halt execution
    }

    // Define HTTP server routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/status", HTTP_GET, handleStatusPage);
    server.on("/settings", HTTP_GET, handleSettingsPage);

    // API Routes
    server.on("/api/status", HTTP_GET, handleStatusJson);    // Get current device status
    server.on("/api/config", HTTP_GET, handleGetConfig);     // Get current button configuration
    server.on("/api/config", HTTP_POST, handlePostConfig);   // Update button configuration
    server.on("/api/action", HTTP_POST, handlePostAction);   // Execute a configured action

    // ElegantOTA setup
    ElegantOTA.begin(&server, ota_username, ota_password); // Pass server, username, and password

    // Handle 404 Not Found for any other requests
    server.onNotFound([]() {
        server.send(404, "text/plain", "404: Not Found");
    });

    // Start the HTTP server
    server.begin();
    Serial.println("HTTP server started.");
    Serial.println("OTA updates available at http://" + WiFi.localIP().toString() + "/update");
}

void loop() {
    server.handleClient(); // Handle incoming client requests
    ElegantOTA.loop();     // Process ElegantOTA tasks
    delay(1);              // Small delay to yield to other tasks if any, can be adjusted
}