/* FILL IN INFORMATION FROM BLYNK CONSOLE HERE */
#define BLYNK_TEMPLATE_ID "TMPL6dMbY8r4i"
#define BLYNK_TEMPLATE_NAME "Smart Home System"
#define BLYNK_AUTH_TOKEN "8QaioEX2_-51jy5Tzz5ziuLR0ZoCiJd7"

/* BLYNK & WIFI LIBRARIES */
#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

/* SENSOR LIBRARIES */
#include <LiquidCrystal.h>
#include <DHT.h>

/* ================= PINS ================= */
#define PIN_DHT     15
#define PIN_PIR     13
#define PIN_LDR     34
#define PIN_RELAY   26
#define PIN_SERVO   18   
#define PIN_BUZZER  27
#define LED_G       32
#define LED_Y       33
#define LED_R       25

/* ================= CREDENTIALS ================= */
// Wokwi's Virtual WiFi
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

/* ================= OBJECTS ================= */
LiquidCrystal lcd(19, 23, 5, 17, 16, 4);
#define DHTTYPE DHT22
DHT dht(PIN_DHT, DHTTYPE);
BlynkTimer timer; // Timer to avoid using delay()

/* ================= VARIABLES ================= */
int servoAngle = 90; // Default Open
bool manualOverride = false; // Is the App controlling the AC?

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  
  // Hardware Init
  dht.begin();
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_SERVO, OUTPUT); 
  pinMode(LED_G, OUTPUT);
  pinMode(LED_Y, OUTPUT);
  pinMode(LED_R, OUTPUT);

  // LCD Init
  lcd.begin(16, 2);
  lcd.print("Connecting...");

  // CONNECT TO CLOUD
  // This function hangs here until connected
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  lcd.clear();
  lcd.print("Online!");
  delay(1000);

  // Setup a Timer to run 'sendSensorData' every 1000ms (1 second)
  timer.setInterval(1000L, sendSensorData);
}

/* ================= MANUAL SERVO PULSE ================= */
// Keeps the servo steady without crashing the ESP32
void manualServo(int angle) {
  int pulseWidth = map(angle, 0, 180, 500, 2400);
  digitalWrite(PIN_SERVO, HIGH);
  delayMicroseconds(pulseWidth);
  digitalWrite(PIN_SERVO, LOW);
  delayMicroseconds(20000 - pulseWidth);
}

/* ================= BLYNK COMMANDS (DOWNSTREAM) ================= */
// This triggers when you flip the switch on the App (V3)
BLYNK_WRITE(V3) {
  int switchValue = param.asInt(); // 0 or 1
  
  if (switchValue == 1) {
    manualOverride = true;
    servoAngle = 0; // Force Close (AC ON)
    Serial.println("APP COMMAND: AC FORCED ON");
  } else {
    manualOverride = false; // Go back to automatic mode
    Serial.println("APP COMMAND: AUTO MODE");
  }
}

/* ================= SENSOR LOGIC (UPSTREAM) ================= */
// Runs every 1 second via Timer
void sendSensorData() {
  // 1. Read Data
  float temp = dht.readTemperature();
  int motion = digitalRead(PIN_PIR);
  int lightRaw = analogRead(PIN_LDR); 

  // 2. Send to Blynk (Cloud)
  Blynk.virtualWrite(V0, temp);      // V0 = Temperature Gauge
  Blynk.virtualWrite(V1, lightRaw);  // V1 = Light Graph (Optional)
  
  // V2 = Motion LED (255 is Max Brightness for Widget)
  if (motion == HIGH) {
    Blynk.virtualWrite(V2, 255); 
  } else {
    Blynk.virtualWrite(V2, 0);
  }

  // 3. Local Logic (Fire, Lights, AC)
  bool isFire = (temp > 50.0);
  
  if (isFire) {
    // --- FIRE MODE ---
    Serial.println("!! FIRE !!");
    Blynk.logEvent("fire_alert", "Temperature Critical!"); // Push Notification
    
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, LOW);
    digitalWrite(PIN_BUZZER, HIGH); // Beep handled by short pulses in loop or simple ON here
    
    servoAngle = 0; // Close Window
    digitalWrite(PIN_RELAY, LOW); // Lights OFF

    lcd.setCursor(0,0); lcd.print("!! FIRE ALERT !!");
    
  } else {
    // --- NORMAL MODE ---
    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(LED_R, LOW);
    
    // Light Logic
    if (motion && lightRaw < 1000) {
      digitalWrite(PIN_RELAY, HIGH);
      digitalWrite(LED_Y, HIGH);
    } else {
      digitalWrite(PIN_RELAY, LOW);
      digitalWrite(LED_Y, LOW);
    }

    // AC Logic (Only if App is NOT overriding)
    if (!manualOverride) {
      if (temp > 28.0) {
        servoAngle = 0; // Close for AC
        digitalWrite(LED_G, LOW);
      } else {
        servoAngle = 90; // Open Window
        digitalWrite(LED_G, HIGH);
      }
    }

    // LCD
    lcd.setCursor(0, 0);
    lcd.print("T:" + String(temp, 1) + "C ");
    lcd.print(manualOverride ? "APP " : "AUTO");
  }
}

/* ================= MAIN LOOP ================= */
void loop() {
  // These two MUST run as fast as possible
  Blynk.run();
  timer.run();
  
  // Drive Servo (Must happen frequently)
  manualServo(servoAngle);
}