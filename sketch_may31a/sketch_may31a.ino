// -----------------------------------------------------------------------------------
// ESP32 Web Server for Home Control (Combined .ino file)
//
// This sketch provides a web interface to control GPIO pins, manage button
// configurations, view ESP32 status, and track history.
//
// Required Libraries (install via Arduino IDE Library Manager):
// 1. ESP Async WebServer
// 2. AsyncTCP (dependency of ESP Async WebServer)
// 3. ArduinoJson
// 4. NTPClient
//
// Also, install the "ESP32 LittleFS Filesystem Uploader" tool:
// https://randomnerdtutorials.com/esp32-web-server-spiffs-arduino/ (replace SPIFFS with LittleFS)
//
// Create a 'data' folder in your sketch directory and place your HTML, CSS,
// and JavaScript files (index.html, settings.html, status.html, history.html,
// style.css, script.js) inside it.
// -----------------------------------------------------------------------------------

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <NTPClient.h> // For accurate timestamps
#include <WiFiUdp.h>   // For NTPClient
#include <set>         // <--- ADD THIS LINE to include std::set
#include <algorithm>   // <--- AND THIS LINE for std::max (used in logHistory and saveHistory)

// --- WiFi Credentials (for initial setup, or use WiFiManager) ---
const char* ssid = "Airtel_Tanay";
const char* password = "Tanay@8089";

// --- Global Objects ---
AsyncWebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 5.5 * 3600); // NTP server, IST offset (5.5 hours)

// --- Data Structures ---
struct ButtonConfig {
    String id;      // Unique ID for the button (e.g., "button1", "myLight")
    String name;    // Display name
    int gpioPin;
    bool state;     // true for ON, false for OFF
};

// Global vector to store button configurations
std::vector<ButtonConfig> buttonConfigs;

// History entry structure
struct HistoryEntry {
    String timestamp;
    String event;
};
std::vector<HistoryEntry> historyLog; // In-memory history

// --- File Paths for LittleFS ---
const char* CONFIG_FILE = "/config.json";
const char* HISTORY_FILE = "/history.json";
const int MAX_HISTORY_ENTRIES = 50; // Limit history entries to prevent file bloat

// --- GPIO Management ---
// Array to keep track of used GPIOs. Initialize with all false.
// ESP32 has many GPIOs, but not all are freely usable. Adjust size and
// `initializeGPIOPins` based on your specific board and usable pins.
// Example for ESP32 DevKitC:
// GPIO 0, 2, 4, 12-19, 21-23, 25-27, 32-33, 34-39 (input only).
// Avoid GPIO 6-11 (for flash). GPIO 12 is a strapping pin; avoid for output.
bool gpioUsed[40]; // Assuming a max of 40 GPIOs, index 0-39

// --- Forward Declarations of Functions ---
// (Essential when function definitions appear after their calls, like in setup())
void handleRoot(AsyncWebServerRequest *request);
void handleNotFound(AsyncWebServerRequest *request);
void handleButtonsAPI(AsyncWebServerRequest *request);
void handleGPIOPinsAPI(AsyncWebServerRequest *request);
void handleSettingsAPI(AsyncWebServerRequest *request);
void handleStatusAPI(AsyncWebServerRequest *request);
void handleHistoryAPI(AsyncWebServerRequest *request);

void saveConfig();
void loadConfig();
void saveHistory();
void loadHistory();
String getTimestamp();
void logHistory(String event);

void initializeGPIOPins();
bool isGPIOPinAvailable(int pin);
void markGPIOPinUsed(int pin, bool used);

