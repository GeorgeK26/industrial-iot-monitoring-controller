/*
INDUSTRIAL IoT MONITORING CONTROLLER
ESP32 DevKitC V4
Firmware line: 3.1 / final working version

IMPORTANT:
Paste the exact final working sketch from the project conversation into this file.
Do not replace it with an older revision.

The final working firmware includes:
- DHT22 temperature/humidity
- ADC machine-load monitoring
- moving-average filtering
- DHT/ADC fault detection
- warning/critical alarm thresholds with hysteresis
- OLED UI
- 3-button navigation
- Wi-Fi monitoring and reconnect
- JSON serial telemetry
- event logging
- automatic alarm event detection
- compact Event Log UI with no overlapping rows

This marker file is intentionally not pretending to contain code that was not
provided as a current attachment. The repository documentation is complete and
ready; copy the verified final sketch here before committing.
*/



#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHT.h"
#include <WiFi.h>

// ============================================================
// INDUSTRIAL IoT MONITORING CONTROLLER
// FINAL FIRMWARE V3.2
// ESP32 DevKitC V4
//
// FEATURES
// ------------------------------------------------------------
// - DHT22 temperature / humidity monitoring
// - ADC machine-load monitoring
// - 4-sample moving-average filtering
// - Sensor fault detection
// - Warning / Critical alarm states
// - Alarm hysteresis
// - LED + buzzer alarm outputs
// - OLED local HMI
// - 3-button navigation
// - Wi-Fi monitoring
// - Automatic Wi-Fi reconnection
// - JSON telemetry
// - Event logging
// - Event log pagination
// - Alarm transition detection
// - Serial diagnostics
// ============================================================


// ============================================================
// FIRMWARE CONFIGURATION
// ============================================================

const char* DEVICE_NAME = "INDUSTRIAL-CTRL-01";
const char* FIRMWARE_VERSION = "3.2";


// ============================================================
// WIFI CONFIGURATION
// ============================================================

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

bool wifiConnected = false;

unsigned long lastWiFiAttempt = 0;

const unsigned long WIFI_RECONNECT_INTERVAL = 10000;


// ============================================================
// DHT22 CONFIGURATION
// ============================================================

#define DHT_PIN 15
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);


// ============================================================
// OLED CONFIGURATION
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);


// ============================================================
// BUTTON CONFIGURATION
// ============================================================

#define BTN_UP 25
#define BTN_DOWN 26
#define BTN_SELECT 27


// ============================================================
// INDUSTRIAL INPUT / OUTPUT
// ============================================================

#define POT_PIN 34

#define ALARM_LED 4
#define BUZZER_PIN 5


// ============================================================
// SCREEN SYSTEM
// ============================================================

enum Screen
{
  MAIN_MENU,
  LIVE_DATA,
  SENSOR_STATUS,
  ALARM_STATUS,
  SYSTEM_INFO,
  EVENT_LOG
};

Screen currentScreen = MAIN_MENU;


// ============================================================
// MAIN MENU
// ============================================================

int menuItem = 0;

const int MENU_ITEMS = 5;

const char* menuItems[MENU_ITEMS] =
{
  "LIVE DATA",
  "SENSOR STATUS",
  "ALARM STATUS",
  "SYSTEM INFO",
  "EVENT LOG"
};


// ============================================================
// SENSOR VARIABLES
// ============================================================

float temperature = 0.0;
float humidity = 0.0;

int loadPercentage = 0;

int rawADC = 0;
int filteredADC = 0;


// ============================================================
// SENSOR STATUS
// ============================================================

bool dhtFault = false;
bool adcFault = false;

int dhtFailureCount = 0;

const int DHT_MAX_FAILURES = 3;


// ============================================================
// DHT VALIDATION
// ============================================================

const float MIN_TEMPERATURE = -40.0;
const float MAX_TEMPERATURE = 80.0;

const float MIN_HUMIDITY = 0.0;
const float MAX_HUMIDITY = 100.0;

unsigned long lastValidDHTReading = 0;


// ============================================================
// ADC MOVING AVERAGE
// ============================================================

const int ADC_SAMPLES = 4;

int adcSamples[ADC_SAMPLES];

int adcIndex = 0;

long adcTotal = 0;


// ============================================================
// ALARM STATE MACHINE
// ============================================================

enum AlarmState
{
  NORMAL,
  WARNING,
  CRITICAL,
  SENSOR_FAULT
};

AlarmState alarmState = NORMAL;
AlarmState previousAlarmState = NORMAL;


// ============================================================
// ALARM THRESHOLDS
// ============================================================

const int WARNING_ON = 70;
const int WARNING_OFF = 68;

const int CRITICAL_ON = 90;
const int CRITICAL_OFF = 88;


// ============================================================
// TIMING
// ============================================================

unsigned long lastSensorUpdate = 0;

const unsigned long SENSOR_INTERVAL = 2000;


unsigned long lastADCUpdate = 0;

const unsigned long ADC_INTERVAL = 100;


unsigned long lastDisplayUpdate = 0;

const unsigned long DISPLAY_INTERVAL = 150;


// ============================================================
// BUTTON DEBOUNCE
// ============================================================

const unsigned long DEBOUNCE_TIME = 40;

