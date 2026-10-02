#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TM1637Display.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// --- Configuration TM1637 ---
#define CLK_PIN 2
#define DIO_PIN 3
TM1637Display display(CLK_PIN, DIO_PIN);

// --- Configuration DFPlayer ---
#define DF_RX_PIN 10
#define DF_TX_PIN 11
SoftwareSerial dfSerial(DF_RX_PIN, DF_TX_PIN);
DFRobotDFPlayerMini myDFPlayer;

// --- Configuration LCD 2004 I2C ---
// Adresse confirme par le scanner : 0x27
LiquidCrystal_I2C lcd(0x27, 20, 4);

void setup() {
  Serial.begin(9600);
  
  // 1. Initialisation TM1637 (Affichage 7-segments)
  display.setBrightness(0x0f);
  display.showNumberDec(1234, true);

  // 2. Initialisation LCD 2004
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  lcd.setCursor(0, 0);
  lcd.print("=== ARDUINO BOMB ===");
  lcd.setCursor(0, 1);
  lcd.print("Systeme Initialise  ");
  lcd.setCursor(0, 2);
  lcd.print("Ecran LCD 2004: OK  ");
  lcd.setCursor(0, 3);
  lcd.print("TM1637 Display: OK  ");

  // 3. Initialisation DFPlayer Mini
  dfSerial.begin(9600);
  if (myDFPlayer.begin(dfSerial)) {
    Serial.println(F("DFPlayer OK"));
    myDFPlayer.volume(20); // Volume (0 a 30)
    // myDFPlayer.play(1); // Décommente si une carte MicroSD est insérée
  } else {
    Serial.println(F("Erreur DFPlayer (SD manquante ou cablage)"));
  }
}

void loop() {
  // Boucle de test vide
}