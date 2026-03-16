#include <ChainableLED.h>
#include "rgb_lcd.h"
#include <Wire.h>
#include "DHT.h"
#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

// ============================================================
// PÉRIPHÉRIQUES
// ============================================================
rgb_lcd        lcd;
ChainableLED   led(8, 9, 1);
DHT            dht(6, DHT11);
SoftwareSerial gpsSerial(4, 5);  // RX, TX

#define LUMIN_PIN        A1
#define BOUTON_ROUGE     2
#define BOUTON_VERT      3
#define SD_CS            10
#define RTC_ADDR         0x68
#define EEPROM_SIGNATURE 0x42

// ============================================================
// RTC DS1307 — sans bibliothèque
// Communication I2C directe, encodage BCD
// ============================================================
struct DateTime { uint8_t h, m, s; };

static inline uint8_t bcdToDec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static inline uint8_t decToBcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

// Lecture heure courante depuis le DS1307
DateTime rtcNow() {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)RTC_ADDR, (uint8_t)3);
  DateTime dt;
  dt.s = bcdToDec(Wire.read() & 0x7F); // masque bit CH
  dt.m = bcdToDec(Wire.read());
  dt.h = bcdToDec(Wire.read() & 0x3F); // mode 24h
  return dt;
}

// Initialisation RTC avec la date/heure de compilation
void rtcAdjust() {
  uint8_t h = (__TIME__[0]-'0')*10 + (__TIME__[1]-'0');
  uint8_t m = (__TIME__[3]-'0')*10 + (__TIME__[4]-'0');
  uint8_t s = (__TIME__[6]-'0')*10 + (__TIME__[7]-'0');

  uint8_t day = (__DATE__[4] == ' ' ? 0 : __DATE__[4]-'0') * 10 + (__DATE__[5]-'0');
  const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  uint8_t month = 1;
  for (uint8_t i = 0; i < 12; i++) {
    if (strncmp(__DATE__, months + i*3, 3) == 0) { month = i + 1; break; }
  }
  uint8_t year = (__DATE__[9]-'0') * 10 + (__DATE__[10]-'0');

  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x00);
  Wire.write(decToBcd(s));   // CH=0 : démarre l'horloge
  Wire.write(decToBcd(m));
  Wire.write(decToBcd(h));
  Wire.write(0x01);           // jour de semaine (arbitraire)
  Wire.write(decToBcd(day));
  Wire.write(decToBcd(month));
  Wire.write(decToBcd(year));
  Wire.endTransmission();
}

// Vérifie si le DS1307 tourne (bit CH = 0)
bool rtcRunning() {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)RTC_ADDR, (uint8_t)1);
  return !(Wire.read() & 0x80);
}

// ============================================================
// GPS NMEA — sans TinyGPS++
// Parseur minimal de trames $GPRMC / $GNRMC
// ============================================================
struct GPSData { double lat, lon; bool valid; };
static GPSData gpsData = { 0.0, 0.0, false };

static char    nmeaBuf[90];
static uint8_t nmeaIdx = 0;

// Convertit coordonnée NMEA (DDDMM.MMMM) en degrés décimaux
static double nmea2deg(const char* s) {
  double raw = atof(s);
  int    deg = (int)(raw / 100.0);
  return deg + (raw - deg * 100.0) / 60.0;
}

// Parse une trame GPRMC/GNRMC et met à jour gpsData
static void parseNMEA(char* buf) {
  if (strncmp(buf, "$GPRMC", 6) != 0 && strncmp(buf, "$GNRMC", 6) != 0) return;

  // Découpe la trame sur ',' et '*' (modification in-place du buffer)
  char* f[9]; uint8_t n = 0;
  for (char* p = buf; *p && n < 9; p++) {
    f[n++] = p;
    while (*p && *p != ',' && *p != '*') p++;
    if (*p) *p = '\0'; else break;
  }
  if (n < 7) return;

  gpsData.valid = (f[2][0] == 'A');
  if (gpsData.valid) {
    gpsData.lat = nmea2deg(f[3]); if (f[4][0] == 'S') gpsData.lat = -gpsData.lat;
    gpsData.lon = nmea2deg(f[5]); if (f[6][0] == 'W') gpsData.lon = -gpsData.lon;
  }
}