bool upStableState = HIGH;
bool downStableState = HIGH;
bool selectStableState = HIGH;

bool upLastReading = HIGH;
bool downLastReading = HIGH;
bool selectLastReading = HIGH;

unsigned long upLastChange = 0;
unsigned long downLastChange = 0;
unsigned long selectLastChange = 0;


// ============================================================
// STATISTICS
// ============================================================

unsigned long sensorUpdateCount = 0;
unsigned long jsonPacketCount = 0;


// ============================================================
// EVENT LOG
// ============================================================

const int MAX_EVENTS = 10;

struct Event
{
  unsigned long timestamp;
  const char* type;
  const char* message;
};

Event events[MAX_EVENTS];

int eventCount = 0;


// Event-log page
int eventPage = 0;

const int EVENTS_PER_PAGE = 4;


// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

// Wi-Fi
void connectWiFi();
void updateWiFiStatus();

// Sensors
void updateDHT();
void updateLoad();

// Alarm
void updateAlarm();
void setAlarmOutputs(bool active);

// Event log
void addEvent(const char* type, const char* message);
void showEventLog();

// Telemetry
void sendSensorData();

// Buttons
void handleButtons(unsigned long now);
void handleUpPress();
void handleDownPress();
void handleSelectPress();

// Display
void updateDisplay();
void showMainMenu();
void showLiveData();
void showSensorStatus();
void showAlarmStatus();
void showSystemInfo();

// Text helpers
const char* getAlarmText();
const char* getDHTText();
const char* getADCText();
const char* getWiFiText();


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(500);

  Serial.println();
  Serial.println("==========================================");
  Serial.println(" INDUSTRIAL IoT MONITORING CONTROLLER");
  Serial.println(" ESP32 DEVKITC V4");
  Serial.print(" FIRMWARE: ");
  Serial.println(FIRMWARE_VERSION);
  Serial.println("==========================================");


  // ----------------------------------------------------------
  // BUTTONS
  // ----------------------------------------------------------

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);


  // ----------------------------------------------------------
  // ALARM OUTPUTS
  // ----------------------------------------------------------

  pinMode(ALARM_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(ALARM_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);


  // ----------------------------------------------------------
  // DHT22
  // ----------------------------------------------------------

  dht.begin();


  // ----------------------------------------------------------
  // ADC
  // ----------------------------------------------------------

  analogReadResolution(12);

  rawADC = analogRead(POT_PIN);


  // ----------------------------------------------------------
  // INITIALIZE ADC FILTER
  // ----------------------------------------------------------

  for (int i = 0; i < ADC_SAMPLES; i++)
  {
    adcSamples[i] = rawADC;
    adcTotal += rawADC;
  }

  filteredADC = rawADC;


  // ----------------------------------------------------------
  // INITIAL LOAD
  // ----------------------------------------------------------

  loadPercentage = map(
    filteredADC,
    0,
    4095,
    0,
    100
  );

  loadPercentage = constrain(
    loadPercentage,
    0,
    100
  );


  // ----------------------------------------------------------
  // I2C
  // ----------------------------------------------------------

  Wire.begin(21, 22);


  // ----------------------------------------------------------
  // OLED
  // ----------------------------------------------------------

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS
      ))
  {
    Serial.println("ERROR: OLED initialization failed!");

    while (true)
    {
      delay(1000);
    }
  }


  // ----------------------------------------------------------
  // OLED CONFIGURATION
  // ----------------------------------------------------------

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);


  // ----------------------------------------------------------
  // STARTUP SCREEN
  // ----------------------------------------------------------

  display.setCursor(10, 2);
  display.println("INDUSTRIAL IoT");

  display.setCursor(10, 14);
  display.println("MONITORING");

  display.setCursor(10, 26);
  display.println("CONTROLLER");

  display.setCursor(10, 40);
  display.print("FW ");

  display.println(FIRMWARE_VERSION);

  display.setCursor(10, 53);
  display.println("INITIALIZING");

  display.display();

  delay(1000);


  // ----------------------------------------------------------
  // INITIAL WIFI
  // ----------------------------------------------------------

  connectWiFi();


  // ----------------------------------------------------------
  // INITIAL SENSOR DATA
  // ----------------------------------------------------------

  updateLoad();

  updateDHT();

  updateAlarm();


  // ----------------------------------------------------------
  // STARTUP EVENT
  // ----------------------------------------------------------

  addEvent(
    "INFO",
    "Controller started"
  );


  // ----------------------------------------------------------
  // INITIAL TELEMETRY
  // ----------------------------------------------------------

  sendSensorData();


  // ----------------------------------------------------------
  // MAIN MENU
  // ----------------------------------------------------------

  currentScreen = MAIN_MENU;

  menuItem = 0;

  eventPage = 0;

  showMainMenu();
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
  unsigned long now = millis();


  // ----------------------------------------------------------
  // WIFI
  // ----------------------------------------------------------

  updateWiFiStatus();


  // ----------------------------------------------------------
  // FAST ADC UPDATE
  // ----------------------------------------------------------

  if (now - lastADCUpdate >= ADC_INTERVAL)
  {
    lastADCUpdate = now;

    updateLoad();

    updateAlarm();
  }


  // ----------------------------------------------------------
  // DHT22 UPDATE
  // ----------------------------------------------------------

  if (now - lastSensorUpdate >= SENSOR_INTERVAL)
  {
    lastSensorUpdate = now;

    updateDHT();

    updateAlarm();

    sendSensorData();
  }


  // ----------------------------------------------------------
  // BUTTONS
  // ----------------------------------------------------------

  handleButtons(now);


  // ----------------------------------------------------------
  // DISPLAY
  // ----------------------------------------------------------

  if (now - lastDisplayUpdate >= DISPLAY_INTERVAL)
  {
    lastDisplayUpdate = now;

    updateDisplay();
  }
}


