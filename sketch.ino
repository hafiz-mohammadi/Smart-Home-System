/* * PROJECT: Smart Home System (IoT)
 * AUTHOR: [Your Name]
 * HARDWARE: ESP32, DHT22, PIR, LDR, Servo, Relay, Buzzer, LCD
 * CLOUD: Blynk IoT Platform
 */

/* ================= 1. BLYNK CONFIGURATION ================= */
#define BLYNK_TEMPLATE_ID "TMPL6dMbY8r4i"
#define BLYNK_TEMPLATE_NAME "Smart Home System"
#define BLYNK_AUTH_TOKEN "8QaioEX2_-51jy5Tzz5ziuLR0ZoCiJd7"

/* ================= 2. LIBRARIES ================= */
#define BLYNK_PRINT Serial 
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <LiquidCrystal.h>
#include <DHT.h>

/* ================= 3. PIN DEFINITIONS ================= */
#define PIN_DHT     15   // Temp/Humidity
#define PIN_PIR     13   // Motion
#define PIN_LDR     34   // Light (Analog)
#define PIN_RELAY   26   // Room Light
#define PIN_SERVO   18   // AC/Window
#define PIN_BUZZER  27   // Alarm
#define LED_G       32   // Green (Safe)
#define LED_Y       33   // Yellow (Light On)
#define LED_R       25   // Red (Fire/Alarm)

/* ================= 4. OBJECT INITIALIZATION ================= */
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

LiquidCrystal lcd(19, 23, 5, 17, 16, 4); 
#define DHTTYPE DHT22
DHT dht(PIN_DHT, DHTTYPE);
BlynkTimer timer; 

/* ================= 5. GLOBAL VARIABLES ================= */
// AC Variables
int servoAngle = 90;         
bool manualAcOverride = false; 
bool isAcOn = false;         

// Light Variables
bool manualLightOverride = false; 
bool isLightOn = false;           

// NEW: Buzzer Variables
bool manualAlarmOverride = false; // True = App Panic Button
bool isAlarmOn = false;           // Actual Status

/* ================= 6. SETUP ROUTINE ================= */
void setup() {
  Serial.begin(115200);
  
  dht.begin();
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_SERVO, OUTPUT); 
  pinMode(LED_G, OUTPUT);
  pinMode(LED_Y, OUTPUT);
  pinMode(LED_R, OUTPUT);

  lcd.begin(16, 2);
  lcd.print("System Starting");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  lcd.clear();
  lcd.print("Online!");
  delay(1000); 

  timer.setInterval(1000L, sendSensorData);
}

/* ================= 7. HELPER FUNCTIONS ================= */
void manualServo(int angle) {
  int pulseWidth = map(angle, 0, 180, 500, 2400); 
  digitalWrite(PIN_SERVO, HIGH);
  delayMicroseconds(pulseWidth);
  digitalWrite(PIN_SERVO, LOW);
  delayMicroseconds(20000 - pulseWidth); 
}

/* ================= 8. BLYNK COMMANDS (DOWNSTREAM) ================= */

// AC CONTROL (V3)
BLYNK_WRITE(V3) {
  if (param.asInt() == 1) {
    manualAcOverride = true;
    servoAngle = 0; 
    Serial.println("APP: AC FORCED ON");
  } else {
    manualAcOverride = false; 
    Serial.println("APP: AC AUTO MODE");
  }
}

// LIGHT CONTROL (V5)
BLYNK_WRITE(V5) {
  if (param.asInt() == 1) {
    manualLightOverride = true;
    Serial.println("APP: LIGHT FORCED ON");
  } else {
    manualLightOverride = false;
    Serial.println("APP: LIGHT AUTO MODE");
  }
}

// NEW: ALARM CONTROL (V7)
BLYNK_WRITE(V7) {
  if (param.asInt() == 1) {
    manualAlarmOverride = true;
    Serial.println("APP: PANIC ALARM ON");
  } else {
    manualAlarmOverride = false;
    Serial.println("APP: ALARM OFF");
  }
}

