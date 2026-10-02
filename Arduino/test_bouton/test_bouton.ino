// Broches des 5 boutons
#define BTN_UP    4
#define BTN_DOWN  5
#define BTN_LEFT  6
#define BTN_RIGHT 7
#define BTN_OK    8

void setup() {
  Serial.begin(9600);
  
  // Activation des résistances de rappel internes (INPUT_PULLUP)
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);

  Serial.println(F("--- TEST DES 5 BOUTONS NAVIGATION ---"));
  Serial.println(F("Appuie sur HAUT, BAS, GAUCHE, DROITE ou OK..."));
}

void loop() {
  if (digitalRead(BTN_UP) == LOW) {
    Serial.println(F("Bouton appuyé : HAUT (D4)"));
    delay(200); // Anti-rebond
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    Serial.println(F("Bouton appuyé : BAS (D5)"));
    delay(200);
  }
  if (digitalRead(BTN_LEFT) == LOW) {
    Serial.println(F("Bouton appuyé : GAUCHE (D6)"));
    delay(200);
  }
  if (digitalRead(BTN_RIGHT) == LOW) {
    Serial.println(F("Bouton appuyé : DROITE (D7)"));
    delay(200);
  }
  if (digitalRead(BTN_OK) == LOW) {
    Serial.println(F("Bouton appuyé : OK (D8)"));
    delay(200);
  }
}