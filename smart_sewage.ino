#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

const char *ssid = "Smart-Sewage-System";
const char *password = "12345678";

ESP8266WebServer server(80);
LiquidCrystal_I2C lcd(0x27, 16, 2);

const int PUMP_PIN = D3;  
const int TRIG_PIN = D7;  
const int ECHO_PIN = D5;  
const int GAS_PIN  = A0;  

const int PIPE_DEPTH = 8;     // Total depth of your pipe
const int TRIGGER_DIST = 3;   // Pump starts when 3cm space is left
const int GAS_THRESHOLD = 280; 
bool gasAlreadyPumped = false;
String pumpStatus = "SYSTEM IDLE";

String getHTML() {
  int gVal = analogRead(GAS_PIN);
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH);
  int dist = (dur == 0) ? PIPE_DEPTH : (dur * 0.034 / 2);
  
  // Logic: Height of sewage = Total Depth - Distance measured
  int sewageHeight = PIPE_DEPTH - dist;
  if(sewageHeight < 0) sewageHeight = 0; // Prevent negative numbers

  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta http-equiv='refresh' content='1'>";
  html += "<style>";
  html += "body { font-family: 'Segoe UI', sans-serif; background: linear-gradient(135deg, #0f2027, #2c5364); color: white; text-align: center; padding: 10px; }";
  html += ".card { background: rgba(255, 255, 255, 0.1); backdrop-filter: blur(15px); border-radius: 20px; padding: 20px; margin: 15px auto; max-width: 400px; border: 1px solid rgba(255,255,255,0.1); box-shadow: 0 8px 32px rgba(0,0,0,0.5); }";
  html += "h1 { letter-spacing: 2px; color: #00d2ff; font-size: 22px; }";
  html += ".value { font-size: 50px; font-weight: 800; margin: 5px 0; }";
  html += ".bar-bg { height: 15px; background: #333; border-radius: 10px; margin-top: 10px; overflow: hidden; }";
  html += ".bar-fill { height: 100%; transition: width 0.5s; background: #00d2ff; }";
  html += ".gas-warn { background: #ff416c; padding: 10px; border-radius: 10px; animation: blink 1s infinite; font-weight: bold; }";
  html += "@keyframes blink { 0% { opacity: 1; } 50% { opacity: 0.5; } 100% { opacity: 1; } }";
  html += "</style></head><body>";

  html += "<h1>SMART SEWAGE SYSTEM</h1>";
  
  // Dashboard shows Sewage Height (0 to 8cm)
  int levelPercent = map(constrain(sewageHeight, 0, PIPE_DEPTH), 0, PIPE_DEPTH, 5, 100); 
  html += "<div class='card'><h4>SEWAGE LEVEL</h4>";
  html += "<div class='value'>" + String(sewageHeight) + "<span style='font-size:20px; opacity:0.6;'>cm</span></div>";
  html += "<div class='bar-bg'><div class='bar-fill' style='width:" + String(levelPercent) + "%'></div></div></div>";

  html += "<div class='card'><h4>ATMOSPHERE SAFETY</h4>";
  if (gVal > GAS_THRESHOLD) {
    html += "<div class='gas-warn'>CRITICAL GAS DETECTED</div>";
    html += "<div class='value' style='color:#ff416c;'>" + String(gVal) + "</div>";
  } else {
    html += "<div style='color:#00ff88; font-weight:bold;'>STATUS: NOMINAL</div>";
    html += "<div class='value'>" + String(gVal) + "</div>";
  }
  html += "</div>";

  String pCol = (pumpStatus == "SYSTEM IDLE") ? "#888" : "#ffcc00";
  html += "<div class='card'><h4>PUMP ACTIVITY</h4>";
  html += "<div style='font-size:24px; color:" + pCol + "; font-weight:bold;'>" + pumpStatus + "</div></div>";

  html += "</body></html>";
  return html;
}

void handleRoot() { server.send(200, "text/html", getHTML()); }

void setup() {
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(PUMP_PIN, LOW);

  Wire.begin(D2, D1); 
  lcd.init();
  lcd.backlight();
  
  WiFi.softAP(ssid, password);
  server.on("/", handleRoot);
  server.begin();

  lcd.setCursor(0,0);
  lcd.print("SYSTEM ONLINE");
  lcd.setCursor(0,1);
  lcd.print("IP: 192.168.4.1");
  delay(2000);
}

void loop() {
  server.handleClient(); 

  // Sensor Reading
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  int distance = (duration == 0) ? PIPE_DEPTH : (duration * 0.034 / 2);
  int gasValue = analogRead(GAS_PIN);
  int sHeight = PIPE_DEPTH - distance;
  if(sHeight < 0) sHeight = 0;

  // Gas Safety Logic
  if (gasValue > GAS_THRESHOLD) {
    if (!gasAlreadyPumped) {
      pumpStatus = "GAS WARNING (2S)";
      digitalWrite(PUMP_PIN, HIGH);
      lcd.clear();
      lcd.setCursor(0,0); lcd.print("GAS ALERT!");
      lcd.setCursor(0,1); lcd.print("PULSING 2 SEC");
      delay(2000); 
      digitalWrite(PUMP_PIN, LOW);
      gasAlreadyPumped = true;
      lcd.clear();
    }
  } else {
    gasAlreadyPumped = false; 
  }

  // Sewage Level Logic
  if (gasValue <= GAS_THRESHOLD) {
    if (distance > 0 && distance <= TRIGGER_DIST) {
      digitalWrite(PUMP_PIN, HIGH);
      pumpStatus = "PUMPING...";
      lcd.setCursor(0,0);
      lcd.print("LVL: HIGH! [ON] ");
    } 
    else {
      digitalWrite(PUMP_PIN, LOW);
      pumpStatus = "SYSTEM IDLE";
      lcd.setCursor(0, 0);
      lcd.print("LVL: SAFE [OFF]");
    }
    
    // LCD Monitor: Shows Sewage Height (H) and Gas (G)
    lcd.setCursor(0, 1);
    lcd.print("H:"); lcd.print(sHeight); lcd.print("cm G:"); lcd.print(gasValue);
    lcd.print("    "); 
  }

  // Battery Safety: If LCD looks crazy, this helps reset it
  static unsigned long lastReset = 0;
  if(millis() - lastReset > 10000) { 
     Wire.beginTransmission(0x27);
     if (Wire.endTransmission() != 0) lcd.init(); 
     lastReset = millis();
  }

  delay(100); 
}