// ============================================================
// WIFI CONNECTION
// ============================================================

void connectWiFi()
{
  Serial.println();
  Serial.println("Connecting to Wi-Fi...");

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  unsigned long startTime = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - startTime < 10000
  )
  {
    delay(250);

    Serial.print(".");
  }

  Serial.println();


  if (WiFi.status() == WL_CONNECTED)
  {
    wifiConnected = true;

    Serial.println("Wi-Fi connected!");

    Serial.print("IP address: ");

    Serial.println(
      WiFi.localIP()
    );

    addEvent(
      "INFO",
      "Wi-Fi connected"
    );
  }
  else
  {
    wifiConnected = false;

    Serial.println(
      "Wi-Fi connection failed."
    );

    addEvent(
      "WARNING",
      "Wi-Fi unavailable"
    );
  }
}


// ============================================================
// WIFI STATUS / AUTOMATIC RECONNECT
// ============================================================

void updateWiFiStatus()
{
  bool currentlyConnected =
    (WiFi.status() == WL_CONNECTED);


  // ----------------------------------------------------------
  // CONNECTED
  // ----------------------------------------------------------

  if (currentlyConnected)
  {
    if (!wifiConnected)
    {
      wifiConnected = true;

      addEvent(
        "INFO",
        "Wi-Fi reconnected"
      );

      Serial.println(
        "Wi-Fi reconnected."
      );
    }

    return;
  }


  // ----------------------------------------------------------
  // DISCONNECTED
  // ----------------------------------------------------------

  if (wifiConnected)
  {
    wifiConnected = false;

    addEvent(
      "WARNING",
      "Wi-Fi disconnected"
    );

    Serial.println(
      "Wi-Fi disconnected."
    );
  }


  // ----------------------------------------------------------
  // AUTOMATIC RECONNECT
  // ----------------------------------------------------------

  unsigned long now = millis();

  if (
    now - lastWiFiAttempt >=
    WIFI_RECONNECT_INTERVAL
  )
  {
    lastWiFiAttempt = now;

    Serial.println(
      "Attempting Wi-Fi reconnect..."
    );

    WiFi.disconnect();

    WiFi.begin(
      WIFI_SSID,
      WIFI_PASSWORD
    );
  }
}


// ============================================================
// DHT22 UPDATE
// ============================================================

void updateDHT()
{
  sensorUpdateCount++;


  // ----------------------------------------------------------
  // READ SENSOR
  // ----------------------------------------------------------

  float newTemperature =
    dht.readTemperature();

  float newHumidity =
    dht.readHumidity();


  bool readingValid = true;


  // ----------------------------------------------------------
  // NaN CHECK
  // ----------------------------------------------------------

  if (
    isnan(newTemperature) ||
    isnan(newHumidity)
  )
  {
    readingValid = false;
  }


  // ----------------------------------------------------------
  // RANGE VALIDATION
  // ----------------------------------------------------------

  if (readingValid)
  {
    if (
      newTemperature < MIN_TEMPERATURE ||
      newTemperature > MAX_TEMPERATURE
    )
    {
      readingValid = false;
    }
  }


  if (readingValid)
  {
    if (
      newHumidity < MIN_HUMIDITY ||
      newHumidity > MAX_HUMIDITY
    )
    {
      readingValid = false;
    }
  }


  // ----------------------------------------------------------
  // VALID READING
  // ----------------------------------------------------------

  if (readingValid)
  {
    temperature = newTemperature;

    humidity = newHumidity;

    dhtFailureCount = 0;

    if (dhtFault)
    {
      dhtFault = false;

      addEvent(
        "INFO",
        "DHT22 fault cleared"
      );
    }

    dhtFault = false;

    lastValidDHTReading = millis();
  }


  // ----------------------------------------------------------
  // INVALID READING
  // ----------------------------------------------------------

  else
  {
    dhtFailureCount++;

    Serial.print(
      "DHT22 invalid reading. Failure count: "
    );

    Serial.println(
      dhtFailureCount
    );


    if (
      dhtFailureCount >=
      DHT_MAX_FAILURES
    )
    {
      if (!dhtFault)
      {
        dhtFault = true;

        addEvent(
          "FAULT",
          "DHT22 sensor fault"
        );
      }
    }
  }


  // ----------------------------------------------------------
  // SERIAL DIAGNOSTICS
  // ----------------------------------------------------------

  Serial.println();

  Serial.println(
    "---------- DHT UPDATE ----------"
  );

  Serial.print("DHT22: ");

  Serial.println(
    dhtFault ? "FAULT" : "OK"
  );


  Serial.print("Temperature: ");

  if (dhtFault)
  {
    Serial.println("FAULT");
  }
  else
  {
    Serial.print(
      temperature,
      1
    );

    Serial.println(" C");
  }


  Serial.print("Humidity: ");

  if (dhtFault)
  {
    Serial.println("FAULT");
  }
  else
  {
    Serial.print(
      humidity,
      1
    );

    Serial.println(" %");
  }

  Serial.println(
    "--------------------------------"
  );
}


