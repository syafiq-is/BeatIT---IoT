#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>

// ===== WiFi Credentials =====
#define WIFI_SSID     "[WIFI_NAME]"
#define WIFI_PASSWORD "[WIFI_PASSWORD]"

// ===== Firebase Project Credentials =====
#define FIREBASE_HOST "[PROJECT_ID].asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "[DATABASE_SECRET]"

// ===== Pin Definitions =====
#define BUTTON_PIN_1 D2
#define RELAY_PIN_1  D5  // GPIO14
#define LED_PIN      LED_BUILTIN

// ===== Firebase Objects =====
FirebaseData fbdo;
FirebaseData stream;
FirebaseAuth auth;
FirebaseConfig config;

// ===== State Tracking =====
bool lastButtonState = false;
bool lastRelayState  = false;

void connectWiFi() {
  Serial.println("📶 Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 40) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi Failed.");
  }
}

void connectFirebase() {
  Serial.println("🔥 Connecting to Firebase...");

  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (!Firebase.beginStream(stream, "/relay1/state")) {
    Serial.println("❌ Stream start failed:");
    Serial.println(stream.errorReason());
  } else {
    Serial.println("✅ Listening to /relay1/state");
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(BUTTON_PIN_1, INPUT);
  pinMode(RELAY_PIN_1, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // Initialize outputs
  digitalWrite(RELAY_PIN_1, HIGH); // OFF (active LOW)
  digitalWrite(LED_PIN, HIGH);

  connectWiFi();
  connectFirebase();
}

void loop() {
  // ========= 1. Listen to Firebase Realtime DB =========
  if (Firebase.readStream(stream)) {
    if (stream.streamPath() == "/relay1/state" && stream.dataType() == "boolean") {
      bool newRelayState = stream.boolData();

      if (newRelayState != lastRelayState) {
        lastRelayState = newRelayState;
        digitalWrite(RELAY_PIN_1, newRelayState ? LOW : HIGH); // active LOW
        digitalWrite(LED_PIN,     newRelayState ? LOW : HIGH);

        Serial.print("📥 Firebase updated → Relay is ");
        Serial.println(newRelayState ? "ON" : "OFF");
      }
    }
  } else {
    if (stream.httpCode() != 200) {
      Serial.print("❌ Firebase stream error: ");
      Serial.println(stream.errorReason());
    }
  }

  // ========= 2. Check Button Press =========
  bool btnPressed = digitalRead(BUTTON_PIN_1) == HIGH;

  if (btnPressed != lastButtonState) {
    lastButtonState = btnPressed;

    // only act on button press (not release)
    if (btnPressed) {
      Serial.println("🔘 Button pressed → Writing true to Firebase");

      if (Firebase.setBool(fbdo, "/relay1/state", true)) {
        Serial.println("✅ Firebase write: relay1/state = true");
      } else {
        Serial.print("❌ Firebase write failed: ");
        Serial.println(fbdo.errorReason());
      }
    } else {
      Serial.println("🔘 Button not pressed → Writing false to Firebase");

      if (Firebase.setBool(fbdo, "/relay1/state", false)) {
        Serial.println("✅ Firebase write: relay1/state = false");
      } else {
        Serial.print("❌ Firebase write failed: ");
        Serial.println(fbdo.errorReason());
      }
    }
  }

  delay(50); // debounce and pacing
}
