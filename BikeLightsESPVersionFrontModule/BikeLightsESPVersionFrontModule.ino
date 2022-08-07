
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>

#include <AutoConnect.h>

#include <AsyncUDP.h>

#define LEFT_BUTTON GPIO_NUM_25
#define RIGHT_BUTTON GPIO_NUM_27
#define LEFT_BUTTON_LED GPIO_NUM_26
#define RIGHT_BUTTON_LED GPIO_NUM_23
#define LEFT_FRONT GPIO_NUM_15
#define RIGHT_FRONT GPIO_NUM_2
#define POWER_BUTTON GPIO_NUM_4
#define MAIN_POWER GPIO_NUM_16

Adafruit_NeoPixel leftFront = Adafruit_NeoPixel(36, LEFT_FRONT, NEO_GRBW + NEO_KHZ800);
Adafruit_NeoPixel rightFront = Adafruit_NeoPixel(36, RIGHT_FRONT, NEO_GRBW + NEO_KHZ800);
bool blink = false;
unsigned long microsBlink = 0;
unsigned long microsIndicate = 0;
bool lastLeftState = true;
bool leftState = false;
bool lastRightState = true;
bool rightState = false;
bool indicating = false;
bool indicateLeft = false;
bool indicateRight = false;
bool stateChange = false;
unsigned long debounceDelay = 50;
unsigned long millisBounce = 0;
unsigned long tmp = 0;
String UNIQUE_KEY = "01ad64hg";
String strStatus = "z";

unsigned long lastSend = 0;
unsigned int sendDelay = 900;

const char* ssid = "KdBikeLights";
const char* password = "1234567890";

int status = WL_IDLE_STATUS;
char server[] = "192.168.4.1";

WiFiClient client;

AsyncUDP udp;

const int freq = 5000;
const int leftLedChannel = 0;
const int rightLedChannel = 1;
const int resolution = 8;

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("Disconnected from WiFi access point");
    Serial.print("WiFi lost connection. Reason: ");
    Serial.println(info.disconnected.reason);
    Serial.println("Trying to Reconnect");
    WiFi.begin(ssid, password);
}

void setup()
{
  Serial.begin(921600);
  pinMode(MAIN_POWER, OUTPUT);
  digitalWrite(MAIN_POWER, HIGH);
  leftFront.begin();
  leftFront.show(); // Initialize all pixels to 'off'
  rightFront.begin();
  rightFront.show(); // Initialize all pixels to 'off'
  pinMode(LEFT_BUTTON, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON, INPUT_PULLUP);
  ledcSetup(leftLedChannel, freq, resolution);
  ledcSetup(rightLedChannel, freq, resolution);
  ledcAttachPin(LEFT_BUTTON_LED, leftLedChannel);
  ledcAttachPin(RIGHT_BUTTON_LED, rightLedChannel);

  ledcWrite(leftLedChannel, 20);
  ledcWrite(rightLedChannel, 20);


  Serial.print("Connecting to: ");
  Serial.println(ssid);

  bool blinkStat = false;

  WiFi.disconnect(true);

  delay(1000);

  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);


  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
      Serial.print("status ");
      Serial.println(WiFi.status());
      delay(1000);
      if (blinkStat) {
          ledcWrite(leftLedChannel, 20);
          ledcWrite(rightLedChannel, 255);
          blinkStat = false;
          Serial.println("true");
      }
      else {
          ledcWrite(leftLedChannel, 255);
          ledcWrite(rightLedChannel, 20);
          blinkStat = true;
          Serial.println("false");
      }
  }

  Serial.println("Connected to wifi");
  pinMode(POWER_BUTTON, INPUT_PULLDOWN);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 1);
  delay(500);
  WakeUp();
}
void loop() // run over and over
{
    int power;
    power = digitalRead(POWER_BUTTON);
    if (power == 1) {
        digitalWrite(MAIN_POWER, LOW);
        esp_deep_sleep_start();
    }

  unsigned long now = millis();
  if (now > lastSend + sendDelay) {
      /*if ((WiFi.status() != WL_CONNECTED)) {
          Serial.print(millis());
          Serial.println("Reconnecting to WiFi...");
          WiFi.disconnect();
          WiFi.begin(ssid, password);
      }*/
    lastSend = now;
    String message = UNIQUE_KEY + strStatus;
    Serial.println(message);
    SendStatus();
  }

  ProcessButtons();
  UpdateLeds();
}

void WakeUp() {
  int led = 0;
  for (int j = 0; j < 36; j++) {
    for (int i = 0; i < 36; i++) {
      if (i == led) {
        leftFront.setPixelColor(i, 255, 255, 255, 255);
        rightFront.setPixelColor(i, 255, 255, 255, 255);
      }
      else
      {
        leftFront.setPixelColor(i, 0, 0, 0, 0);
        rightFront.setPixelColor(i, 0, 0, 0, 0);
      }
    }
    leftFront.show();
    rightFront.show();
    delay(10);
    led++;
  }
  for (int j = 0; j < 36; j++) {
    for (int i = 35; i > -1; i--) {
      if (i == led) {
        leftFront.setPixelColor(i, 255, 255, 255, 255);
        rightFront.setPixelColor(i, 255, 255, 255, 255);
      }
      else
      {
        leftFront.setPixelColor(i, 0, 0, 0, 0);
        rightFront.setPixelColor(i, 0, 0, 0, 0);
      }
    }
    leftFront.show();
    rightFront.show();
    delay(10);
    led--;
  }
}

