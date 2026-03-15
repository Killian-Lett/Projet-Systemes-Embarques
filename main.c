#include <ChainableLED.h>  
#include "rgb_lcd.h"
#include <Wire.h>
#include "RTClib.h"
#include "DHT.h"
#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <EEPROM.h>

// ============================================================
// PÉRIPHÉRIQUES
// ============================================================
rgb_lcd      lcd;
RTC_DS1307   rtc;
TinyGPSPlus gps;

ChainableLED led(8, 9, 1);
DHT          dht(6, DHT11);
SoftwareSerial gpsSerial(4, 5);  // RX, TX

#define LUMIN_PIN      A1
#define BOUTON_ROUGE   2
#define BOUTON_VERT    3
#define SD_CS          10
#define EEPROM_SIGNATURE 0x42

// ============================================================
// DONNES SYSTEME 
// ============================================================

const unsigned int LCD_REFRESH    = 1000;
const uint8_t DEBOUNCE_DELAY = 50;

struct Variable {
    // Paramètres généraux
    unsigned int DUREE_APPUI_LONG;
    unsigned int CONFIG_TIMEOUT;
    unsigned int LOG_INTERVAL;
    unsigned int FILE_MAX_SIZE;

    bool LUMIN;
    int LUMIN_LOW;
    int LUMIN_HIGH;

    bool TEMP_AIR;
    int MIN_TEMP_AIR;
    int MAX_TEMP_AIR;

    bool HYGR;
    int HYGR_MINT;
    int HYGR_MAXT;

    bool PRESSURE;
    int PRESSURE_MIN;
    int PRESSURE_MAX;
};

// Initialisation des valeurs par défaut
Variable config = {
    3000, 20000, 10000, 4096,   // DUREE_APPUI_LONG, CONFIG_TIMEOUT, LOG_INTERVAL, FILE_MAX_SIZE
    1, 255, 768,          // LUMIN, LUMIN_LOW, LUMIN_HIGH
    1, -10, 60,           // TEMP_AIR, MIN_TEMP_AIR, MAX_TEMP_AIR
    1, 0, 50,             // HYGR, HYGR_MINT, HYGR_MAXT
    1, 850, 1080          // PRESSURE, PRESSURE_MIN, PRESSURE_MAX
};

// ============================================================
// EEPROM
// ============================================================
void sauvegarderEEPROM() {
  EEPROM.update(0, EEPROM_SIGNATURE);
  EEPROM.put(1, config);
}

bool chargerEEPROM() {
  if(EEPROM.read(0) != EEPROM_SIGNATURE){
    Serial.println(F("EEPROM invalide, valeurs par defaut"));
    return false;
  }
  EEPROM.get(1, config);
  return true;
}

// ============================================================
// MODES
// ============================================================
enum ModeCapteur { STANDARD, CONFIGURATION, MAINTENANCE, ECONOMIQUE };
ModeCapteur modeActuel    = STANDARD;
ModeCapteur modePrecedent = STANDARD;

// ============================================================
// ERREURS
// ============================================================
enum TypeErreur {
  ERREUR_AUCUNE,
  ERREUR_RTC,
  ERREUR_GPS,
  ERREUR_CAPTEUR,
  ERREUR_CAPTEUR_INCOHERENT,
  ERREUR_SD_PLEINE,
  ERREUR_SD_ECRITURE
};

TypeErreur erreurActuelle = ERREUR_AUCUNE;
unsigned long lastBlink = 0;
bool etatLED = false;

// ============================================================
// TIMERS
// ============================================================
unsigned long lastMeasure      = 0;
unsigned long lastLCDRefresh   = 0;
unsigned long derniereActivite = 0;

// ============================================================
// BOUTONS
// ============================================================
volatile bool rougeEvent = false;
volatile bool vertEvent  = false;

unsigned long debutRouge        = 0;
unsigned long debutVert         = 0;
unsigned long lastDebounceRouge = 0;
unsigned long lastDebounceVert  = 0;

void ISR_boutonRouge() { rougeEvent = true; }
void ISR_boutonVert()  { vertEvent  = true; }