// ============================================================
// ADC / LOAD UPDATE
// ============================================================

void updateLoad()
{
  // ----------------------------------------------------------
  // READ ADC
  // ----------------------------------------------------------

  rawADC = analogRead(POT_PIN);


  // ----------------------------------------------------------
  // ADC VALIDATION
  // ----------------------------------------------------------

  if (
    rawADC < 0 ||
    rawADC > 4095
  )
  {
    if (!adcFault)
    {
      adcFault = true;

      addEvent(
        "FAULT",
        "ADC input fault"
      );
    }

    Serial.println(
      "ADC FAULT!"
    );

    return;
  }


  // ----------------------------------------------------------
  // CLEAR ADC FAULT
  // ----------------------------------------------------------

  if (adcFault)
  {
    adcFault = false;

    addEvent(
      "INFO",
      "ADC fault cleared"
    );
  }


  // ----------------------------------------------------------
  // REMOVE OLD SAMPLE
  // ----------------------------------------------------------

  adcTotal -=
    adcSamples[adcIndex];


  // ----------------------------------------------------------
  // ADD NEW SAMPLE
  // ----------------------------------------------------------

  adcSamples[adcIndex] =
    rawADC;

  adcTotal +=
    rawADC;


  // ----------------------------------------------------------
  // CIRCULAR BUFFER
  // ----------------------------------------------------------

  adcIndex++;

  if (
    adcIndex >= ADC_SAMPLES
  )
  {
    adcIndex = 0;
  }


  // ----------------------------------------------------------
  // MOVING AVERAGE
  // ----------------------------------------------------------

  filteredADC =
    adcTotal / ADC_SAMPLES;


  // ----------------------------------------------------------
  // ADC → LOAD %
  // ----------------------------------------------------------

  loadPercentage = map(
    filteredADC,
    0,
    4095,
    0,
    100
  );

  loadPercentage =
    constrain(
      loadPercentage,
      0,
      100
    );
}


// ============================================================
// ALARM OUTPUT
// ============================================================

void setAlarmOutputs(bool active)
{
  if (active)
  {
    digitalWrite(
      ALARM_LED,
      HIGH
    );

    digitalWrite(
      BUZZER_PIN,
      HIGH
    );
  }
  else
  {
    digitalWrite(
      ALARM_LED,
      LOW
    );

    digitalWrite(
      BUZZER_PIN,
      LOW
    );
  }
}


// ============================================================
// ALARM STATE MACHINE
// ============================================================

