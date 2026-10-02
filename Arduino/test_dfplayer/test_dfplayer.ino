#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

SoftwareSerial mySoftwareSerial(10, 11); // RX (D10 -> TX DFPlayer via résistance 1k), TX (D11 -> RX DFPlayer)
DFRobotDFPlayerMini myDFPlayer;

void setup() {
  Serial.begin(9600);
  mySoftwareSerial.begin(9600);

  Serial.println(F("--- TEST DFPLAYER MINI ---"));

  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println(F("Erreur : DFPlayer non détecté. Vérifier le câblage et la carte SD."));
    while (true);
  }

  Serial.println(F("DFPlayer connecté avec succès !"));
  myDFPlayer.volume(20); // Volume de 0 à 30

  // Jouer le premier son (0001.mp3)
  Serial.println(F("Lecture de 0001.mp3..."));
  myDFPlayer.play(1);
}

void loop() {
  // Rien à exécuter en boucle pour ce test simple
}