// -----------------------------------------------------------------------------------
// SETUP Function
// -----------------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    // Initialize LittleFS
    if (!LittleFS.begin()) {
        Serial.println("An Error has occurred while mounting LittleFS");
        return; // Halt setup if LittleFS fails
    }
    Serial.println("LittleFS mounted successfully.");

    // Initialize GPIO tracking array
    initializeGPIOPins();

    // Load saved configurations and history
    loadConfig();
    loadHistory();

    // Initialize GPIOs based on loaded config
    for (const auto& btn : buttonConfigs) {
        pinMode(btn.gpioPin, OUTPUT);
        digitalWrite(btn.gpioPin, btn.state ? HIGH : LOW);
        markGPIOPinUsed(btn.gpioPin, true); // Mark pin as used
    }

    // --- Connect to WiFi ---
    WiFi.mode(WIFI_STA);
    Serial.print("Connecting to WiFi ");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print('.');
    }
    Serial.println("\nWiFi Connected.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // --- Initialize NTP Client for accurate time ---
    timeClient.begin();
    timeClient.update(); // Initial time update
    Serial.print("NTP time synchronized: ");
    Serial.println(timeClient.getFormattedTime());

    // --- Web Server Routes ---

    // Serve static files from LittleFS (e.g., index.html, style.css, script.js)
    // The `serveStatic("/", LittleFS, "/").setDefaultFile("index.html");`
    // means that requests for "/" or anything not explicitly defined below will
    // look for a file in the LittleFS root, defaulting to "index.html".
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // API Endpoints
    server.on("/api/buttons", HTTP_GET, handleButtonsAPI);     // Get all button states (Home page)
    server.on("/api/buttons", HTTP_POST, handleButtonsAPI);    // Toggle button state (Home page)

    server.on("/api/settings/buttons", HTTP_GET, handleSettingsAPI);    // Get all button configs
    // server.on("/api/settings/buttons", HTTP_POST, handleSettingsAPI);   // Add new button

    server.on("/api/settings/buttons", HTTP_POST,
              [](AsyncWebServerRequest *request){
                  // This part is for processing headers before body, often not needed for simple JSON
                  // For plain JSON POSTs, the request body is delivered via the 'onBody' handler below.
                  // So, this lambda can remain empty or handle simple header checks if necessary.
              },
              NULL, // onUpload callback (used for multipart form data, not for plain JSON)
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                  // This is the onBody callback: it's called when body data is received.
                  // For small JSON payloads, 'len' will likely be equal to 'total', meaning the whole body arrived in one chunk.
                  if (index == 0 && len == total) { // Confirm this is the complete body, received in one chunk
                       String requestBody = String((char*)data).substring(0, len); // Convert raw data to String

                       Serial.println("\n--- DEBUG: POST /api/settings/buttons (onBody handler) ---");
                       Serial.print("Received raw body: ");
                       Serial.println(requestBody); // Print the raw JSON received

                       JsonDocument doc;
                       DeserializationError error = deserializeJson(doc, requestBody);

                       if (error) {
                           request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON or body too large\"}");
                           Serial.println("JSON parse error on add button (onBody): " + String(error.f_str()));
                           return;
                       }

                       // --- Start of original POST logic from handleSettingsAPI ---
                       String name = doc["name"].as<String>();
                       int gpioPin = doc["gpioPin"].as<int>();

                       if (name.length() == 0 || !isGPIOPinAvailable(gpioPin)) {
                           request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid name or GPIO pin not available/valid\"}");
                           Serial.println("Error: Invalid name or GPIO pin not available (onBody).");
                           return;
                       }

                       // Check for duplicate names (optional but good for UX)
                       for (const auto& btn : buttonConfigs) {
                           if (btn.name.equalsIgnoreCase(name)) { // Case-insensitive check
                               request->send(400, "application/json", "{\"success\":false,\"message\":\"Button with this name already exists\"}");
                               Serial.println("Error: Button with this name already exists (onBody).");
                               return;
                           }
                       }

                       ButtonConfig newButton;
                       newButton.id = "btn_" + String(millis()); // Simple unique ID
                       newButton.name = name;
                       newButton.gpioPin = gpioPin;
                       newButton.state = false; // Default off when created

                       buttonConfigs.push_back(newButton);
                       pinMode(newButton.gpioPin, OUTPUT);
                       digitalWrite(newButton.gpioPin, LOW); // Ensure off initially
                       markGPIOPinUsed(newButton.gpioPin, true); // Mark as used
                       saveConfig();
                       logHistory("New button '" + newButton.name + "' created on GPIO " + String(newButton.gpioPin));
                       request->send(200, "application/json", "{\"success\":true}");
                       Serial.println("Button added successfully (onBody).");
                       // --- End of original POST logic from handleSettingsAPI ---

                  }
                  // For chunked bodies (larger requests), you would concatenate 'data' to a String
                  // or buffer here. For typical JSON posts, it's usually one chunk.
              }
    );

    
    server.on("/api/settings/buttons", HTTP_DELETE, handleSettingsAPI); // Delete button

    server.on("/api/gpio/available", HTTP_GET, handleGPIOPinsAPI); // Get available GPIO pins

    server.on("/api/status", HTTP_GET, handleStatusAPI); // Get ESP32 status info

    server.on("/api/history", HTTP_GET, handleHistoryAPI); // Get event history

    // Fallback for unknown routes (404 Not Found)
    server.onNotFound(handleNotFound);

    // Start the server
    server.begin();
    Serial.println("HTTP server started.");
}