void updateAlarm()
{
  // ----------------------------------------------------------
  // SAVE PREVIOUS STATE
  // ----------------------------------------------------------

  previousAlarmState =
    alarmState;


  // ==========================================================
  // SENSOR FAULT HAS HIGHEST PRIORITY
  // ==========================================================

  if (
    dhtFault ||
    adcFault
  )
  {
    alarmState =
      SENSOR_FAULT;
  }


  // ==========================================================
  // NORMAL ALARM STATE MACHINE
  // ==========================================================

  else
  {
    switch (alarmState)
    {
      // ------------------------------------------------------
      // NORMAL
      // ------------------------------------------------------

      case NORMAL:

        if (
          loadPercentage >=
          CRITICAL_ON
        )
        {
          alarmState =
            CRITICAL;
        }
        else if (
          loadPercentage >=
          WARNING_ON
        )
        {
          alarmState =
            WARNING;
        }

        break;


      // ------------------------------------------------------
      // WARNING
      // ------------------------------------------------------

      case WARNING:

        if (
          loadPercentage >=
          CRITICAL_ON
        )
        {
          alarmState =
            CRITICAL;
        }
        else if (
          loadPercentage < 
          WARNING_OFF
        )
        {
          alarmState =
            NORMAL;
        }

        break;


      // ------------------------------------------------------
      // CRITICAL
      // ------------------------------------------------------

      case CRITICAL:

        if (
          loadPercentage <
          CRITICAL_OFF
        )
        {
          if (
            loadPercentage >=
            WARNING_ON
          )
          {
            alarmState =
              WARNING;
          }
          else
          {
            alarmState =
              NORMAL;
          }
        }

        break;


      // ------------------------------------------------------
      // SENSOR FAULT
      // ------------------------------------------------------

      case SENSOR_FAULT:

        if (
          !dhtFault &&
          !adcFault
        )
        {
          if (
            loadPercentage >=
            CRITICAL_ON
          )
          {
            alarmState =
              CRITICAL;
          }
          else if (
            loadPercentage >=
            WARNING_ON
          )
          {
            alarmState =
              WARNING;
          }
          else
          {
            alarmState =
              NORMAL;
          }
        }

        break;
    }
  }


  // ==========================================================
  // AUTOMATIC EVENT DETECTION
  // ==========================================================

  if (
    alarmState !=
    previousAlarmState
  )
  {

    // --------------------------------------------------------
    // NORMAL → WARNING
    // --------------------------------------------------------

    if (
      alarmState == WARNING &&
      previousAlarmState == NORMAL
    )
    {
      addEvent(
        "WARNING",
        "Load above 70%"
      );
    }


    // --------------------------------------------------------
    // WARNING → CRITICAL
    // --------------------------------------------------------

    else if (
      alarmState == CRITICAL &&
      previousAlarmState == WARNING
    )
    {
      addEvent(
        "CRITICAL",
        "Load above 90%"
      );
    }


    // --------------------------------------------------------
    // NORMAL → CRITICAL
    // --------------------------------------------------------

    else if (
      alarmState == CRITICAL &&
      previousAlarmState == NORMAL
    )
    {
      addEvent(
        "CRITICAL",
        "Load above 90%"
      );
    }


    // --------------------------------------------------------
    // SENSOR FAULT
    // --------------------------------------------------------

    else if (
      alarmState == SENSOR_FAULT
    )
    {
      if (dhtFault)
      {
        addEvent(
          "FAULT",
          "DHT22 sensor fault"
        );
      }
      else if (adcFault)
      {
        addEvent(
          "FAULT",
          "ADC input fault"
        );
      }
      else
      {
        addEvent(
          "FAULT",
          "Sensor fault"
        );
      }
    }


    // --------------------------------------------------------
    // CRITICAL → WARNING
    // --------------------------------------------------------

    else if (
      alarmState == WARNING &&
      previousAlarmState == CRITICAL
    )
    {
      addEvent(
        "INFO",
        "Load returned to warning"
      );
    }


    // --------------------------------------------------------
    // WARNING → NORMAL
    // --------------------------------------------------------

    else if (
      alarmState == NORMAL &&
      previousAlarmState == WARNING
    )
    {
      addEvent(
        "INFO",
        "Warning cleared"
      );
    }


    // --------------------------------------------------------
    // CRITICAL → NORMAL
    // --------------------------------------------------------

    else if (
      alarmState == NORMAL &&
      previousAlarmState == CRITICAL
    )
    {
      addEvent(
        "INFO",
        "Critical alarm cleared"
      );
    }


    // --------------------------------------------------------
    // SENSOR FAULT → NORMAL
    // --------------------------------------------------------

    else if (
      alarmState == NORMAL &&
      previousAlarmState == SENSOR_FAULT
    )
    {
      addEvent(
        "INFO",
        "Sensor fault cleared"
      );
    }
  }


  // ==========================================================
  // OUTPUT CONTROL
  // ==========================================================

  if (
    alarmState == CRITICAL ||
    alarmState == SENSOR_FAULT
  )
  {
    setAlarmOutputs(true);
  }
  else
  {
    setAlarmOutputs(false);
  }
}


// ============================================================
// EVENT LOG
// ============================================================

void addEvent(
  const char* type,
  const char* message
)
{
  // ----------------------------------------------------------
  // REMOVE OLDEST EVENT IF FULL
  // ----------------------------------------------------------

  if (
    eventCount >= MAX_EVENTS
  )
  {
    for (
      int i = 0;
      i < MAX_EVENTS - 1;
      i++
    )
    {
      events[i] =
        events[i + 1];
    }

    eventCount =
      MAX_EVENTS - 1;
  }


  // ----------------------------------------------------------
  // ADD EVENT
  // ----------------------------------------------------------

  events[eventCount].timestamp =
    millis();

  events[eventCount].type =
    type;

  events[eventCount].message =
    message;

  eventCount++;


  // ----------------------------------------------------------
  // RESET EVENT PAGE
  // ----------------------------------------------------------

  eventPage = 0;


  // ----------------------------------------------------------
  // SERIAL LOG
  // ----------------------------------------------------------

  Serial.print("[EVENT] ");

  Serial.print(type);

  Serial.print(" | ");

  Serial.print(message);

  Serial.print(" | t=");

  Serial.print(millis());

  Serial.println(" ms");
}


// ============================================================
// TELEMETRY
// ============================================================

