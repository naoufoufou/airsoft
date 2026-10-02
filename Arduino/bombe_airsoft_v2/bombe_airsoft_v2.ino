#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TM1637Display.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include "PCF8574.h"

// --- BROCHES ---
#define CLK_PIN 2
#define DIO_PIN 3
#define BTN_UP    4
#define BTN_DOWN  5
#define BTN_LEFT  6
#define BTN_RIGHT 7
#define BTN_OK    8
#define RX_PIN 10
#define TX_PIN 11

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

// Anti-maintien
char lastKeyDetected = 0;
int lastBtnDetected = -1;

// --- LECTURE CLAVIER PCF8574 ---
char getKpdKey() {
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
  if (currentPressedKey == lastKeyDetected) return 0;
  lastKeyDetected = currentPressedKey;
  delay(30);
  return currentPressedKey;
}

// --- LECTURE DES 5 BOUTONS FLÈCHES + OK ---
char getNavInput() {
  int pressedPin = -1;
  if (digitalRead(BTN_UP) == LOW)    pressedPin = BTN_UP;
  if (digitalRead(BTN_DOWN) == LOW)  pressedPin = BTN_DOWN;
  if (digitalRead(BTN_LEFT) == LOW)  pressedPin = BTN_LEFT;
  if (digitalRead(BTN_RIGHT) == LOW) pressedPin = BTN_RIGHT;
  if (digitalRead(BTN_OK) == LOW)    pressedPin = BTN_OK;

  if (pressedPin == -1) {
    lastBtnDetected = -1;
    return 0;
  }
  if (pressedPin == lastBtnDetected) return 0;
  lastBtnDetected = pressedPin;
  delay(30);

  if (pressedPin == BTN_UP)    return 'U'; // Up
  if (pressedPin == BTN_DOWN)  return 'D'; // Down
  if (pressedPin == BTN_LEFT)  return 'L'; // Left (Effacer)
  if (pressedPin == BTN_RIGHT) return 'R'; // Right
  if (pressedPin == BTN_OK)    return 'O'; // OK (Valider)
  return 0;
}

// --- MISE À JOUR CHRONO TM1637 ---
void updateTM1637(unsigned long seconds) {
  int mins = seconds / 60;
  int secs = seconds % 60;
  int displayValue = (mins * 100) + secs;
  tm.showNumberDecEx(displayValue, 0b01000000, true);
}

void printFooter() {
  lcd.setCursor(0, 3);
  lcd.print("*:Clear/<-   #:OK");
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
  tm.setBrightness(7);

  pcf.begin();

  mySoftwareSerial.begin(9600);
  if (dfPlayer.begin(mySoftwareSerial)) {
    dfPlayer.volume(22);
  }

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
  // Capture de l'entrée (Clavier OU Boutons)
  char input = getKpdKey();
  if (input == 0) input = getNavInput();

  switch (currentState) {

    // 1. CONFIGURATION DU CODE (Chiffres + Flèches)
    case SETUP_CODE:
      if ((input >= '0' && input <= '9') || input == 'U' || input == 'D' || input == 'L' || input == 'R') {
        if (currentInput.length() < 4) {
          currentInput += input;
          dfPlayer.play(1);
          lcd.setCursor(6 + currentInput.length() - 1, 2);
          lcd.print(input);
        }
      } else if (input == '#' || input == 'O') { // Valider (# ou OK)
        if (currentInput.length() == 4) {
          secretCode = currentInput;
          currentInput = "";
          currentState = SETUP_TIME;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("=== BOMB SETUP ===");
          lcd.setCursor(0, 1);
          lcd.print("Temps (Fleches/Pad):");
          lcd.setCursor(0, 2);
          lcd.print("Min: ");
          lcd.print(timeMinutes);
          printFooter();
        }
      } else if (input == '*') { // Effacer
        currentInput = "";
        lcd.setCursor(6, 2);
        lcd.print("    ");
      }
      break;

    // 2. CONFIGURATION DU TEMPS (+1 / -1 avec flèches ou au pavé)
    case SETUP_TIME:
      if (input == 'U') { // Flèche HAUT : +1 min
        timeMinutes++;
        if (timeMinutes > 99) timeMinutes = 99;
        dfPlayer.play(1);
        lcd.setCursor(5, 2);
        lcd.print("  ");
        lcd.setCursor(5, 2);
        lcd.print(timeMinutes);
      } else if (input == 'D') { // Flèche BAS : -1 min
        timeMinutes--;
        if (timeMinutes < 1) timeMinutes = 1;
        dfPlayer.play(1);
        lcd.setCursor(5, 2);
        lcd.print("  ");
        lcd.setCursor(5, 2);
        lcd.print(timeMinutes);
      } else if (input >= '0' && input <= '9') {
        if (currentInput.length() < 2) {
          currentInput += input;
          timeMinutes = currentInput.toInt();
          dfPlayer.play(1);
          lcd.setCursor(5, 2);
          lcd.print("  ");
          lcd.setCursor(5, 2);
          lcd.print(timeMinutes);
        }
      } else if (input == '#' || input == 'O') { // Valider (# ou OK)
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
      } else if (input == '*') {
        currentInput = "";
        timeMinutes = 10;
        lcd.setCursor(5, 2);
        lcd.print("10");
      }
      break;

    // 3. PHASE DE JEU
    case ARMED:
      // Décompte
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

      // Saisie du code
      if (input) {
        lastKeyPressTime = millis();
        
        if ((input >= '0' && input <= '9') || input == 'U' || input == 'D' || input == 'L' || input == 'R') {
          if (currentInput.length() < 4) {
            currentInput += input;
            dfPlayer.play(1);
            lcd.setCursor(6, 2);
            for (int i = 0; i < 4; i++) {
              if (i < currentInput.length()) lcd.print("*");
              else lcd.print("_");
            }
          }
        } else if (input == '#' || input == 'O') { // Valider avec # ou OK
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
              // Pénalité -1 minute
              dfPlayer.play(3);
              if (totalSeconds > 60) totalSeconds -= 60;
              else totalSeconds = 0;
              
              currentInput = "";
              lcd.setCursor(6, 2);
              lcd.print("____ (FAUX -1M)");
              delay(800);
              lcd.setCursor(6, 2);
              lcd.print("____           ");
            }
          }
        } else if (input == '*') {
          currentInput = "";
          lcd.setCursor(6, 2);
          lcd.print("____           ");
        }
      }

      // Timeout 30s d'inactivité
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