// Lit les octets disponibles sur le port série GPS
void readGPS() {
  while (gpsSerial.available()) {
    char c = (char)gpsSerial.read();
    if (c == '$')             nmeaIdx = 0;
    if (nmeaIdx < sizeof(nmeaBuf) - 1) nmeaBuf[nmeaIdx++] = c;
    if (c == '\n' || c == '\r') {
      nmeaBuf[nmeaIdx] = '\0';
      if (nmeaIdx > 6) parseNMEA(nmeaBuf);
      nmeaIdx = 0;
    }
  }
}

// ============================================================
// CONFIG & EEPROM
// ============================================================
struct Variable {
  unsigned int DUREE_APPUI_LONG, CONFIG_TIMEOUT, LOG_INTERVAL;
  bool LUMIN;    int LUMIN_LOW,    LUMIN_HIGH;
  bool TEMP_AIR; int MIN_TEMP_AIR, MAX_TEMP_AIR;
  bool HYGR;     int HYGR_MINT,    HYGR_MAXT;
  bool PRESSURE; int PRESSURE_MIN, PRESSURE_MAX;
};

static const Variable CONFIG_DEFAULT PROGMEM = {
  3000, 20000, 10000,
  true,  255,  768,
  true,  -10,  60,
  true,  0,    50,
  true,  850,  1080
};

Variable config;

void sauvegarderEEPROM() {
  EEPROM.update(0, EEPROM_SIGNATURE);
  EEPROM.put(1, config);
}

void chargerEEPROM() {
  if (EEPROM.read(0) != EEPROM_SIGNATURE) {
    memcpy_P(&config, &CONFIG_DEFAULT, sizeof(config));
    Serial.println(F("EEPROM invalide, valeurs par defaut"));
  } else {
    EEPROM.get(1, config);
  }
}

// ============================================================
// MODES
// ============================================================
enum ModeCapteur : uint8_t { STANDARD, CONFIGURATION, MAINTENANCE, ECONOMIQUE };
ModeCapteur modeActuel    = STANDARD;
ModeCapteur modePrecedent = STANDARD;

// ============================================================
// TIMERS / ÉTATS
// ============================================================
const uint16_t LCD_REFRESH    = 1000;
const uint8_t  DEBOUNCE_DELAY = 50;

unsigned long lastMeasure      = 0;
unsigned long lastLCDRefresh   = 0;
unsigned long derniereActivite = 0;

// ============================================================
// BOUTONS (interruptions)
// ============================================================
volatile bool rougeEvent = false;
volatile bool vertEvent  = false;
unsigned long debutRouge = 0, lastDebounceRouge = 0;
unsigned long debutVert  = 0, lastDebounceVert  = 0;

void ISR_boutonRouge() { rougeEvent = true; }
void ISR_boutonVert()  { vertEvent  = true; }

// ============================================================
// UTILITAIRES AFFICHAGE
// ============================================================
static void printPad2(Print &out, uint8_t v) {
  if (v < 10) out.print('0');
  out.print(v);
}

void afficherHeure(Print &out, const DateTime &t) {
  printPad2(out, t.h); out.print(':');
  printPad2(out, t.m); out.print(':');
  printPad2(out, t.s);
}

void afficherEtEnregistrer(bool ecrireSD) {
  DateTime t   = rtcNow();
  float temp   = dht.readTemperature();
  float hum    = dht.readHumidity();
  int   lum    = analogRead(LUMIN_PIN);
  bool  dhtOK  = !isnan(temp) && !isnan(hum);

  // --- Série ---
  afficherHeure(Serial, t);
  if (dhtOK) { Serial.print(F(" | T:")); Serial.print(temp, 1); Serial.print(F("C | H:")); Serial.print(hum, 1); Serial.print('%'); }
  else          Serial.print(F(" | T:ERR | H:ERR"));
  Serial.print(F(" | L:")); Serial.print(lum);
  if (gpsData.valid) { Serial.print(F(" | Lat:")); Serial.print(gpsData.lat, 6); Serial.print(F(" Lon:")); Serial.print(gpsData.lon, 6); }
  else                 Serial.print(F(" | GPS:-"));
  Serial.println();

  // --- Carte SD ---
  if (!ecrireSD) return;
  File f = SD.open("meteo.txt", FILE_WRITE);
  if (f) {
    afficherHeure(f, t);
    if (dhtOK) { f.print(F(" | T:")); f.print(temp, 1); f.print(F("C | H:")); f.print(hum, 1); f.print('%'); }
    else          f.print(F(" | T:ERR | H:ERR"));
    f.print(F(" | L:")); f.print(lum);
    if (gpsData.valid) { f.print(F(" | Lat:")); f.print(gpsData.lat, 6); f.print(F(" Lon:")); f.print(gpsData.lon, 6); }
    else                 f.print(F(" | GPS:-"));
    f.println(); f.close();
  } else Serial.println(F("Err SD!"));
}

