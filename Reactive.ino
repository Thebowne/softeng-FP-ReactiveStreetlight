#include <Wire.h>   
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Pins
const int ledPin = 9; 
const int pirPin = 7;
const int lightSensorPin = A2;
const int forceSensorPin = A0;

// Thresholds Set
const int DARKNESS_THRESHOLD = 40;
const int PRESSURE_THRESHOLD = 40;

// Light On Duration
const unsigned long LIGHT_DURATION = 60000;
const unsigned long VEHICLE_HOLD_TIME = 8000;
const unsigned long PEDESTRIAN_HOLD_TIME = 10000;

// Brightness Levels
const int BRIGHTNESS_OFF = 0;
const int BRIGHTNESS_DARK = 77;
const int BRIGHTNESS_PEDESTRIAN = 192;
const int BRIGHTNESS_VEHICLE = 250;

// State tracking
struct PedestrianState {
    bool active;
    unsigned long endTime;
    unsigned long lastTriggerTime;  
};

struct VehicleState {
    bool active;
    unsigned long endTime;
    unsigned long lastTriggerTime;  
};

PedestrianState pedestrian = {false, 0, 0};
VehicleState vehicle = {false, 0, 0};
unsigned long overallStartTime = 0;
int currentBrightness = BRIGHTNESS_OFF;
bool isDark = false;

// Cooldown to prevent repeated triggers
const unsigned long TRIGGER_COOLDOWN = 1000;  

void setup() {
    Serial.begin(115200);
    
    lcd.init();
    lcd.backlight();
    
    lcd.print("Smart Street");
    lcd.setCursor(0, 1);
    lcd.print("Light v3.1");
    delay(2000);
    lcd.clear();
    
    pinMode(ledPin, OUTPUT);
    pinMode(lightSensorPin, INPUT);
    pinMode(pirPin, INPUT);
    pinMode(forceSensorPin, INPUT);
    
    analogWrite(ledPin, BRIGHTNESS_OFF);
    
    Serial.println("System ready. PIR needs warm-up time.");
}

