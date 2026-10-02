#include "PCF8574.h"

// Remplace 0x20 par l'adresse détectée par ton scanner I2C (ex: 0x20, 0x21, 0x38, 0x39)
PCF8574 pcf(0x20);

const char keys[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

void setup() {
  Serial.begin(9600);
  Wire.begin();

  if (!pcf.begin()) {
    Serial.println("Erreur : Module PCF8574 introuvable à cette adresse !");
    while (1);
  }

  Serial.println("--- TEST CLAVIER PCF8574 PRÊT ---");
  Serial.println("Appuie sur une touche...");
}

void loop() {
  for (int col = 0; col < 4; col++) {
    // Mettre toutes les colonnes en HIGH
    for (int i = 0; i < 8; i++) pcf.write(i, HIGH);
    
    // Mettre la colonne active en LOW (P4 à P7)
    pcf.write(col + 4, LOW);

    // Lire les lignes (P0 à P3)
    for (int row = 0; row < 4; row++) {
      if (pcf.read(row) == LOW) {
        Serial.print("Touche appuyee : ");
        Serial.println(keys[row][col]);
        delay(250); // anti-rebond
      }
    }
  }
}