// ============================================================
// POINTEUR DE FONCTION
// ============================================================
typedef void (*ModeHandler)();
ModeHandler modeHandler = nullptr;

// ============================================================
// UTILITAIRES
// ============================================================
void afficherHeure(Print &out, DateTime t) {
  if (t.hour() < 10) out.print('0'); out.print(t.hour()); out.print(':');
  if (t.minute() < 10) out.print('0'); out.print(t.minute()); out.print(':');
  if (t.second() < 10) out.print('0'); out.print(t.second());
}

void afficherEtEnregistrer(bool ecrireSD) {
  DateTime t = rtc.now();
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();
  int lum    = analogRead(LUMIN_PIN);
  bool dhtOK = !isnan(temp) && !isnan(hum);
  if(!dhtOK) erreurActuelle = ERREUR_CAPTEUR;
  if(!gps.location.isValid()) erreurActuelle = ERREUR_GPS;

  Print &out = Serial;
  afficherHeure(out, t);

  if (dhtOK) {
    out.print(F(" | Temp: ")); out.print(temp);
    out.print(F(" | Hum: ")); out.print(hum);
  } else out.print(F(" | Temp: ERR | Hum: ERR"));

  out.print(F(" | Lum: ")); out.print(lum);

  if (gps.location.isValid()) {
    out.print(F(" | Lat: ")); out.print(gps.location.lat(), 6);
    out.print(F(" | Lon: ")); out.print(gps.location.lng(), 6);
  } else out.print(F(" | GPS: Pas de signal"));

  out.println();

  if(ecrireSD){
    File f = SD.open("meteo.txt", FILE_WRITE);
    if(f){
      afficherHeure(f, t);
      if(dhtOK){ f.print(F(" | Temp: ")); f.print(temp); f.print(F(" | Hum: ")); f.print(hum); }
      else f.print(F(" | Temp: ERR | Hum: ERR"));
      f.print(F(" | Lum: ")); f.print(lum);
      if(gps.location.isValid()){ f.print(F(" | Lat: ")); f.print(gps.location.lat(), 6); f.print(F(" | Lon: ")); f.print(gps.location.lng(), 6); }
      else f.print(F(" | GPS: ERR"));
      f.println(); f.close();
    } else Serial.println(F("Erreur ecriture SD !"));
      erreurActuelle = ERREUR_SD_ECRITURE;
  }
}


// ============================================================
// CONFIGURATION RTC
// ============================================================

void configurerHeure(const char *val){
    int h, m, s;

    if(sscanf(val, "%d:%d:%d", &h, &m, &s) != 3){
        Serial.println(F("Format CLOCK invalide."));
        return;
    }

    if(h<0 || h>23 || m<0 || m>59 || s<0 || s>59){
        Serial.println(F("Valeurs heure invalides."));
        return;
    }

    DateTime now = rtc.now();
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), h, m, s));

    Serial.println(F("Heure mise a jour."));
}

void configurerDate(const char *val){
    int mois, jour, annee;

    if(sscanf(val, "%d,%d,%d", &mois, &jour, &annee) != 3){
        Serial.println(F("Format DATE invalide."));
        return;
    }

    if(mois<1 || mois>12 || jour<1 || jour>31 || annee<2000 || annee>2099){
        Serial.println(F("Valeurs date invalides."));
        return;
    }

    DateTime now = rtc.now();
    rtc.adjust(DateTime(annee, mois, jour, now.hour(), now.minute(), now.second()));

    Serial.println(F("Date mise a jour."));
}

void configurerJour(const char *val){
    int day = -1;

    if(strcmp(val,"MON")==0) day=1;
    else if(strcmp(val,"TUE")==0) day=2;
    else if(strcmp(val,"WED")==0) day=3;
    else if(strcmp(val,"THU")==0) day=4;
    else if(strcmp(val,"FRI")==0) day=5;
    else if(strcmp(val,"SAT")==0) day=6;
    else if(strcmp(val,"SUN")==0) day=0;

    if(day == -1){
        Serial.println(F("Jour invalide."));
        return;
    }

    DateTime now = rtc.now();

    // On conserve la date actuelle mais on force le jour via recalcul RTC
    rtc.adjust(DateTime(now.year(), now.month(), now.day(),
                        now.hour(), now.minute(), now.second()));

    Serial.println(F("Jour de semaine mis a jour."));
}