// ============================================================
// MODE CONFIG — Traitement des commandes série
// ============================================================
void resetParametres() {
  memcpy_P(&config, &CONFIG_DEFAULT, sizeof(config));
  sauvegarderEEPROM();
  Serial.println(F("Parametres reinitialises."));
}

void traiterCommande(String &cmd) {
  cmd.trim();
  if (cmd == F("RESET"))   { resetParametres(); return; }
  if (cmd == F("VERSION")) { Serial.println(F("Station Meteo v1.0")); return; }

  int sep = cmd.indexOf('=');
  if (sep < 0) { Serial.println(F("Syntaxe: CLE=VALEUR")); return; }

  String var = cmd.substring(0, sep);
  int    val = cmd.substring(sep + 1).toInt();

  if      (var == F("LOG_INTERVAL"))     config.LOG_INTERVAL     = (unsigned int)val * 1000;
  else if (var == F("DUREE_APPUI_LONG")) config.DUREE_APPUI_LONG = (unsigned int)val * 1000;
  else if (var == F("CONFIG_TIMEOUT"))   config.CONFIG_TIMEOUT   = (unsigned int)val * 1000;
  else if (var == F("LUMIN"))            config.LUMIN            = val != 0;
  else if (var == F("LUMIN_LOW"))        config.LUMIN_LOW        = val;
  else if (var == F("LUMIN_HIGH"))       config.LUMIN_HIGH       = val;
  else if (var == F("TEMP_AIR"))         config.TEMP_AIR         = val != 0;
  else if (var == F("MIN_TEMP_AIR"))     config.MIN_TEMP_AIR     = val;
  else if (var == F("MAX_TEMP_AIR"))     config.MAX_TEMP_AIR     = val;
  else if (var == F("HYGR"))             config.HYGR             = val != 0;
  else if (var == F("HYGR_MINT"))        config.HYGR_MINT        = val;
  else if (var == F("HYGR_MAXT"))        config.HYGR_MAXT        = val;
  else if (var == F("PRESSURE"))         config.PRESSURE         = val != 0;
  else if (var == F("PRESSURE_MIN"))     config.PRESSURE_MIN     = val;
  else if (var == F("PRESSURE_MAX"))     config.PRESSURE_MAX     = val;
  else { Serial.println(F("Commande inconnue")); return; }

  sauvegarderEEPROM();
  Serial.print(var); Serial.println(F(" applique"));
}

// ============================================================
// GESTION DES MODES
// ============================================================
typedef void (*ModeHandler)();
static ModeHandler modeHandler = nullptr;
static uint8_t compteurEco = 0;

void setLEDMode() {
  switch (modeActuel) {
    case STANDARD:      led.setColorRGB(0,   0, 100,   0); break; // vert
    case CONFIGURATION: led.setColorRGB(0, 100, 100, 100); break; // blanc
    case MAINTENANCE:   led.setColorRGB(0, 100,  50,   0); break; // jaune
    case ECONOMIQUE:    led.setColorRGB(0,   0,   0, 100); break; // bleu
  }
}

void modeStandard() {
  setLEDMode();
  if (millis() - lastMeasure >= config.LOG_INTERVAL) {
    lastMeasure = millis();
    afficherEtEnregistrer(true);
  }
}

void modeConfiguration() {
  setLEDMode();
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    derniereActivite = millis();
    traiterCommande(cmd);
  }
}

void modeMaintenance() {
  setLEDMode();
  if (millis() - lastMeasure >= config.LOG_INTERVAL) {
    lastMeasure = millis();
    afficherEtEnregistrer(false); // pas d'écriture SD en maintenance
  }
}

void modeEconomique() {
  setLEDMode();
  unsigned long now = millis();
  if (now - lastMeasure >= (unsigned long)config.LOG_INTERVAL * 2) {
    lastMeasure = now;
    // Alterne GPS actif / inactif
    if (++compteurEco & 1) Serial.println(F("GPS OFF"));
    else                   Serial.println(F("GPS ON"));
    afficherEtEnregistrer(true);
  }
}