// -----------------------------------------------------------------------------------
// LOOP Function
// -----------------------------------------------------------------------------------
void loop() {
    // AsyncWebServer handles requests in the background.
    // Keep NTP time updated periodically.
    timeClient.update();
    // You can add other periodic tasks here (e.g., sensor readings, other logic)
}

// -----------------------------------------------------------------------------------
// Function Definitions (Grouped for clarity)
// -----------------------------------------------------------------------------------

// --- GPIO Management Functions ---
void initializeGPIOPins() {
    for (int i = 0; i < 40; i++) { // Max possible GPIOs on ESP32
        gpioUsed[i] = false;
    }

    // Mark pins that are generally not recommended for general I/O or are input-only
    // Adjust this list based on your specific ESP32 board and application.
    // Common pins to avoid/be aware of:
    // GPIO 6-11: Used for integrated SPI flash, generally unavailable for user.
    // GPIO 12 (MTDI): Strapping pin, can affect boot mode, use with caution for output.
    // GPIO 34-39: Input-only pins, no internal pull-up/down.
    gpioUsed[6] = gpioUsed[7] = gpioUsed[8] = gpioUsed[9] = gpioUsed[10] = gpioUsed[11] = true;
    // Add other pins you might use for internal purposes (e.g., if you have an SD card, I2C, etc.)
    // For example, if you use I2C on GPIO 21, 22:
    // gpioUsed[21] = true;
    // gpioUsed[22] = true;
}

bool isGPIOPinAvailable(int pin) {
    if (pin < 0 || pin >= 40) return false; // Out of bounds
    if (gpioUsed[pin]) return false; // Already marked as used or unavailable

    // Additional checks for specific ESP32 GPIO characteristics:
    if (pin >= 34 && pin <= 39) return false; // Input-only pins, cannot be outputs
    // Add more specific checks if needed (e.g., if a pin is broken on your board, etc.)

    return true;
}

void markGPIOPinUsed(int pin, bool used) {
    if (pin >= 0 && pin < 40) {
        gpioUsed[pin] = used;
    }
}

// --- Data Persistence (LittleFS) Functions ---
void saveConfig() {
    File configFile = LittleFS.open(CONFIG_FILE, "w");
    if (!configFile) {
        Serial.println("Failed to open config file for writing!");
        return;
    }

    JsonDocument doc;
    JsonArray buttonsArray = doc.to<JsonArray>();

    for (const auto& btn : buttonConfigs) {
        JsonObject buttonObj = buttonsArray.add<JsonObject>();
        buttonObj["id"] = btn.id;
        buttonObj["name"] = btn.name;
        buttonObj["gpioPin"] = btn.gpioPin;
        buttonObj["state"] = btn.state; // Save the current state
    }

    if (serializeJson(doc, configFile) == 0) {
        Serial.println("Failed to write JSON to config file!");
    } else {
        Serial.println("Configuration saved successfully.");
    }
    configFile.close();
}

void loadConfig() {
    File configFile = LittleFS.open(CONFIG_FILE, "r");
    if (!configFile) {
        Serial.println("Config file not found, creating a new one.");
        saveConfig(); // Create an empty config file if it doesn't exist
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, configFile);
    if (error) {
        Serial.print(F("Failed to read config file, using empty configuration: "));
        Serial.println(error.f_str());
        configFile.close();
        return;
    }

    buttonConfigs.clear(); // Clear any existing in-memory configs
    JsonArray buttonsArray = doc.as<JsonArray>();
    for (JsonObject buttonObj : buttonsArray) {
        ButtonConfig btn;
        btn.id = buttonObj["id"].as<String>();
        btn.name = buttonObj["name"].as<String>();
        btn.gpioPin = buttonObj["gpioPin"].as<int>();
        btn.state = buttonObj["state"].as<bool>();
        buttonConfigs.push_back(btn);
        // Mark these pins as used *after* loading them
        if (!isGPIOPinAvailable(btn.gpioPin)) {
             Serial.printf("Warning: Loaded button '%s' uses already taken/invalid GPIO %d.\n", btn.name.c_str(), btn.gpioPin);
        }
        markGPIOPinUsed(btn.gpioPin, true);
    }
    Serial.println("Configuration loaded successfully.");
    configFile.close();
}