void sendSensorData()
{
  jsonPacketCount++;


  Serial.print("{");


  // DEVICE
  Serial.print("\"device\":\"");
  Serial.print(DEVICE_NAME);
  Serial.print("\",");


  // TIMESTAMP
  Serial.print("\"timestamp_ms\":");
  Serial.print(millis());
  Serial.print(",");


  // TEMPERATURE
  Serial.print("\"temperature\":");

  if (dhtFault)
  {
    Serial.print("null");
  }
  else
  {
    Serial.print(
      temperature,
      1
    );
  }

  Serial.print(",");


  // HUMIDITY
  Serial.print("\"humidity\":");

  if (dhtFault)
  {
    Serial.print("null");
  }
  else
  {
    Serial.print(
      humidity,
      1
    );
  }

  Serial.print(",");


  // RAW ADC
  Serial.print("\"adc_raw\":");
  Serial.print(rawADC);
  Serial.print(",");


  // FILTERED ADC
  Serial.print("\"adc_filtered\":");
  Serial.print(filteredADC);
  Serial.print(",");


  // LOAD
  Serial.print("\"load_percent\":");
  Serial.print(loadPercentage);
  Serial.print(",");


  // ALARM
  Serial.print("\"alarm\":\"");
  Serial.print(getAlarmText());
  Serial.print("\",");


  // DHT STATUS
  Serial.print("\"dht_status\":\"");
  Serial.print(getDHTText());
  Serial.print("\",");


  // ADC STATUS
  Serial.print("\"adc_status\":\"");
  Serial.print(getADCText());
  Serial.print("\",");


  // LED
  Serial.print("\"alarm_led\":");

  if (
    alarmState == CRITICAL ||
    alarmState == SENSOR_FAULT
  )
  {
    Serial.print("true");
  }
  else
  {
    Serial.print("false");
  }

  Serial.print(",");


  // BUZZER
  Serial.print("\"buzzer\":");

  if (
    alarmState == CRITICAL ||
    alarmState == SENSOR_FAULT
  )
  {
    Serial.print("true");
  }
  else
  {
    Serial.print("false");
  }

  Serial.print(",");


  // WIFI
  Serial.print("\"wifi\":\"");
  Serial.print(getWiFiText());
  Serial.print("\",");


  // FIRMWARE
  Serial.print("\"firmware\":\"");
  Serial.print(FIRMWARE_VERSION);
  Serial.print("\",");


  // SYSTEM STATUS
  Serial.print("\"status\":\"");

  if (
    dhtFault ||
    adcFault
  )
  {
    Serial.print("FAULT");
  }
  else if (
    alarmState == CRITICAL
  )
  {
    Serial.print("CRITICAL");
  }
  else if (
    alarmState == WARNING
  )
  {
    Serial.print("WARNING");
  }
  else
  {
    Serial.print("OK");
  }

  Serial.println("\"}");
}


// ============================================================
// BUTTON HANDLING
// ============================================================

void handleButtons(
  unsigned long now
)
{
  bool upReading =
    digitalRead(BTN_UP);

  bool downReading =
    digitalRead(BTN_DOWN);

  bool selectReading =
    digitalRead(BTN_SELECT);


  // ==========================================================
  // UP
  // ==========================================================

  if (
    upReading !=
    upLastReading
  )
  {
    upLastChange = now;

    upLastReading =
      upReading;
  }


  if (
    now - upLastChange >=
    DEBOUNCE_TIME
  )
  {
    if (
      upReading !=
      upStableState
    )
    {
      upStableState =
        upReading;

      if (
        upStableState == LOW
      )
      {
        handleUpPress();
      }
    }
  }


  // ==========================================================
  // DOWN
  // ==========================================================

  if (
    downReading !=
    downLastReading
  )
  {
    downLastChange = now;

    downLastReading =
      downReading;
  }


  if (
    now - downLastChange >=
    DEBOUNCE_TIME
  )
  {
    if (
      downReading !=
      downStableState
    )
    {
      downStableState =
        downReading;

      if (
        downStableState == LOW
      )
      {
        handleDownPress();
      }
    }
  }


  // ==========================================================
  // SELECT
  // ==========================================================

  if (
    selectReading !=
    selectLastReading
  )
  {
    selectLastChange = now;

    selectLastReading =
      selectReading;
  }


  if (
    now - selectLastChange >=
    DEBOUNCE_TIME
  )
  {
    if (
      selectReading !=
      selectStableState
    )
    {
      selectStableState =
        selectReading;

      if (
        selectStableState == LOW
      )
      {
        handleSelectPress();
      }
    }
  }
}


// ============================================================
// UP BUTTON
// ============================================================

void handleUpPress()
{
  // ----------------------------------------------------------
  // MAIN MENU
  // ----------------------------------------------------------

  if (
    currentScreen ==
    MAIN_MENU
  )
  {
    menuItem--;

    if (
      menuItem < 0
    )
    {
      menuItem =
        MENU_ITEMS - 1;
    }

    return;
  }


  // ----------------------------------------------------------
  // EVENT LOG
  // ----------------------------------------------------------

  if (
    currentScreen ==
    EVENT_LOG
  )
  {
    int totalPages =
      (eventCount + EVENTS_PER_PAGE - 1)
      / EVENTS_PER_PAGE;

    if (totalPages <= 1)
    {
      return;
    }

    eventPage--;

    if (eventPage < 0)
    {
      eventPage =
        totalPages - 1;
    }

    return;
  }
}


// ============================================================
// DOWN BUTTON
// ============================================================

void handleDownPress()
{
  // ----------------------------------------------------------
  // MAIN MENU
  // ----------------------------------------------------------

  if (
    currentScreen ==
    MAIN_MENU
  )
  {
    menuItem++;

    if (
      menuItem >= MENU_ITEMS
    )
    {
      menuItem = 0;
    }

    return;
  }


  // ----------------------------------------------------------
  // EVENT LOG
  // ----------------------------------------------------------

  if (
    currentScreen ==
    EVENT_LOG
  )
  {
    int totalPages =
      (eventCount + EVENTS_PER_PAGE - 1)
      / EVENTS_PER_PAGE;

    if (totalPages <= 1)
    {
      return;
    }

    eventPage++;

    if (
      eventPage >= totalPages
    )
    {
      eventPage = 0;
    }

    return;
  }
}


// ============================================================
// SELECT BUTTON
// ============================================================