void loop() {
    int lightValue = analogRead(lightSensorPin);
    bool pedestrianNow = digitalRead(pirPin);
    int forceValue = analogRead(forceSensorPin);
    int pressure = constrain(map(forceValue, 0, 466, 0, 100), 0, 100);
    int lightLevel = constrain(map(lightValue, 0, 679, 0, 100), 0, 100);
    
    isDark = (lightLevel < DARKNESS_THRESHOLD);
    bool vehicleNow = (pressure > PRESSURE_THRESHOLD);
    
    unsigned long currentTime = millis();
    
    // Pedestrian detection with cooldown
    if (isDark && pedestrianNow) {
        // Check if enough time has passed since last trigger
        if (currentTime - pedestrian.lastTriggerTime > TRIGGER_COOLDOWN) {
            pedestrian.active = true;
            pedestrian.endTime = currentTime + PEDESTRIAN_HOLD_TIME;
            pedestrian.lastTriggerTime = currentTime;
            
            Serial.println("=== PEDESTRIAN DETECTED - 20s timer ===");
            Serial.print("Current brightness should be: ");
            Serial.print(map(BRIGHTNESS_PEDESTRIAN, 0, 255, 0, 100));
            Serial.println("%");
        }
    }
    
    // Vehicle detection with cooldown
    if (isDark && vehicleNow) {
        if (currentTime - vehicle.lastTriggerTime > TRIGGER_COOLDOWN) {
            vehicle.active = true;
            vehicle.endTime = currentTime + VEHICLE_HOLD_TIME;
            vehicle.lastTriggerTime = currentTime;
            
            Serial.println("=== VEHICLE DETECTED - 8s timer ===");
        }
    }
    
    // Update timers and check expiration
    if (pedestrian.active && currentTime >= pedestrian.endTime) {
        pedestrian.active = false;
        Serial.println("Pedestrian timer expired");
    }
    
    if (vehicle.active && currentTime >= vehicle.endTime) {
        vehicle.active = false;
        Serial.println("Vehicle timer expired");
    }
    
    int newBrightness = BRIGHTNESS_OFF;
    
    if (isDark) {
        newBrightness = BRIGHTNESS_DARK;
        
        if (pedestrian.active) {
            newBrightness = BRIGHTNESS_PEDESTRIAN;
        }
        
        if (vehicle.active) {
            newBrightness = BRIGHTNESS_VEHICLE;
        }
    }
    
    // DEBUG: Print when brightness should change
    if (newBrightness != currentBrightness) {
        Serial.print("DEBUG: Brightness changing from ");
        Serial.print(map(currentBrightness, 0, 255, 0, 100));
        Serial.print("% to ");
        Serial.print(map(newBrightness, 0, 255, 0, 100));
        Serial.println("%");
    }

    if (newBrightness != currentBrightness) {
        currentBrightness = newBrightness;
        analogWrite(ledPin, currentBrightness);
        
        Serial.print(">>> Brightness SET to: ");
        Serial.print(map(currentBrightness, 0, 255, 0, 100));
        Serial.println("% <<<");
        
        if (currentBrightness > BRIGHTNESS_OFF && overallStartTime == 0) {
            overallStartTime = currentTime;
        } else if (currentBrightness == BRIGHTNESS_OFF) {
            overallStartTime = 0;
        }
    }
    
    if (currentBrightness > BRIGHTNESS_OFF && 
        (currentTime - overallStartTime > LIGHT_DURATION)) {
        currentBrightness = BRIGHTNESS_OFF;
        analogWrite(ledPin, BRIGHTNESS_OFF);
        pedestrian.active = false;
        vehicle.active = false;
        overallStartTime = 0;
        Serial.println("Overall duration expired - light off");
    }
    
    static unsigned long lastPrint = 0;
    if (currentTime - lastPrint > 1000) {
        long pedestrianRemaining = pedestrian.active ? (pedestrian.endTime - currentTime) / 1000 : 0;
        long vehicleRemaining = vehicle.active ? (vehicle.endTime - currentTime) / 1000 : 0;
        
        // Serial Monitor for debugging
        Serial.print("Light: ");
        Serial.print(lightLevel);
        Serial.print("% | Dark: ");
        Serial.print(isDark ? "Yes" : "No");
        Serial.print(" | PIR: ");
        Serial.print(pedestrianNow ? "HIGH" : "LOW");
        Serial.print(" | Ped Active: ");
        Serial.print(pedestrian.active ? "Yes" : "No");
        Serial.print(" (");
        Serial.print(pedestrianRemaining);
        Serial.print("s)");
        Serial.print(" | Last Trigger: ");
        Serial.print((currentTime - pedestrian.lastTriggerTime) / 1000);
        Serial.print("s ago");
        Serial.print(" | Vehicle: ");
        Serial.print(vehicleNow ? "Yes" : "No");
        Serial.print(" (");
        Serial.print(vehicleRemaining);
        Serial.print("s)");
        Serial.print(" | Pressure: ");
        Serial.print(pressure);
        Serial.print("%");
        Serial.print(" | Brightness: ");
        Serial.print(map(currentBrightness, 0, 255, 0, 100));
        Serial.println("%");
        
        lcd.clear();
        
        lcd.setCursor(0, 0);
        if (pedestrian.active) {
            lcd.print("WALK");
            lcd.print(pedestrianRemaining);
            lcd.print("s ");
        } else if (pedestrianNow) {
            lcd.print("PIR:ON  ");
        } else {
            lcd.print("        ");
        }
        
        if (vehicle.active) {
            lcd.print("CAR");
            lcd.print(vehicleRemaining);
            lcd.print("s ");
        }
        
        lcd.print("L:");
        lcd.print(lightLevel);
        lcd.print("%");
        
        // Print Brightness Level
        lcd.setCursor(0, 1);
        if (currentBrightness == BRIGHTNESS_OFF) {
            lcd.print("OFF        ");
        } else if (currentBrightness == BRIGHTNESS_DARK) {
            lcd.print("DARK 30%   ");
        } else if (currentBrightness == BRIGHTNESS_PEDESTRIAN) {
            lcd.print("PED: 75%");
        } else if (currentBrightness == BRIGHTNESS_VEHICLE) {
            lcd.print("CAR: 100%");
        }
        
        if (pedestrian.active || vehicle.active) {
            lcd.print(" T:");
            if (overallStartTime > 0) {
                lcd.print((LIGHT_DURATION - (currentTime - overallStartTime)) / 1000);
                lcd.print("s");
            }
        }
        
        lastPrint = currentTime;
    }
    
    delay(50);
}