void saveHistory() {
    File historyFile = LittleFS.open(HISTORY_FILE, "w");
    if (!historyFile) {
        Serial.println("Failed to open history file for writing!");
        return;
    }

    JsonDocument doc;
    JsonArray historyArray = doc.to<JsonArray>();

    // Only save a reasonable number of history entries (most recent)
    int startIndex = std::max(0, (int)historyLog.size() - MAX_HISTORY_ENTRIES);

    for (int i = startIndex; i < historyLog.size(); ++i) {
        JsonObject entryObj = historyArray.add<JsonObject>();
        entryObj["timestamp"] = historyLog[i].timestamp;
        entryObj["event"] = historyLog[i].event;
    }

    if (serializeJson(doc, historyFile) == 0) {
        Serial.println("Failed to write JSON to history file!");
    } else {
        // Serial.println("History saved."); // Commented out to reduce serial spam
    }
    historyFile.close();
}

void loadHistory() {
    File historyFile = LittleFS.open(HISTORY_FILE, "r");
    if (!historyFile) {
        Serial.println("History file not found, starting with empty history.");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, historyFile);
    if (error) {
        Serial.print(F("Failed to read history file, starting with empty history: "));
        Serial.println(error.f_str());
        historyFile.close();
        return;
    }

    historyLog.clear();
    JsonArray historyArray = doc.as<JsonArray>();
    for (JsonObject entryObj : historyArray) {
        HistoryEntry entry;
        entry.timestamp = entryObj["timestamp"].as<String>();
        entry.event = entryObj["event"].as<String>();
        historyLog.push_back(entry);
    }
    Serial.println("History loaded successfully.");
    historyFile.close();
}

String getTimestamp() {
    // Ensure NTP time is updated, though it should be handled in loop.
    timeClient.update();
    time_t rawTime = timeClient.getEpochTime();
    struct tm * ti;
    ti = localtime (&rawTime); // Convert epoch time to local time struct

    char timeStr[64];
    // Format: YYYY-MM-DD HH:MM:SS
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", ti);
    return String(timeStr);
}

void logHistory(String event) {
    HistoryEntry newEntry;
    newEntry.timestamp = getTimestamp();
    newEntry.event = event;
    historyLog.push_back(newEntry);
    // Keep history log within limits
    if (historyLog.size() > MAX_HISTORY_ENTRIES) {
        historyLog.erase(historyLog.begin()); // Remove oldest entry
    }
    saveHistory(); // Save history after each new entry (can be optimized to save less frequently for high-frequency events)
}

// --- API Handlers ---

void handleRoot(AsyncWebServerRequest *request) {
    // This function is generally not needed if using serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    // because the server will automatically serve index.html for the root path.
    // If you need specific dynamic content on the root, you would handle it here.
    request->send(LittleFS, "/index.html", "text/html");
}

void handleNotFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "404 Not Found");
}

void handleButtonsAPI(AsyncWebServerRequest *request) {
    if (request->method() == HTTP_GET) {
        JsonDocument doc;
        JsonArray buttonsArray = doc.to<JsonArray>();
        for (const auto& btn : buttonConfigs) {
            JsonObject buttonObj = buttonsArray.add<JsonObject>();
            buttonObj["id"] = btn.id;
            buttonObj["name"] = btn.name;
            buttonObj["gpioPin"] = btn.gpioPin;
            buttonObj["state"] = btn.state;
        }
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);

    } else if (request->method() == HTTP_POST) { // Toggle button state
        if (request->hasParam("plain", true)) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, request->arg("plain"));
            if (error) {
                request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
                Serial.println("JSON parse error on button toggle: " + String(error.f_str()));
                return;
            }

            String id = doc["id"].as<String>();
            bool newState = doc["state"].as<bool>();

            auto it = std::find_if(buttonConfigs.begin(), buttonConfigs.end(),
                                   [&](const ButtonConfig& b){ return b.id == id; });

            if (it != buttonConfigs.end()) {
                it->state = newState;
                digitalWrite(it->gpioPin, newState ? HIGH : LOW);
                saveConfig(); // Save updated state (includes current state)
                logHistory("Button '" + it->name + "' (GPIO " + String(it->gpioPin) + ") turned " + (newState ? "ON" : "OFF"));
                request->send(200, "application/json", "{\"success\":true}");
            } else {
                request->send(404, "application/json", "{\"success\":false,\"message\":\"Button not found\"}");
                Serial.println("Button not found for toggle: " + id);
            }
        } else {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"No data received\"}");
        }
    }
}

