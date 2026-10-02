#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TM1637Display.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include "PCF8574.h"

// --- BROCHES SYSTEME ---
#define CLK_PIN     2
#define DIO_PIN     3
#define BTN_UP      4
#define BTN_DOWN    5
#define BTN_LEFT    6
#define BTN_RIGHT   7
#define BTN_OK      8
#define RX_PIN      10
#define TX_PIN      11

// --- BROCHES DETECTEURS AGENTS CHIMIQUES (MICROSWITCHES) ---
#define SENSOR_1    A0
#define SENSOR_2    A1
#define SENSOR_3    A2

// --- ADRESSES I2C ---
#define LCD_ADDR    0x27
#define PCF_ADDR    0x20

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

// --- MODES DE JEU ---
const int TOTAL_MODES = 4;
const char* gameModes[] = {
  "1. Bombe (Code)",
  "2. Agent Chimique",
  "3. Domination",
  "4. Mode Memoire"
};
int menuIndex = 0;

// --- ETATS DU SYSTEME ---
enum GameState { 
  MAIN_MENU, 
  SETUP_CODE, 
  SETUP_TIME, 
  ARMED_CODE, 
  ARMED_CHEMICAL, 
  DEFUSED, 
  EXPLODED 
};
GameState currentState = MAIN_MENU;

// --- VARIABLES DE JEU ---
String secretCode = "";         
String currentInput = "";     
int timeMinutes = 10;         
unsigned long totalSeconds = 0;

unsigned long lastTickTime = 0;
char lastKeyDetected = 0;
int lastBtnDetected = -1;

// Anti-rebond des switchs chimiques
bool s1_prev = false, s2_prev = false, s3_prev = false;

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

// --- LECTURE BOUTONS NAV ---
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

  if (pressedPin == BTN_UP)    return 'U'; 
  if (pressedPin == BTN_DOWN)  return 'D'; 
  if (pressedPin == BTN_LEFT)  return 'L'; 
  if (pressedPin == BTN_RIGHT) return 'R'; 
  if (pressedPin == BTN_OK)    return 'O'; 
  return 0;
}

// --- AFFICHAGE CHRONO ---
void updateTM1637(unsigned long seconds) {
  int mins = seconds / 60;
  int secs = seconds % 60;
  int displayValue = (mins * 100) + secs;
  tm.showNumberDecEx(displayValue, 0b01000000, true);
}

// --- AFFICHAGE MENU PRINCIPAL ---
void drawMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("=== MENU PRINCIPAL ===");
  lcd.setCursor(0, 1);
  lcd.print("> ");
  lcd.print(gameModes[menuIndex]);
  lcd.setCursor(0, 3);
  lcd.print("^/v:Nav  OK:Choisir");
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // Boutons navigation
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);

  // Capteurs fin de course (Agents Chimiques)
  pinMode(SENSOR_1, INPUT_PULLUP);
  pinMode(SENSOR_2, INPUT_PULLUP);
  pinMode(SENSOR_3, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
  tm.setBrightness(7);
  tm.clear();

  pcf.begin();

  mySoftwareSerial.begin(9600);
  if (dfPlayer.begin(mySoftwareSerial)) {
    dfPlayer.volume(22);
  }

  drawMenu();
}