void updateModeHandler() {
  switch (modeActuel) {
    case STANDARD:      modeHandler = modeStandard;      break;
    case CONFIGURATION: modeHandler = modeConfiguration; break;
    case MAINTENANCE:   modeHandler = modeMaintenance;   break;
    case ECONOMIQUE:    modeHandler = modeEconomique;    break;
  }
}

// ============================================================
// LCD
// ============================================================
void afficherLCD() {
  if (millis() - lastLCDRefresh < LCD_REFRESH) return;
  lastLCDRefresh = millis();

  DateTime t = rtcNow();
  lcd.setCursor(0, 0);
  afficherHeure(lcd, t);

  lcd.setCursor(0, 1);
  switch (modeActuel) {
    case STANDARD:      lcd.print(F("Mode: STANDARD  ")); break;
    case CONFIGURATION: lcd.print(F("Mode: CONFIG    ")); break;
    case MAINTENANCE:   lcd.print(F("Mode: MAINT     ")); break;
    case ECONOMIQUE:    lcd.print(F("Mode: ECO       ")); break;
  }
}

// ============================================================
// BOUTONS — traitement générique
// ============================================================
void traiterBouton(uint8_t pin, volatile bool &evt, unsigned long &debut,
                   unsigned long &lastDB, void(*court)(), void(*longp)()) {
  if (!evt) return;
  evt = false;
  unsigned long now = millis();
  if (now - lastDB < DEBOUNCE_DELAY) return;
  lastDB = now;

  if (digitalRead(pin) == LOW) { debut = now; }
  else {
    unsigned long dur = now - debut;
    if (dur >= config.DUREE_APPUI_LONG) { if (longp)  longp(); }
    else                                { if (court)  court(); }
  }
}

// --- Actions bouton rouge ---
void rougeCourte() {
  modeActuel = CONFIGURATION;
  derniereActivite = millis();
  lastLCDRefresh = 0;
  updateModeHandler();
}
void rougeLongue() {
  switch (modeActuel) {
    case STANDARD:    modePrecedent = STANDARD; modeActuel = MAINTENANCE; break;
    case ECONOMIQUE:  modeActuel = STANDARD;                              break;
    case MAINTENANCE: modeActuel = modePrecedent;                         break;
    default: break;
  }
  lastLCDRefresh = 0;
  updateModeHandler();
}

// --- Actions bouton vert ---
void vertLongue() {
  if (modeActuel == STANDARD) {
    modePrecedent = STANDARD;
    modeActuel    = ECONOMIQUE;
    lastLCDRefresh = 0;
    updateModeHandler();
  }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(9600);
  gpsSerial.begin(9600);
  Wire.begin();

  // RTC : vérifie le bit CH (horloge arrêtée si = 1)
  if (!rtcRunning()) {
    Serial.println(F("RTC arrete, init..."));
    rtcAdjust();
  }

  dht.begin();
  pinMode(LUMIN_PIN,    INPUT);
  pinMode(BOUTON_ROUGE, INPUT_PULLUP);
  pinMode(BOUTON_VERT,  INPUT_PULLUP);

  lcd.begin(16, 2);
  lcd.setRGB(0, 255, 0);

  if (!SD.begin(SD_CS)) {
    Serial.println(F("Err SD!"));
  } else {
    Serial.println(F("SD OK"));
    if (SD.exists("meteo.txt")) { SD.remove("meteo.txt"); Serial.println(F("Log efface")); }
  }

  attachInterrupt(digitalPinToInterrupt(BOUTON_ROUGE), ISR_boutonRouge, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BOUTON_VERT),  ISR_boutonVert,  CHANGE);

  chargerEEPROM();
  modeActuel = STANDARD;
  updateModeHandler();
  Serial.println(F("Systeme demarre."));
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  unsigned long now = millis();

  readGPS(); // lecture permanente du buffer GPS

  traiterBouton(BOUTON_ROUGE, rougeEvent, debutRouge, lastDebounceRouge, rougeCourte, rougeLongue);
  traiterBouton(BOUTON_VERT,  vertEvent,  debutVert,  lastDebounceVert,  nullptr,     vertLongue);

  // Timeout config → retour standard
  if (modeActuel == CONFIGURATION && now - derniereActivite >= config.CONFIG_TIMEOUT) {
    modeActuel = STANDARD;
    lastLCDRefresh = 0;
    updateModeHandler();
  }

  if (modeHandler) modeHandler();
  afficherLCD();

  delay(50);
}