void ProcessButtons() {
  bool leftReading = !digitalRead(LEFT_BUTTON);
  bool rightReading = !digitalRead(RIGHT_BUTTON);

  if (!(leftReading && rightReading)) { //check both buttons are not pressed
    if (leftReading != lastLeftState) {
      millisBounce = millis();
    }

    if (rightReading != lastRightState) {
      millisBounce = millis();
    }
    tmp = millis() - millisBounce;
    if (tmp > 50) {
      if (leftReading != leftState) {
        leftState = leftReading;

        stateChange = true;
      }
      if (rightReading != rightState)
      {
        rightState = rightReading;
        stateChange = true;
      }
    }
    lastLeftState = leftReading;
    lastRightState = rightReading;

    if (leftState && stateChange) {
      indicateLeft = !indicateLeft;
      if (indicateLeft) indicateRight = false;

      stateChange = false;

      if (indicateLeft) {
        strStatus = "q";
      }
      else
      {
        strStatus = "z";
      }

      SendStatus();
      //delay(500);
    }
    if (rightState && stateChange) {
      indicateRight = !indicateRight;
      if (indicateRight) indicateLeft = false;
      stateChange = false;
      if (indicateRight) {
        strStatus = "a";
      }
      else
      {
        strStatus = "z";
      }

      Serial.println(UNIQUE_KEY + strStatus);
      SendStatus();
      //delay(500);
    }
  }
}

void UpdateLeds() {
  if (indicateLeft) {
    if (indicating == false) {
      ledcWrite(leftLedChannel, 255);
      ledcWrite(rightLedChannel, 20);
      for (uint16_t i = 0; i < 36; i++) {
        rightFront.setPixelColor(i, rightFront.Color(0, 0, 0, 100));
      }
      rightFront.show();
      for (uint16_t i = 0; i < 36; i++) {
        leftFront.setPixelColor(i, leftFront.Color(255, 60, 0, 0));
        leftFront.show();
        ProcessButtons();
        delay(10);
      }
      microsIndicate = millis();
      indicating = true;
    }
    if (millis() > microsIndicate + 400) {
      ledcWrite(leftLedChannel, 20);
      for (uint16_t i = 0; i < 36; i++) {
        rightFront.setPixelColor(i, rightFront.Color(0, 0, 0, 100));
      }
      rightFront.show();
      for (uint16_t i = 0; i < 36; i++) {
        leftFront.setPixelColor(i, leftFront.Color(0, 0, 0, 100));
      }
      leftFront.show();
    }
    if (millis() > microsIndicate + 600) {
      indicating = false;
    }
  }
  if (indicateRight) {
    if (indicating == false) {
      ledcWrite(rightLedChannel, 255);
      ledcWrite(leftLedChannel, 20);
      for (uint16_t i = 0; i < 36; i++) {
        leftFront.setPixelColor(i, leftFront.Color(0, 0, 0, 100));
      }
      rightFront.show();
      for (uint16_t i = 0; i < 36; i++) {
        rightFront.setPixelColor(i, rightFront.Color(255, 60, 0, 0));
        rightFront.show();
        ProcessButtons();
        delay(10);
      }
      microsIndicate = millis();
      indicating = true;
    }
    if (millis() > microsIndicate + 400) {
      ledcWrite(rightLedChannel, 20);
      for (uint16_t i = 0; i < 36; i++) {
        leftFront.setPixelColor(i, leftFront.Color(0, 0, 0, 100));
      }
      rightFront.show();
      for (uint16_t i = 0; i < 36; i++) {
        rightFront.setPixelColor(i, rightFront.Color(0, 0, 0, 100));
      }
      rightFront.show();
    }
    if (millis() > microsIndicate + 600) {
      indicating = false;
    }
  }

  if (!indicateLeft && !indicateRight) {
    indicating = false;
    if ((millis() > microsBlink + 100 && millis() < microsBlink + 150) || millis() > microsBlink + 300) {
      blink = true;
    }
    if ((millis() > microsBlink + 150 && millis() < microsBlink + 200) || millis() > microsBlink + 350) {
      blink = false;
    }
    if (millis() > microsBlink + 400) microsBlink = millis();
    for (int j = 0; j < 36; j++)
    {
      leftFront.setPixelColor(j, 0, 0, 0, 100); //192
      rightFront.setPixelColor(j, 0, 0, 0, 100);

      if (blink) {
        if (j % 5 == 0) {
          leftFront.setPixelColor(j, 255, 255, 255, 255); //255
          rightFront.setPixelColor(j, 255, 255, 255, 255);
        }
      }
    }
    ledcWrite(leftLedChannel, 20);
    ledcWrite(rightLedChannel, 20);
  }
  leftFront.show();
  rightFront.show();
}

void SendStatus() {
    const char* stat;
    String message = UNIQUE_KEY + strStatus;
    udp.broadcastTo(message.c_str(), 4909);
}