// ============================================================
// MODE CONFIG 
// ============================================================
void resetParametres() {
    config = {
        3000, 20000, 10000, 4096,   // DUREE_APPUI_LONG, CONFIG_TIMEOUT, LOG_INTERVAL
        1, 255, 768,          // LUMIN, LUMIN_LOW, LUMIN_HIGH
        1, -10, 60,           // TEMP_AIR, MIN_TEMP_AIR, MAX_TEMP_AIR
        1, 0, 50,             // HYGR, HYGR_MINT, HYGR_MAXT
        1, 850, 1080          // PRESSURE, PRESSURE_MIN, PRESSURE_MAX
    };
    sauvegarderEEPROM(); // Sauvegarde dans l'EEPROM
    Serial.println(F("Paramètres réinitialisés par défaut."));
}

void traiterCommande(char *cmd){

    if(strncmp(cmd, "CLOCK ", 6) == 0){configurerHeure(cmd + 6);return;}

    if(strncmp(cmd, "DATE ", 5) == 0){configurerDate(cmd + 5);return;}

    if(strncmp(cmd, "DAY ", 4) == 0){configurerJour(cmd + 4);return;}

    if(strcmp(cmd, "RESET") == 0){resetParametres();return;}

    else if(strcmp(cmd, "VERSION") == 0){Serial.println(F("Station Meteo CESI - Version 1.0"));return;}

    char *sep = strchr(cmd, '=');
    if(sep == NULL) return;

    *sep = '\0';
    char *varName = cmd;
    int valeur = atoi(sep + 1);

    if(strcmp(varName, "LOG_INTERVAL") == 0)          config.LOG_INTERVAL = valeur * 1000;
    else if(strcmp(varName, "DUREE_APPUI_LONG") == 0) config.DUREE_APPUI_LONG = valeur * 1000;
    else if(strcmp(varName, "CONFIG_TIMEOUT") == 0)   config.CONFIG_TIMEOUT = valeur * 1000;
    else if(strcmp(varName, "FILE_MAX_SIZE") == 0)   config.FILE_MAX_SIZE = valeur;

    else if(strcmp(varName, "LUMIN") == 0)            config.LUMIN = valeur != 0;
    else if(strcmp(varName, "LUMIN_LOW") == 0)        config.LUMIN_LOW = valeur;
    else if(strcmp(varName, "LUMIN_HIGH") == 0)       config.LUMIN_HIGH = valeur;

    else if(strcmp(varName, "TEMP_AIR") == 0)         config.TEMP_AIR = valeur != 0;
    else if(strcmp(varName, "MIN_TEMP_AIR") == 0)     config.MIN_TEMP_AIR = valeur;
    else if(strcmp(varName, "MAX_TEMP_AIR") == 0)     config.MAX_TEMP_AIR = valeur;

    else if(strcmp(varName, "HYGR") == 0)             config.HYGR = valeur != 0;
    else if(strcmp(varName, "HYGR_MINT") == 0)        config.HYGR_MINT = valeur;
    else if(strcmp(varName, "HYGR_MAXT") == 0)        config.HYGR_MAXT = valeur;

    else if(strcmp(varName, "PRESSURE") == 0)         config.PRESSURE = valeur != 0;
    else if(strcmp(varName, "PRESSURE_MIN") == 0)     config.PRESSURE_MIN = valeur;
    else if(strcmp(varName, "PRESSURE_MAX") == 0)     config.PRESSURE_MAX = valeur;

    else{
        Serial.println(F("Commande inconnue"));
    }

    sauvegarderEEPROM();

    Serial.print(varName);
    Serial.println(F(" modifications appliquees"));
}

// ============================================================
// MODES
// ============================================================
int compteurEco = 0;

void setLEDMode() {
  switch(modeActuel){
    case STANDARD:      led.setColorRGB(0, 0, 100, 0); break;     // vert 
    case CONFIGURATION: led.setColorRGB(0, 100, 100, 100); break; // blanc
    case MAINTENANCE:   led.setColorRGB(0, 100, 50, 0); break;    // jaune
    case ECONOMIQUE:    led.setColorRGB(0, 0, 0, 100); break;     // bleu
  }
}