/* ================= 9. MAIN LOGIC LOOP (UPSTREAM) ================= */
void sendSensorData() {
  // --- A. READ SENSORS ---
  float temp = dht.readTemperature();
  int motion = digitalRead(PIN_PIR);
  int lightRaw = analogRead(PIN_LDR); 

  // --- B. UPLOAD RAW DATA ---
  Blynk.virtualWrite(V0, temp);      
  Blynk.virtualWrite(V1, lightRaw);  
  Blynk.virtualWrite(V2, motion ? 255 : 0); 

  // --- C. SYSTEM LOGIC ---
  
  // 1. FIRE LOGIC (Highest Priority)
  bool isFire = (temp > 50.0);
  
  if (isFire) {
    // --- EMERGENCY MODE ---
    Serial.println("!! FIRE DETECTED !!");
    Blynk.logEvent("fire_alert", "Temperature Critical!"); 
    
    isAlarmOn = true; // Fire triggers alarm
    
    digitalWrite(LED_R, HIGH);     
    digitalWrite(LED_G, LOW);
    digitalWrite(PIN_RELAY, LOW);  // Cut Lights
    servoAngle = 0;                // Close Vents
    
    // LCD Override
    lcd.setCursor(0,0); lcd.print("!! FIRE ALERT !!");
    lcd.setCursor(0,1); lcd.print("Temp: " + String(temp, 1) + "C   ");
    
  } else {
    // --- NORMAL / MANUAL MODE ---
    
    // 2. ALARM LOGIC (Manual Panic Button)
    if (manualAlarmOverride) {
      isAlarmOn = true;
      digitalWrite(LED_R, HIGH); // Visual Alert
    } else {
      isAlarmOn = false;
      digitalWrite(LED_R, LOW);
    }

    // 3. LIGHT LOGIC
    if (manualLightOverride) {
      digitalWrite(PIN_RELAY, HIGH);
      digitalWrite(LED_Y, HIGH);
      isLightOn = true;
    } else {
      if (motion == HIGH && lightRaw < 1000) {
        digitalWrite(PIN_RELAY, HIGH);
        digitalWrite(LED_Y, HIGH);
        isLightOn = true;
      } else {
        digitalWrite(PIN_RELAY, LOW);
        digitalWrite(LED_Y, LOW);
        isLightOn = false;
      }
    }

    // 4. AC LOGIC
    if (manualAcOverride) {
      isAcOn = (servoAngle == 0); 
    } else {
      if (temp > 28.0) {
        servoAngle = 0; 
        digitalWrite(LED_G, LOW);
        isAcOn = true;
      } else {
        servoAngle = 90; 
        digitalWrite(LED_G, HIGH);
        isAcOn = false;
      }
    }

    // --- D. FEEDBACK & DISPLAY ---

    // Execute Buzzer State
    if (isAlarmOn) {
       digitalWrite(PIN_BUZZER, HIGH);
    } else {
       digitalWrite(PIN_BUZZER, LOW);
    }

    // LCD Update (Only if NOT Fire)
    // If Alarm is manually ON, we show "PANIC MODE"
    if (manualAlarmOverride) {
      lcd.setCursor(0,0); lcd.print("!! PANIC MODE !!");
      lcd.setCursor(0,1); lcd.print("SIREN ACTIVE    ");
    } else {
      // Standard Display
      lcd.setCursor(0, 0);
      lcd.print("T:" + String(temp, 1) + "C ");
      lcd.print(isLightOn ? "L:ON " : "L:-- ");

      lcd.setCursor(0, 1);
      lcd.print(isAcOn ? "AC:ON " : "AC:-- ");
      lcd.print(manualAcOverride ? "(APP)" : "(AUTO)");
    }
    
    // App Status Updates
    Blynk.virtualWrite(V4, isAcOn ? 255 : 0);    
    Blynk.virtualWrite(V6, isLightOn ? 255 : 0); 
    Blynk.virtualWrite(V8, isAlarmOn ? 255 : 0); // NEW: Buzzer Status
  }
}

/* ================= 10. MAIN EXECUTION LOOP ================= */
void loop() {
  Blynk.run();
  timer.run();
  manualServo(servoAngle);
}