void loop() {
  char input = getKpdKey();
  if (input == 0) input = getNavInput();

  // ROUTE COMMUNE : '#' ramene au menu principal
  if (input == '#') {
    currentState = MAIN_MENU;
    currentInput = "";
    tm.clear();
    dfPlayer.play(1);
    drawMenu();
    return;
  }

  switch (currentState) {

    case MAIN_MENU:
      if (input == 'U') {
        menuIndex--;
        if (menuIndex < 0) menuIndex = TOTAL_MODES - 1;
        dfPlayer.play(1);
        drawMenu();
      } else if (input == 'D') {
        menuIndex++;
        if (menuIndex >= TOTAL_MODES) menuIndex = 0;
        dfPlayer.play(1);
        drawMenu();
      } else if (input == 'O') {
        dfPlayer.play(1);
        currentInput = "";
        
        if (menuIndex == 0 || menuIndex == 1) { 
          currentState = SETUP_CODE;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("=== CONFIG CODE ===");
          lcd.setCursor(0, 1);
          lcd.print("Code (4-20 carac):");
          lcd.setCursor(0, 3);
          lcd.print("*:Effacer  OK:Valider");
        } else {
          lcd.clear();
          lcd.setCursor(0, 1);
          lcd.print("MODE EN COURS...");
          lcd.setCursor(0, 3);
          lcd.print("#: Menu Principal");
        }
      }
      break;

    case SETUP_CODE:
      if ((input >= '0' && input <= '9') || input == 'U' || input == 'D' || input == 'L' || input == 'R') {
        if (currentInput.length() < 20) {
          currentInput += input;
          dfPlayer.play(1);
          lcd.setCursor(0, 2);
          lcd.print("                    ");
          lcd.setCursor(0, 2);
          lcd.print(currentInput);
        }
      } else if (input == 'O') {
        if (currentInput.length() >= 4) {
          secretCode = currentInput;
          currentInput = "";
          currentState = SETUP_TIME;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("=== REGLE DU TEMPS ===");
          lcd.setCursor(0, 1);
          lcd.print("Temps (Min 1-99):");
          lcd.setCursor(0, 2);
          lcd.print("Min: ");
          lcd.print(timeMinutes);
          lcd.setCursor(0, 3);
          lcd.print("*:Retour   OK:Armer");
        }
      } else if (input == '*' || input == 'L') {
        currentInput = "";
        lcd.setCursor(0, 2);
        lcd.print("                    ");
      }
      break;

    case SETUP_TIME:
      if (input == 'U') {
        timeMinutes++;
        if (timeMinutes > 99) timeMinutes = 99;
        dfPlayer.play(1);
        lcd.setCursor(5, 2);
        lcd.print("  ");
        lcd.setCursor(5, 2);
        lcd.print(timeMinutes);
      } else if (input == 'D') {
        timeMinutes--;
        if (timeMinutes < 1) timeMinutes = 1;
        dfPlayer.play(1);
        lcd.setCursor(5, 2);
        lcd.print("  ");
        lcd.setCursor(5, 2);
        lcd.print(timeMinutes);
      } else if (input == 'O') {
        totalSeconds = (unsigned long)timeMinutes * 60;
        currentInput = "";
        lastTickTime = millis();
        dfPlayer.play(2);

        lcd.clear();
        if (menuIndex == 0) {
          currentState = ARMED_CODE;
          lcd.setCursor(0, 0);
          lcd.print("!!! BOMBE ARMEE !!!");
          lcd.setCursor(0, 1);
          lcd.print("Entrer code:");
        } else if (menuIndex == 1) {
          currentState = ARMED_CHEMICAL;
          lcd.setCursor(0, 0);
          lcd.print("! ALERTE CHIMIQUE !");
        }
        lcd.setCursor(0, 3);
        lcd.print("*:Effacer   OK:Tester");
      } else if (input == '*') {
        currentState = MAIN_MENU;
        drawMenu();
      }
      break;

    // --- MODE 1 : BOMBE CLASSIQUE ---
    case ARMED_CODE:
      // Gestion chrono
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
          lcd.setCursor(2, 0);
          lcd.print("BOOM ! EXPLOSION");
          lcd.setCursor(1, 1);
          lcd.print("BOMBE EXPLOSEE !");
          lcd.setCursor(0, 3);
          lcd.print("#: Menu Principal");
          tm.showNumberDec(0, true);
        }
      }

      if (input) {
        if ((input >= '0' && input <= '9') || input == 'U' || input == 'D' || input == 'L' || input == 'R') {
          currentInput += input;
          dfPlayer.play(1);
          lcd.setCursor(0, 2);
          for (int i = 0; i < 20; i++) {
            if (i < currentInput.length()) lcd.print("*");
            else lcd.print(" ");
          }
        } else if (input == 'O') {
          if (currentInput == secretCode) {
            currentState = DEFUSED;
            dfPlayer.play(2);
            lcd.clear();
            lcd.setCursor(2, 0);
            lcd.print("DESAMORCAGE OK !");
            lcd.setCursor(3, 1);
            lcd.print("BRAVO AUX CT !");
            lcd.setCursor(0, 3);
            lcd.print("#: Menu Principal");
          } else {
            dfPlayer.play(3);
            if (totalSeconds > 60) totalSeconds -= 60;
            else totalSeconds = 0;
            currentInput = "";
            lcd.setCursor(0, 2);
            lcd.print("ERREUR ! (-1 MIN)   ");
            delay(1000);
            lcd.setCursor(0, 2);
            lcd.print("                    ");
          }
        } else if (input == '*' || input == 'L') {
          currentInput = "";
          lcd.setCursor(0, 2);
          lcd.print("                    ");
        }
      }
      break;

    // --- MODE 2 : DETECTEUR D'AGENTS CHIMIQUES ---
    case ARMED_CHEMICAL:
      // Chrono
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
          lcd.setCursor(2, 0);
          lcd.print("BOOM ! EXPLOSION");
          lcd.setCursor(1, 1);
          lcd.print("GAZ DISPERSE !");
          lcd.setCursor(0, 3);
          lcd.print("#: Menu Principal");
          tm.showNumberDec(0, true);
        }
      }

      // Lecture instantanee des 3 microswitches
      bool s1 = (digitalRead(SENSOR_1) == LOW);
      bool s2 = (digitalRead(SENSOR_2) == LOW);
      bool s3 = (digitalRead(SENSOR_3) == LOW);

      // Bip sonore si une nouvelle cartouche est inseree
      if ((s1 && !s1_prev) || (s2 && !s2_prev) || (s3 && !s3_prev)) {
        dfPlayer.play(1);
      }
      s1_prev = s1; s2_prev = s2; s3_prev = s3;

      bool allSensorsReady = (s1 && s2 && s3);

      // Affichage dynamique des emplacements d'agents
      lcd.setCursor(0, 1);
      lcd.print("AGENTS: ");
      lcd.print(s1 ? "[X] " : "[ ] ");
      lcd.print(s2 ? "[X] " : "[ ] ");
      lcd.print(s3 ? "[X]"  : "[ ]");

      // Si les 3 agents ne sont pas là, blocage du clavier
      if (!allSensorsReady) {
        lcd.setCursor(0, 2);
        lcd.print("INSERER 3 AGENTS ! ");
        currentInput = ""; 
      } else {
        // Clavier débloqué
        if (input) {
          if ((input >= '0' && input <= '9') || input == 'U' || input == 'D' || input == 'L' || input == 'R') {
            currentInput += input;
            dfPlayer.play(1);
            lcd.setCursor(0, 2);
            for (int i = 0; i < 20; i++) {
              if (i < currentInput.length()) lcd.print("*");
              else lcd.print(" ");
            }
          } else if (input == 'O') {
            if (currentInput == secretCode) {
              currentState = DEFUSED;
              dfPlayer.play(2);
              lcd.clear();
              lcd.setCursor(2, 0);
              lcd.print("DESAMORCAGE OK !");
              lcd.setCursor(2, 1);
              lcd.print("AGENTS NEUTRALISES");
              lcd.setCursor(0, 3);
              lcd.print("#: Menu Principal");
            } else {
              dfPlayer.play(3);
              if (totalSeconds > 60) totalSeconds -= 60;
              else totalSeconds = 0;
              currentInput = "";
              lcd.setCursor(0, 2);
              lcd.print("ERREUR ! (-1 MIN)   ");
              delay(1000);
              lcd.setCursor(0, 2);
              lcd.print("                    ");
            }
          } else if (input == '*' || input == 'L') {
            currentInput = "";
            lcd.setCursor(0, 2);
            lcd.print("                    ");
          }
        }
      }
      break;

    case DEFUSED:
    case EXPLODED:
      break;
  }
}