#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WebServer.h>
#include <vector>

// === Pin Definitions ===
#define SS_PIN    5
#define RST_PIN   22
#define BUZZER    13
#define SDA_PIN   26
#define SCL_PIN   27

// === Wi-Fi Credentials ===
const char* ssid = "Ohh Just";
const char* password = "12345678";

MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer server(80);

struct Item {
  String uid;
  String name;
  float price;
};

Item itemList[] = {
  {"F394E30D", "Milk", 30},
  {"B3DF9214", "Bread", 25},
  {"9347DF0D", "Eggs", 50},
  {"D345D00D", "Cake", 150},
};

String checkoutUID = "332DFB03";
String masterUID = "";
bool removeMode = false;
std::vector<Item> cart;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  pinMode(BUZZER, OUTPUT);

  SPI.begin();
  rfid.PCD_Init();

  lcd.setCursor(0, 0);
  lcd.print("Smart Trolley");
  lcd.setCursor(0, 1);
  lcd.print("Scan to Start");

WiFi.begin(ssid, password);
Serial.println("Connecting to Wi-Fi...");

unsigned long startAttemptTime = millis();
while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
  delay(500);
  Serial.print(".");
}

if (WiFi.status() != WL_CONNECTED) {
  Serial.println("Failed to connect to Wi-Fi.");
} else {
  Serial.println("\nConnected to Wi-Fi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

  server.on("/", handleRoot);
  server.begin();
}

void loop() {
  server.handleClient();

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;
  String uid = getUID();
  Serial.println("Scanned: " + uid);
  beep();

  if (masterUID == "") {
    masterUID = uid;
    lcd.clear();
    lcd.print("Master Set");
    Serial.println("Master card set: " + uid);
    delay(2000);
    showPrompt();
    return;
  }

  if (uid == masterUID) {
    removeMode = !removeMode;
    lcd.clear();
    lcd.print(removeMode ? "Remove Mode" : "Add Mode");
    Serial.println(removeMode ? "Switched to REMOVE mode" : "Switched to ADD mode");
    delay(2000);
    showPrompt();
    return;
  }

  if (uid == checkoutUID) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Checkout Done");
    delay(2000);
    showBill();
    delay(5000);
    cart.clear();
    lcd.clear();
    lcd.print("Cart Cleared");
    delay(2000);
    showPrompt();
    return;
  }

  Item* found = findItem(uid);
  if (found != nullptr) {
    if (removeMode) {
      if (removeItem(uid)) {
        lcd.clear();
        lcd.print("Removed:");
        lcd.setCursor(0, 1);
        lcd.print(found->name);
        Serial.println("Removed: " + found->name);
      } else {
        lcd.clear();
        lcd.print("Not in cart");
        lcd.setCursor(0, 1);
        lcd.print(found->name);
      }
    } else {
      cart.push_back(*found);
      lcd.clear();
      lcd.print("Added:");
      lcd.setCursor(0, 1);
      lcd.print(found->name);
      Serial.println("Added: " + found->name + " | Rs. " + String(found->price));
    }
  } else {
    lcd.clear();
    lcd.print("Unknown Tag");
    Serial.println("Unknown UID: " + uid);
  }

  delay(2000);
  showPrompt();
  rfid.PICC_HaltA();
}

void showPrompt() {
  lcd.clear();
  lcd.print(removeMode ? "Tap to Remove" : "Scan Item");
}

void showBill() {
  lcd.clear();
  lcd.print("Total: Rs. " + String(getTotal()));
  Serial.println("=== FINAL BILL ===");
  for (Item i : cart) {
    Serial.println(i.name + " - Rs. " + String(i.price));
  }
  Serial.println("TOTAL: Rs. " + String(getTotal()));
}

String getUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

Item* findItem(String uid) {
  for (int i = 0; i < sizeof(itemList) / sizeof(itemList[0]); i++) {
    if (itemList[i].uid == uid) return &itemList[i];
  }
  return nullptr;
}

bool removeItem(String uid) {
  for (size_t i = 0; i < cart.size(); i++) {
    if (cart[i].uid == uid) {
      cart.erase(cart.begin() + i);
      return true;
    }
  }
  return false;
}

float getTotal() {
  float total = 0;
  for (Item i : cart) total += i.price;
  return total;
}


void handleRoot() {
  String html = "<html><head>";
  html += "<meta http-equiv='refresh' content='3'>";
  html += "<title>Smart Trolley</title>";
  html += "<style>";
  html += "body { font-family: Arial; background: #f4f4f4; padding: 20px; }";
  html += "h2 { color: #333; }";
  html += "ul { padding-left: 20px; }";
  html += "li { background: #fff; margin: 5px 0; padding: 10px; border-radius: 5px; }";
  html += "p { font-weight: bold; }";
  html += "</style>";
  html += "</head><body>";

  html += "<h2>Shopping Cart</h2><ul>";
  for (Item i : cart) {
    html += "<li>" + i.name + ": Rs. " + String(i.price, 2) + "</li>";
  }
  html += "</ul>";

  html += "<p>Total: Rs. " + String(getTotal(), 2) + "</p>";
  html += "<p>Items in cart: " + String(cart.size()) + "</p>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}



void beep() {
  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(BUZZER, LOW);
}