void modeStandard() {
  setLEDMode();
  if(millis() - lastMeasure >= config.LOG_INTERVAL){
    lastMeasure = millis();
    afficherEtEnregistrer(true);
  }
}

void modeConfiguration() {
  setLEDMode();
  if(Serial.available()){
    char commande[40];
    Serial.readBytesUntil('\n', commande, sizeof(commande));
    traiterCommande(commande);
    derniereActivite = millis();
  }
}

void modeMaintenance() {
  setLEDMode();
  if(millis() - lastMeasure >= config.LOG_INTERVAL){
    lastMeasure = millis();
    afficherEtEnregistrer(false);
  }
}

void modeEconomique() {
  setLEDMode();
  unsigned long now = millis();
  if(now - lastMeasure >= config.LOG_INTERVAL*2){
    lastMeasure = now;
    compteurEco++;
    if(compteurEco % 2 == 0){
      Serial.println(F("GPS ACTIF"));
      while(gpsSerial.available()) gps.encode(gpsSerial.read());
    } else Serial.println(F("GPS DESACTIVE"));
    afficherEtEnregistrer(true);
  }
}

void gererLEDErreur() {
  if(erreurActuelle == ERREUR_AUCUNE) return;

  unsigned long interval = 4000; // durée d'une couleur en ms
  if(erreurActuelle == ERREUR_CAPTEUR_INCOHERENT || erreurActuelle == ERREUR_SD_ECRITURE){
    interval = 8000;
  }

  if(millis() - lastBlink >= interval){
    lastBlink = millis();

    if(erreurActuelle == ERREUR_CAPTEUR_INCOHERENT || erreurActuelle == ERREUR_SD_ECRITURE){
        etatLED = !etatLED; // couleur longue
    } else {
        etatLED = !etatLED; // couleur normale
    }
}

  switch(erreurActuelle){
    case ERREUR_RTC:
      if(etatLED) led.setColorRGB(0,255,0,0);   // rouge
      else          led.setColorRGB(0,0,0,255); // bleu
      break;

    case ERREUR_GPS:
      if(etatLED) led.setColorRGB(0,255,0,0);   // rouge
      else          led.setColorRGB(0,255,255,0); // jaune
      break;

    case ERREUR_CAPTEUR:
      if(etatLED) led.setColorRGB(0,255,0,0);   // rouge
      else          led.setColorRGB(0,0,255,0); // vert
      break;

    case ERREUR_CAPTEUR_INCOHERENT:
      if(etatLED) led.setColorRGB(0,255,0,0);
      else          led.setColorRGB(0,0,255,0); // vert (2x long)
      break;

    case ERREUR_SD_PLEINE:
      if(etatLED) led.setColorRGB(0,255,0,0);
      else          led.setColorRGB(0,255,255,255); // blanc
      break;

    case ERREUR_SD_ECRITURE:
      if(etatLED) led.setColorRGB(0,255,0,0);
      else          led.setColorRGB(0,255,255,255); // blanc (2x long)
      break;

    default:
      break;
  }
}

void updateModeHandler(){
  switch(modeActuel){
    case STANDARD: modeHandler = &modeStandard; break;
    case CONFIGURATION: modeHandler = &modeConfiguration; break;
    case MAINTENANCE: modeHandler = &modeMaintenance; break;
    case ECONOMIQUE: modeHandler = &modeEconomique; break;
  }
}

// ============================================================
// LCD
// ============================================================
void afficherLCD(){
  unsigned long now = millis();
  if(now - lastLCDRefresh < LCD_REFRESH) return;
  lastLCDRefresh = now;

  DateTime t = rtc.now();
  lcd.setCursor(0,0); afficherHeure(lcd,t);
  lcd.setCursor(0,1);
  switch(modeActuel){
    case STANDARD:      lcd.print(F("Mode: STANDARD  ")); break;
    case CONFIGURATION: lcd.print(F("Mode: CONFIG    "));   break;
    case MAINTENANCE:   lcd.print(F("Mode: MAINT     "));   break;
    case ECONOMIQUE:    lcd.print(F("Mode: ECO       "));     break;
  }
}