void handleGPIOPinsAPI(AsyncWebServerRequest *request) {
    JsonDocument doc;
    JsonArray pinsArray = doc.to<JsonArray>();

    // Get currently used pins from active buttons
    std::set<int> usedPinsByButtons;
    for (const auto& btn : buttonConfigs) {
        usedPinsByButtons.insert(btn.gpioPin);
    }

    // Iterate through all possible GPIOs and add available ones
    for (int i = 0; i < 40; i++) {
        // A pin is "available" if:
        // 1. It's generally usable (not reserved/input-only by `initializeGPIOPins` & `isGPIOPinAvailable`)
        // 2. It's not currently assigned to another *active* button.
        // When editing a button, the frontend should handle keeping the current button's GPIO
        // in the dropdown even if it's "used" by that specific button.
        if (isGPIOPinAvailable(i) && usedPinsByButtons.find(i) == usedPinsByButtons.end()) {
            pinsArray.add(i);
        }
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}


void handleSettingsAPI(AsyncWebServerRequest *request) {
    if (request->method() == HTTP_GET) {
        JsonDocument doc;
        JsonArray buttonsArray = doc.to<JsonArray>();
        for (const auto& btn : buttonConfigs) {
            JsonObject buttonObj = buttonsArray.add<JsonObject>();
            buttonObj["id"] = btn.id;
            buttonObj["name"] = btn.name;
            buttonObj["gpioPin"] = btn.gpioPin;
            buttonObj["state"] = btn.state;
        }
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);

    } else if (request->method() == HTTP_POST) { // Add new button
        Serial.println("\n--- DEBUG: POST /api/settings/buttons ---");
        Serial.print("Received POST request to /api/settings/buttons. Content-Type: ");
        Serial.println(request->contentType()); // Print the received Content-Type header

        // Iterate through parameters to see what AsyncWebServer detected
        Serial.println("--- Request Parameters ---");
        int params = request->params();
        for(int i=0; i<params; i++){
            const AsyncWebParameter* p = request->getParam(i);
            if(p->isPost()){ // Check if it's a POST parameter (could be body or form field)
                Serial.print("  Param Name: ");
                Serial.println(p->name());
                Serial.print("  Param Value: ");
                Serial.println(p->value());
                Serial.print("  Param Length: ");
                Serial.println(p->value().length());
            }
        }
        Serial.println("--- End Request Parameters ---");

        Serial.print("Check hasParam(\"plain\", true): ");
        Serial.println(request->hasParam("plain", true) ? "true" : "false");
        Serial.print("Length of arg(\"plain\"): ");
        Serial.println(request->arg("plain").length());
        Serial.print("Content of arg(\"plain\"): ");
        Serial.println(request->arg("plain"));


        if (request->hasParam("plain", true)) {
            // Original JSON parsing and button add logic
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, request->arg("plain"));
            if (error) {
                request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON or body too large\"}");
                Serial.println("JSON parse error on add button: " + String(error.f_str()));
                return;
            }

            String name = doc["name"].as<String>();
            int gpioPin = doc["gpioPin"].as<int>();

            if (name.length() == 0 || !isGPIOPinAvailable(gpioPin)) {
                request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid name or GPIO pin not available/valid\"}");
                Serial.println("Error: Invalid name or GPIO pin not available.");
                return;
            }

            // Check for duplicate names (optional but good for UX)
            for (const auto& btn : buttonConfigs) {
                if (btn.name.equalsIgnoreCase(name)) { // Case-insensitive check
                    request->send(400, "application/json", "{\"success\":false,\"message\":\"Button with this name already exists\"}");
                    Serial.println("Error: Button with this name already exists.");
                    return;
                }
            }

            ButtonConfig newButton;
            newButton.id = "btn_" + String(millis()); // Simple unique ID
            newButton.name = name;
            newButton.gpioPin = gpioPin;
            newButton.state = false; // Default off when created

            buttonConfigs.push_back(newButton);
            pinMode(newButton.gpioPin, OUTPUT);
            digitalWrite(newButton.gpioPin, LOW); // Ensure off initially
            markGPIOPinUsed(newButton.gpioPin, true); // Mark as used
            saveConfig();
            logHistory("New button '" + newButton.name + "' created on GPIO " + String(newButton.gpioPin));
            request->send(200, "application/json", "{\"success\":true}");
            Serial.println("Button added successfully.");
        } else {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"No data received. Debug: Plain param missing or empty.\"");
            Serial.println("Error: No 'plain' data received in POST request to /api/settings/buttons.");
            return;
        }

    } else if (request->method() == HTTP_PUT) { // Edit existing button
        if (request->hasParam("plain", true)) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, request->arg("plain"));
            if (error) {
                request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
                Serial.println("JSON parse error on edit button: " + String(error.f_str()));
                return;
            }

            String id = doc["id"].as<String>();
            String newName = doc["name"].as<String>();
            int newGpioPin = doc["gpioPin"].as<int>();

            if (newName.length() == 0) {
                 request->send(400, "application/json", "{\"success\":false,\"message\":\"Button name cannot be empty\"}");
                 return;
            }

            auto it = std::find_if(buttonConfigs.begin(), buttonConfigs.end(),
                                   [&](const ButtonConfig& b){ return b.id == id; });

            if (it != buttonConfigs.end()) {
                // Check for duplicate names for other buttons (if name changed)
                if (!it->name.equalsIgnoreCase(newName)) {
                    for (const auto& btn : buttonConfigs) {
                        if (btn.id != id && btn.name.equalsIgnoreCase(newName)) {
                            request->send(400, "application/json", "{\"success\":false,\"message\":\"Another button with this name already exists\"}");
                            return;
                        }
                    }
                }

                // If GPIO pin is changing
                if (it->gpioPin != newGpioPin) {
                    // Check if new pin is available (and not the current one, which is 'occupied' by this button)
                    if (!isGPIOPinAvailable(newGpioPin) && newGpioPin != it->gpioPin) {
                        request->send(400, "application/json", "{\"success\":false,\"message\":\"New GPIO pin is not available/valid\"}");
                        return;
                    }
                    // Free up old pin and mark new one as used
                    digitalWrite(it->gpioPin, LOW); // Turn off old GPIO before reassigning
                    markGPIOPinUsed(it->gpioPin, false); // Free up old pin
                    markGPIOPinUsed(newGpioPin, true);   // Mark new pin as used
                    pinMode(newGpioPin, OUTPUT);         // Configure new pin
                    digitalWrite(newGpioPin, it->state ? HIGH : LOW); // Set state of new pin
                }

                it->name = newName;
                it->gpioPin = newGpioPin;
                saveConfig();
                logHistory("Button '" + it->name + "' (ID: " + id + ") updated. GPIO changed to " + String(newGpioPin) + ".");
                request->send(200, "application/json", "{\"success\":true}");
            } else {
                request->send(404, "application/json", "{\"success\":false,\"message\":\"Button not found\"}");
            }
        } else {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"No data received\"}");
        }

    } else if (request->method() == HTTP_DELETE) { // Delete button
        if (request->hasParam("plain", true)) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, request->arg("plain"));
            if (error) {
                request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
                Serial.println("JSON parse error on delete button: " + String(error.f_str()));
                return;
            }

            String id = doc["id"].as<String>();
            auto it = std::find_if(buttonConfigs.begin(), buttonConfigs.end(),
                                   [&](const ButtonConfig& b){ return b.id == id; });

            if (it != buttonConfigs.end()) {
                ButtonConfig buttonToDelete = *it; // Store before erasing
                digitalWrite(buttonToDelete.gpioPin, LOW); // Turn off GPIO before deleting
                markGPIOPinUsed(buttonToDelete.gpioPin, false); // Free up GPIO
                buttonConfigs.erase(it);
                saveConfig();
                logHistory("Button '" + buttonToDelete.name + "' (GPIO " + String(buttonToDelete.gpioPin) + ") deleted.");
                request->send(200, "application/json", "{\"success\":true}");
            } else {
                request->send(404, "application/json", "{\"success\":false,\"message\":\"Button not found\"}");
            }
        } else {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"No data received\"}");
        }
    }
}

void handleStatusAPI(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["uptime"] = millis(); // Milliseconds since boot
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["chipId"] = String(ESP.getEfuseMac(), HEX); // ESP32 MAC address (unique chip ID)
    doc["flashSize"] = ESP.getFlashChipSize();
    doc["cpuFreq"] = ESP.getCpuFreqMHz();
    doc["sdkVersion"] = ESP.getSdkVersion();
    doc["rssi"] = WiFi.RSSI();
    doc["localIP"] = WiFi.localIP().toString();
    doc["ssid"] = WiFi.SSID();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void handleHistoryAPI(AsyncWebServerRequest *request) {
    JsonDocument doc;
    JsonArray historyArray = doc.to<JsonArray>();

    for (const auto& entry : historyLog) {
        JsonObject entryObj = historyArray.add<JsonObject>();
        entryObj["timestamp"] = entry.timestamp;
        entryObj["event"] = entry.event;
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}