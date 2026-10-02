#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TM1637Display.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include "PCF8574.h"

// --- CONFIGURATION DES BROCHES ---
#define CLK_PIN 2   // TM1637 CLK
#define DIO_PIN 3   // TM1637 DIO
#define RX_PIN 10   // DFPlayer TX -> Arduino D10 (via rés. 1k)
#define TX_PIN 11   // DFPlayer RX -> Arduino D11

// --- ADRESSES I2C ---
#define LCD_ADDR 0x27
#define PCF_ADDR 0x20

// --- OBJETS ---
LiquidCrystal_I2C lcd(LCD_ADDR, 20, 4);
TM1637Display tm(CLK_PIN, DIO_PIN);
SoftwareSerial mySoftwareSerial(RX_PIN, TX_PIN);
DFRobotDFPlayerMini dfPlayer;
PCF8574 pcf(PCF_ADDR);

// --- CLAVIER 4x4 ---
const char keys[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// --- VARIABLES DE JEU ---
String secretCode = "1234";   
String currentInput = "";     
int timeMinutes = 10;         
unsigned long totalSeconds = 0;

enum GameState { SETUP_CODE, SETUP_TIME, ARMED, DEFUSED, EXPLODED };
GameState currentState = SETUP_CODE;

unsigned long lastTickTime = 0;
unsigned long lastKeyPressTime = 0;

char lastKeyDetected = 0; 

// --- FONCTION LECTURE CLAVIER AVEC ANTI-MAINTIEN ---
char getKey() {
  char currentPressedKey = 0;

  for (int col = 0; col < 4; col++) {
    for (int i = 0; i < 8; i++) pcf.write(i, HIGH);
    pcf.write(col + 4, LOW);

    for (int row = 0; row < 4; row++) {
      if (pcf.read(row) == LOW) {
        currentPressedKey = keys[row][col];
        break;
      }
    }
    if (currentPressedKey != 0) break;
  }

  if (currentPressedKey == 0) {
    lastKeyDetected = 0;
    return 0;
  }

  if (currentPressedKey == lastKeyDetected) {
    return 0; 
  }

  lastKeyDetected = currentPressedKey;
  delay(50);
  return currentPressedKey;
}

// --- MISE A JOUR AFFICHAGE TM1637 (MM:SS) ---
void updateTM1637(unsigned long seconds) {
  int mins = seconds / 60;
  int secs = seconds % 60;
  int displayValue = (mins * 100) + secs;
  tm.showNumberDecEx(displayValue, 0b01000000, true);
}

// --- AFFICHER LA LEGENDE SUR LA LIGNE 4 (LIGNE INDEX 3) ---
void printFooter() {
  lcd.setCursor(0, 3);
  lcd.print("*:Clear     #:OK    ");
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // Initialisation LCD & TM1637
  lcd.init();
  lcd.backlight();
  tm.setBrightness(7);

  // Initialisation PCF8574
  if (!pcf.begin()) {
    lcd.setCursor(0, 0);
    lcd.print("Erreur PCF8574 !");
    while (1);
  }

  // Initialisation Audio
  mySoftwareSerial.begin(9600);
  if (dfPlayer.begin(mySoftwareSerial)) {
    dfPlayer.volume(22);
  }

  // Ecran d'accueil Setup Code
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("=== BOMB SETUP ===");
  lcd.setCursor(0, 1);
  lcd.print("Entrer Code (4):");
  lcd.setCursor(0, 2);
  lcd.print("Code: ");
  printFooter();
}

void loop() {
  char key = getKey();

  switch (currentState) {

    // 1. CONFIGURATION DU CODE DE DESAMORCAGE
    case SETUP_CODE:
      if (key >= '0' && key <= '9') {
        if (currentInput.length() < 4) {
          currentInput += key;
          dfPlayer.play(1);
          lcd.setCursor(6 + currentInput.length() - 1, 2);
          lcd.print(key);
        }
      } else if (key == '#') {
        if (currentInput.length() == 4) {
          secretCode = currentInput;
          currentInput = "";
          currentState = SETUP_TIME;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("=== BOMB SETUP ===");
          lcd.setCursor(0, 1);
          lcd.print("Temps (Min 1-99):");
          lcd.setCursor(0, 2);
          lcd.print("Min: ");
          printFooter();
        }
      } else if (key == '*') {
        currentInput = "";
        lcd.setCursor(6, 2);
        lcd.print("    ");
      }
      break;

    // 2. CONFIGURATION DU TEMPS DU COMPTE A REBOURS
    case SETUP_TIME:
      if (key >= '0' && key <= '9') {
        if (currentInput.length() < 2) {
          currentInput += key;
          dfPlayer.play(1);
          lcd.setCursor(5 + currentInput.length() - 1, 2);
          lcd.print(key);
        }
      } else if (key == '#') {
        if (currentInput.length() > 0) {
          timeMinutes = currentInput.toInt();
          if (timeMinutes < 1) timeMinutes = 1;
          totalSeconds = (unsigned long)timeMinutes * 60;
          currentInput = "";
          
          currentState = ARMED;
          lastTickTime = millis();
          lastKeyPressTime = millis();
          
          dfPlayer.play(2);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("!!! BOMB ARMEE !!!");
          lcd.setCursor(0, 1);
          lcd.print("Saisir code:");
          lcd.setCursor(0, 2);
          lcd.print("Code: ____");
          printFooter();
        }
      } else if (key == '*') {
        currentInput = "";
        lcd.setCursor(5, 2);
        lcd.print("  ");
      }
      break;

    // 3. PHASE DE JEU (BOMBE ARMEE)
    case ARMED:
      // Décompte chaque seconde
      if (millis() - lastTickTime >= 1000) {
        lastTickTime = millis();
        if (totalSeconds > 0) {
          totalSeconds--;
          updateTM1637(totalSeconds);
          dfPlayer.play(1);
        } else {
          currentState = EXPLODED;
          dfPlayer.play(3);
          lcd.clear();
          lcd.setCursor(2, 1);
          lcd.print("BOOM ! EXPLOSION");
          lcd.setCursor(1, 2);
          lcd.print("BOMBE EXPLOSEE !");
          tm.showNumberDec(0, true);
        }
      }

      // Traitement des touches
      if (key) {
        lastKeyPressTime = millis();
        
        if (key >= '0' && key <= '9') {
          if (currentInput.length() < 4) {
            currentInput += key;
            dfPlayer.play(1);
            lcd.setCursor(6, 2);
            for (int i = 0; i < 4; i++) {
              if (i < currentInput.length()) lcd.print("*");
              else lcd.print("_");
            }
          }
        } else if (key == '#') {
          if (currentInput.length() == 4) {
            if (currentInput == secretCode) {
              currentState = DEFUSED;
              dfPlayer.play(2);
              lcd.clear();
              lcd.setCursor(2, 1);
              lcd.print("DESAMORCAGE OK !");
              lcd.setCursor(3, 2);
              lcd.print("BRAVO AUX CT !");
            } else {
              // ERREUR : -1 MINUTE
              dfPlayer.play(3);
              if (totalSeconds > 60) {
                totalSeconds -= 60;
              } else {
                totalSeconds = 0;
              }
              currentInput = "";
              lcd.setCursor(6, 2);
              lcd.print("____ (FAUX -1M)");
              delay(800);
              lcd.setCursor(6, 2);
              lcd.print("____           ");
            }
          }
        } else if (key == '*') {
          currentInput = "";
          lcd.setCursor(6, 2);
          lcd.print("____           ");
        }
      }

      // RESET AUTOMATIQUE APRES 30 SECONDES D'INACTIVITE
      if (currentInput.length() > 0 && (millis() - lastKeyPressTime >= 30000)) {
        currentInput = "";
        lcd.setCursor(6, 2);
        lcd.print("____ (TIMEOUT) ");
        delay(800);
        lcd.setCursor(6, 2);
        lcd.print("____           ");
      }
      break;

    case DEFUSED:
    case EXPLODED:
      break;
  }
}