// ============================================================
// BOUTONS GÉNÉRIQUES
// ============================================================
void traiterBouton(uint8_t pin, volatile bool &eventFlag, unsigned long &debut, unsigned long &lastDebounce, void (*actionCourte)(), void (*actionLongue)()){
  if(!eventFlag) return;
  eventFlag = false;

  unsigned long now = millis();
  if(now - lastDebounce < DEBOUNCE_DELAY) return;
  lastDebounce = now;

  bool etat = digitalRead(pin);
  if(etat == LOW) debut = now;
  else {
    unsigned long duree = now - debut;
    if(duree >= config.DUREE_APPUI_LONG && actionLongue) actionLongue();
    else if(duree < config.DUREE_APPUI_LONG && actionCourte) actionCourte();
  }
}

// ============================================================
// ACTIONS BOUTONS
// ============================================================
void rougeCourte(){ modeActuel = CONFIGURATION; derniereActivite = millis(); lastLCDRefresh=0; updateModeHandler(); }
void rougeLongue(){
  if(modeActuel==STANDARD){ modePrecedent=STANDARD; modeActuel=MAINTENANCE; lastLCDRefresh=0; }
  else if(modeActuel==ECONOMIQUE){ modeActuel=STANDARD; lastLCDRefresh=0; }
  else if(modeActuel==MAINTENANCE){ modeActuel=modePrecedent; lastLCDRefresh=0; }
  updateModeHandler();
}
void vertLongue(){
  if(modeActuel==STANDARD){ modePrecedent=STANDARD; modeActuel=ECONOMIQUE; lastLCDRefresh=0; updateModeHandler(); }
}

// ============================================================
// SETUP
// ============================================================
void setup(){
  Serial.begin(9600);
  gpsSerial.begin(9600);
  Wire.begin();

  // ===== RTC =====
  if(!rtc.begin()) Serial.println(F("Erreur RTC !"));
  if(!rtc.isrunning()){ rtc.adjust(DateTime(F(__DATE__),F(__TIME__))); }

  // ===== Capteurs et LCD =====
  dht.begin();
  pinMode(LUMIN_PIN, INPUT);
  pinMode(BOUTON_ROUGE, INPUT_PULLUP);
  pinMode(BOUTON_VERT, INPUT_PULLUP);

  lcd.begin(16,2); lcd.setRGB(0,255,0);

  // ===== Carte SD =====
  if(!SD.begin(SD_CS)) Serial.println(F("Erreur carte SD !"));
  else{
    Serial.println(F("Carte SD OK"));
    if(SD.exists("meteo.txt")) { SD.remove("meteo.txt"); Serial.println(F("Anciennes donnees effacees")); }
  }

  // ===== Interruptions boutons =====
  attachInterrupt(digitalPinToInterrupt(BOUTON_ROUGE), ISR_boutonRouge, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BOUTON_VERT),  ISR_boutonVert,  CHANGE);

  // ===== Charger EEPROM =====
  chargerEEPROM();

  // ===== Mode initial =====
  modeActuel = STANDARD;
  updateModeHandler();
  Serial.println(F("Systeme demarre."));
}

// ============================================================
// LOOP
// ============================================================
void loop(){
  unsigned long now = millis();

  while(gpsSerial.available()) gps.encode(gpsSerial.read());

  traiterBouton(BOUTON_ROUGE, rougeEvent, debutRouge, lastDebounceRouge, rougeCourte, rougeLongue);
  traiterBouton(BOUTON_VERT,  vertEvent,  debutVert,  lastDebounceVert,  nullptr, vertLongue);

  if(modeActuel==CONFIGURATION && now-derniereActivite>=config.CONFIG_TIMEOUT){
    modeActuel=STANDARD; updateModeHandler(); lastLCDRefresh=0;
  }

  if(modeHandler) modeHandler();
  afficherLCD();
  if(erreurActuelle != ERREUR_AUCUNE) gererLEDErreur();
  delay(50);
}
