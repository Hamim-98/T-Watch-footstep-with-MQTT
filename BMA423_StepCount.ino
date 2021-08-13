#include "config.h"
#include <WiFi.h>
#include <MQTT.h>

const char ssid[]     = "iPhone";       // replace with your WiFi SSID
const char password[] = "nakapa??";   // replace with your WiFi password

const char clientId[] = "ttgowatch";       // replace with your MQTT Client Id
const char topic[]    = "hajjtracker/step";      // replace with your MQTT Topic
const char server[]   = "nex.airmode.live";  // replace with your MQTT Broker

WiFiClient net;
MQTTClient mqtt;

TTGOClass *ttgo;
PCF8563_Class *rtc;
TFT_eSPI *tft;
BMA *sensor;
bool irq = false;

unsigned long lastMillis = 0;
const char *ntpServer       = "0.asia.pool.ntp.org";
const long  gmtOffset_sec   = 3600;
const int   daylightOffset_sec = 3600;

bool rtcIrq = false;

// Sub-function to connect to WiFi and MQTT broker
void connect() {
  
  Serial.print("Connecting to WiFi ...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" connected!");

  Serial.print("Connecting to MQTT broker ...");
  while (!mqtt.connect(clientId)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" connected!");
}

// Sub function for incoming message from topic subscription
void messageReceived(String &topic, String &payload) {
  
}

void setup()
{
    Serial.begin(115200);

    // STEP 1 - Initialized WiFi connectivity
    WiFi.begin(ssid, password);

    mqtt.begin(server, net);
    mqtt.onMessage(messageReceived);

    // STEP 2 - Connecting to WiFi and MQTT broker
    connect();

    // Get TTGOClass instance
    ttgo = TTGOClass::getWatch();

    // Initialize the hardware, the BMA423 sensor has been initialized internally
    ttgo->begin();

    // Turn on the backlight
    ttgo->openBL();

    //Receive objects for easy writing
    rtc = ttgo->rtc;
    tft = ttgo->tft;
    sensor = ttgo->bma;
    tft->setTextColor(TFT_GREEN, TFT_BLACK);

     //init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        tft->println("Failed to obtain time, Restart in 3 seconds");
        Serial.println("Failed to obtain time, Restart in 3 seconds");
        delay(3000);
        esp_restart();
        while (1);
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    tft->println("Time synchronization succeeded");
    // Sync local time to external RTC
    rtc->syncToRtc();
    tft->setFreeFont(&FreeMonoOblique9pt7b);

    // Accel parameter structure
    Acfg cfg;
    /*!
        Output data rate in Hz, Optional parameters:
            - BMA4_OUTPUT_DATA_RATE_0_78HZ
            - BMA4_OUTPUT_DATA_RATE_1_56HZ
            - BMA4_OUTPUT_DATA_RATE_3_12HZ
            - BMA4_OUTPUT_DATA_RATE_6_25HZ
            - BMA4_OUTPUT_DATA_RATE_12_5HZ
            - BMA4_OUTPUT_DATA_RATE_25HZ
            - BMA4_OUTPUT_DATA_RATE_50HZ
            - BMA4_OUTPUT_DATA_RATE_100HZ
            - BMA4_OUTPUT_DATA_RATE_200HZ
            - BMA4_OUTPUT_DATA_RATE_400HZ
            - BMA4_OUTPUT_DATA_RATE_800HZ
            - BMA4_OUTPUT_DATA_RATE_1600HZ
    */
    cfg.odr = BMA4_OUTPUT_DATA_RATE_100HZ;
    /*!
        G-range, Optional parameters:
            - BMA4_ACCEL_RANGE_2G
            - BMA4_ACCEL_RANGE_4G
            - BMA4_ACCEL_RANGE_8G
            - BMA4_ACCEL_RANGE_16G
    */
    cfg.range = BMA4_ACCEL_RANGE_2G;
    /*!
        Bandwidth parameter, determines filter configuration, Optional parameters:
            - BMA4_ACCEL_OSR4_AVG1
            - BMA4_ACCEL_OSR2_AVG2
            - BMA4_ACCEL_NORMAL_AVG4
            - BMA4_ACCEL_CIC_AVG8
            - BMA4_ACCEL_RES_AVG16
            - BMA4_ACCEL_RES_AVG32
            - BMA4_ACCEL_RES_AVG64
            - BMA4_ACCEL_RES_AVG128
    */
    cfg.bandwidth = BMA4_ACCEL_NORMAL_AVG4;

    /*! Filter performance mode , Optional parameters:
        - BMA4_CIC_AVG_MODE
        - BMA4_CONTINUOUS_MODE
    */
    cfg.perf_mode = BMA4_CONTINUOUS_MODE;

    // Configure the BMA423 accelerometer
    sensor->accelConfig(cfg);

    // Enable BMA423 accelerometer
    // Warning : Need to use steps, you must first enable the accelerometer
    // Warning : Need to use steps, you must first enable the accelerometer
    // Warning : Need to use steps, you must first enable the accelerometer
    sensor->enableAccel();

    pinMode(BMA423_INT1, INPUT);
    attachInterrupt(BMA423_INT1, [] {
        // Set interrupt to set irq value to 1
        irq = 1;
    }, RISING); //It must be a rising edge

    // Enable BMA423 step count feature
    sensor->enableFeature(BMA423_STEP_CNTR, true);

    // Reset steps
    sensor->resetStepCounter();

    // Turn on step interrupt
    sensor->enableStepCountInterrupt();

    // Some display settings
    tft->setTextColor(random(0xFFFF));
    tft->setCursor(0, 50);
    tft->drawString("<<STEP COUNTER>>", 20, 50, 7);
    //tft->drawString("<<STEP COUNTER>>", 3, 50, 4);
    tft->setTextFont(4);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
}

void loop()
{
    tft->drawString(rtc->formatDateTime(), 15, 80, 7);
    tft->drawString(rtc->formatDateTime(PCF_TIMEFORMAT_MM_DD_YYYY), 60, 160);
    delay(1000);
   
    if (irq) {
        irq = 0;
        bool  rlst;
        do {
            // Read the BMA423 interrupt status,
            // need to wait for it to return to true before continuing
            rlst =  sensor->readInterrupt();
        } while (!rlst);

        // Check if it is a step interrupt
        if (sensor->isStepCounter()) {
            // Get step data from register
            uint32_t step = sensor->getCounter();
            tft->setTextColor(random(0xFFFF), TFT_BLACK);
            tft->setCursor(60, 200);
            tft->print("Your steps:");
            tft->print(step);
            Serial.println(step);
        }
    }
    delay(20);

    mqtt.loop();
    delay(10);  // <- fixes some issues with WiFi stability

    if (!mqtt.connected()) connect();

    // STEP 3 - Sensors reading for data acquisition
  

    // update data interval to Favoriot data stream using millis() function
    int stepcount = sensor->getCounter();
     if(millis() - lastMillis > 5000){
    lastMillis = millis();

    // STEP 4 - Send data to MQTT topic
    mqtt.publish(topic, String(stepcount));
  }
}