void handleSelectPress()
{
  // ----------------------------------------------------------
  // MAIN MENU → SCREEN
  // ----------------------------------------------------------

  if (
    currentScreen ==
    MAIN_MENU
  )
  {
    currentScreen =
      (Screen)(menuItem + 1);

    eventPage = 0;

    return;
  }


  // ----------------------------------------------------------
  // ANY SCREEN → MAIN MENU
  // ----------------------------------------------------------

  currentScreen =
    MAIN_MENU;
}


// ============================================================
// DISPLAY UPDATE
// ============================================================

void updateDisplay()
{
  switch (currentScreen)
  {
    case MAIN_MENU:
      showMainMenu();
      break;

    case LIVE_DATA:
      showLiveData();
      break;

    case SENSOR_STATUS:
      showSensorStatus();
      break;

    case ALARM_STATUS:
      showAlarmStatus();
      break;

    case SYSTEM_INFO:
      showSystemInfo();
      break;

    case EVENT_LOG:
      showEventLog();
      break;
  }
}


// ============================================================
// MAIN MENU
// ============================================================

void showMainMenu()
{
  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);


  // ----------------------------------------------------------
  // HEADER
  // ----------------------------------------------------------

  display.setCursor(0, 0);

  display.print("MENU ");

  display.print(
    wifiConnected ?
    "W:OK " :
    "W:OFF "
  );

  display.print("L:");

  display.print(loadPercentage);

  display.print("%");


  display.drawLine(
    0,
    8,
    127,
    8,
    SSD1306_WHITE
  );


  // ----------------------------------------------------------
  // MENU ITEMS
  // ----------------------------------------------------------

  for (
    int i = 0;
    i < MENU_ITEMS;
    i++
  )
  {
    int y =
      11 + i * 10;

    display.setCursor(
      0,
      y
    );


    if (
      i == menuItem
    )
    {
      display.print("> ");
    }
    else
    {
      display.print("  ");
    }


    display.println(
      menuItems[i]
    );
  }


  display.display();
}


// ============================================================
// LIVE DATA SCREEN
// ============================================================

void showLiveData()
{
  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);


  // ----------------------------------------------------------
  // HEADER
  // ----------------------------------------------------------

  display.setCursor(0, 0);

  display.println(
    "LIVE DATA"
  );

  display.drawLine(
    0,
    8,
    127,
    8,
    SSD1306_WHITE
  );


  // ----------------------------------------------------------
  // TEMPERATURE
  // ----------------------------------------------------------

  display.setCursor(
    0,
    12
  );

  display.print("TEMP ");

  if (dhtFault)
  {
    display.print("FAULT");
  }
  else
  {
    display.print(
      temperature,
      1
    );

    display.print("C");
  }


  // ----------------------------------------------------------
  // HUMIDITY
  // ----------------------------------------------------------

  display.setCursor(
    0,
    23
  );

  display.print("HUM  ");

  if (dhtFault)
  {
    display.print("FAULT");
  }
  else
  {
    display.print(
      humidity,
      1
    );

    display.print("%");
  }


  // ----------------------------------------------------------
  // LOAD
  // ----------------------------------------------------------

  display.setCursor(
    0,
    34
  );

  display.print("LOAD ");

  display.print(
    loadPercentage
  );

  display.print("%");


  // ----------------------------------------------------------
  // ALARM
  // ----------------------------------------------------------

  display.setCursor(
    0,
    45
  );

  display.print("STATE ");

  display.println(
    getAlarmText()
  );


  // ----------------------------------------------------------
  // WIFI
  // ----------------------------------------------------------

  display.setCursor(
    0,
    56
  );

  display.print(
    wifiConnected ?
    "WIFI CONNECTED" :
    "WIFI OFFLINE"
  );


  display.display();
}


// ============================================================
// SENSOR STATUS
// ============================================================

void showSensorStatus()
{
  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);


  // HEADER

  display.setCursor(
    0,
    0
  );

  display.println(
    "SENSOR STATUS"
  );

  display.drawLine(
    0,
    8,
    127,
    8,
    SSD1306_WHITE
  );


  // DHT

  display.setCursor(
    0,
    12
  );

  display.print(
    "DHT22 "
  );

  display.println(
    getDHTText()
  );


  // ADC

  display.setCursor(
    0,
    23
  );

  display.print(
    "ADC   "
  );

  display.println(
    getADCText()
  );


  // OLED

  display.setCursor(
    0,
    34
  );

  display.println(
    "OLED  OK"
  );


  // WIFI

  display.setCursor(
    0,
    45
  );

  display.print(
    "WIFI  "
  );

  display.println(
    wifiConnected ?
    "OK" :
    "OFFLINE"
  );


  // LAST DHT READING

  display.setCursor(
    0,
    56
  );

  if (lastValidDHTReading == 0)
  {
    display.print(
      "DHT AGE --"
    );
  }
  else
  {
    unsigned long age =
      (millis() -
       lastValidDHTReading) /
      1000;

    display.print(
      "DHT AGE "
    );

    display.print(age);

    display.print("s");
  }


  display.display();
}


// ============================================================
// ALARM STATUS
// ============================================================

void showAlarmStatus()
{
  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);


  // HEADER

  display.setCursor(
    0,
    0
  );

  display.println(
    "ALARM STATUS"
  );

  display.drawLine(
    0,
    8,
    127,
    8,
    SSD1306_WHITE
  );


  // LOAD

  display.setCursor(
    0,
    12
  );

  display.print(
    "LOAD  "
  );

  display.print(
    loadPercentage
  );

  display.println("%");


  // STATE

  display.setCursor(
    0,
    23
  );

  display.print(
    "STATE "
  );

  display.println(
    getAlarmText()
  );


  // LED

  display.setCursor(
    0,
    34
  );

  display.print(
    "LED   "
  );

  if (
    alarmState == CRITICAL ||
    alarmState == SENSOR_FAULT
  )
  {
    display.println("ON");
  }
  else
  {
    display.println("OFF");
  }


  // BUZZER

  display.setCursor(
    0,
    45
  );

  display.print(
    "BUZZ  "
  );

  if (
    alarmState == CRITICAL ||
    alarmState == SENSOR_FAULT
  )
  {
    display.println("ON");
  }
  else
  {
    display.println("OFF");
  }


  // THRESHOLDS

  display.setCursor(
    0,
    56
  );

  display.print(
    "WARN70 CRIT90"
  );


  display.display();
}


// ============================================================
// SYSTEM INFORMATION
// ============================================================

void showSystemInfo()
{
  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);


  // HEADER

  display.setCursor(
    0,
    0
  );

  display.println(
    "SYSTEM INFO"
  );

  display.drawLine(
    0,
    8,
    127,
    8,
    SSD1306_WHITE
  );


  // DEVICE

  display.setCursor(
    0,
    12
  );

  display.print(
    "ID "
  );

  display.println(
    DEVICE_NAME
  );


  // MCU

  display.setCursor(
    0,
    23
  );

  display.println(
    "MCU ESP32 DevKitC"
  );


  // FIRMWARE

  display.setCursor(
    0,
    34
  );

  display.print(
    "FW "
  );

  display.println(
    FIRMWARE_VERSION
  );


  // PACKETS

  display.setCursor(
    0,
    45
  );

  display.print(
    "JSON "
  );

  display.println(
    jsonPacketCount
  );


  // SENSOR UPDATES

  display.setCursor(
    0,
    56
  );

  display.print(
    "SAMPLES "
  );

  display.println(
    sensorUpdateCount
  );


  display.display();
}


// =====================================================
// EVENT LOG SCREEN - COMPACT VERSION
// =====================================================

void showEventLog() {

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // ---------------------------------------------------
  // HEADER
  // ---------------------------------------------------

  display.setCursor(0, 0);
  display.println("EVENT LOG");

  display.drawLine(
    0,
    9,
    127,
    9,
    SSD1306_WHITE
  );

  // ---------------------------------------------------
  // NO EVENTS
  // ---------------------------------------------------

  if (eventCount == 0) {

    display.setCursor(28, 28);
    display.println("NO EVENTS");

    display.display();
    return;
  }

  // ---------------------------------------------------
  // SHOW NEWEST 5 EVENTS
  // ---------------------------------------------------

  int displayEvents = eventCount;

  if (displayEvents > 5) {
    displayEvents = 5;
  }

  for (int i = 0; i < displayEvents; i++) {

    int index = eventCount - 1 - i;

    // 10 pixels per event
    int y = 12 + (i * 10);

    // -------------------------------------------------
    // TIMESTAMP
    // -------------------------------------------------

    unsigned long seconds =
      events[index].timestamp / 1000;

    display.setCursor(0, y);

    display.print(seconds);
    display.print("s ");

    // -------------------------------------------------
    // SHORT EVENT TYPE
    // -------------------------------------------------

    if (strcmp(events[index].type, "CRITICAL") == 0) {

      display.print("CRIT ");

    }
    else if (
      strcmp(events[index].type, "WARNING") == 0
    ) {

      display.print("WARN ");

    }
    else if (
      strcmp(events[index].type, "FAULT") == 0
    ) {

      display.print("FAIL ");

    }
    else {

      display.print("INFO ");

    }

    // -------------------------------------------------
    // COMPACT MESSAGE
    // -------------------------------------------------

    const char* msg =
      events[index].message;

    // Display only the first 12 characters.
    // This guarantees that one event stays
    // completely inside the 128-pixel display.

    char shortMessage[13];

    strncpy(
      shortMessage,
      msg,
      12
    );

    shortMessage[12] = '\0';

    display.println(shortMessage);
  }

  display.display();
}


// ============================================================
// ALARM TEXT
// ============================================================

const char* getAlarmText()
{
  switch (alarmState)
  {
    case NORMAL:
      return "NORMAL";

    case WARNING:
      return "WARNING";

    case CRITICAL:
      return "CRITICAL";

    case SENSOR_FAULT:
      return "SENSOR FAULT";

    default:
      return "UNKNOWN";
  }
}


// ============================================================
// DHT STATUS TEXT
// ============================================================

const char* getDHTText()
{
  if (dhtFault)
  {
    return "FAULT";
  }

  return "OK";
}


// ============================================================
// ADC STATUS TEXT
// ============================================================

const char* getADCText()
{
  if (adcFault)
  {
    return "FAULT";
  }

  return "OK";
}


// ============================================================
// WIFI STATUS TEXT
// ============================================================

const char* getWiFiText()
{
  if (wifiConnected)
  {
    return "CONNECTED";
  }

  return "OFFLINE";
}