#include <Arduino.h>
#include "rgb_lcd.h"
#include <ESP32Encoder.h>
#include "Adafruit_TCS34725.h"

ESP32Encoder encoder;
rgb_lcd lcd;

// ----------------------------
// Définition des états
// ----------------------------
enum Etat {
  INIT_POS = 1,
  ATTENTE_BALLE,
  ALLER_MESURE,
  TEST_COULEUR,
  ALLER_DETECTION,
  ATTENTE_RETRAIT,
  ALLER_SERVOMOTEUR,
  RETOUR_INIT
};

Etat etat = INIT_POS;

// ----------------------------
// Broches
// ----------------------------
const int IR0_PIN = 36;
const int BP_VERT  = 0;
const int BP_BLEU  = 2;
const int BP_JAUNE = 12;

const int PWM_PIN = 27;
const int DIR_PIN = 26;

// ----------------------------
// Paramètres mécaniques
// ----------------------------
const int POSITION_TICKS = 105;
const int NB_POSITIONS = 16;

// Positions spécifiques du cahier des charges
const int POS_INIT = 0;
const int POS_MESURE = 300;
const int POS_DETECTION = 600;
const int POS_SERVOMOTEUR = 900;

int CONSIGNE = 0;
int integ;

// ----------------------------
// Capteur couleur (exemple constructeur)
// ----------------------------
Adafruit_TCS34725 tcs = Adafruit_TCS34725(
  TCS34725_INTEGRATIONTIME_50MS,
  TCS34725_GAIN_4X
);

// ----------------------------
// Fonction pour inverser le sens du codeur
// ----------------------------
int getPos() {
  return -(int32_t)encoder.getCount();
}

// ----------------------------
// Fonctions moteur
// ----------------------------
void moteur(int sens, int pwm) {
  digitalWrite(DIR_PIN, sens);
  ledcWrite(0, pwm);
}

// ----------------------------
// Déplacement simple avec ton asservissement
// ----------------------------
void allerA(int cible) {
  if (getPos() < cible) {
    CONSIGNE = 1;
    while (getPos() < cible) {}
  } else {
    CONSIGNE = -1;
    while (getPos() > cible) {}
  }
  CONSIGNE = 0;
  integ = 0;
  delay(500);
}

// ----------------------------
// Tâche FreeRTOS : asservissement vitesse
// ----------------------------
void vTaskPeriodic(void *pvParameters)
{
  int codeur, vitesse, codeurMemo = 0, erreur, k = 100, commande;
  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (1)
  {
    codeur = getPos();
    vitesse = codeur - codeurMemo;
    codeurMemo = codeur;

    erreur = CONSIGNE - vitesse;
    integ += erreur;

    commande = k * erreur + 10 * integ;

    if (commande > 2047) commande = 2047;
    if (commande < -2047) commande = -2047;

    if (commande > 0) {
      digitalWrite(DIR_PIN, 0);
      ledcWrite(0, commande);
    } else {
      digitalWrite(DIR_PIN, 1);
      ledcWrite(0, -commande);
    }

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
  }
}

// ----------------------------
// Setup
// ----------------------------
void setup() {
  Serial.begin(115200);

  Wire1.setPins(15, 5);
  lcd.begin(16, 2, LCD_5x8DOTS, Wire1);
  lcd.setRGB(85,120,120);

  pinMode(BP_VERT, INPUT_PULLUP);
  pinMode(BP_BLEU, INPUT_PULLUP);
  pinMode(BP_JAUNE, INPUT_PULLUP);

  pinMode(DIR_PIN, OUTPUT);

  encoder.attachFullQuad(23, 19);
  encoder.clearCount();

  ledcSetup(0, 25000, 11);
  ledcAttachPin(PWM_PIN, 0);

  // Init capteur couleur
  if (tcs.begin()) {
    Serial.println("Capteur TCS34725 OK");
  } else {
    Serial.println("ERREUR capteur TCS34725 !");
    while(1);
  }

  xTaskCreate(vTaskPeriodic, "vTaskPeriodic", 10000, NULL, 2, NULL);

  lcd.print("Systeme pret");
  delay(1000);
}

// ----------------------------
// LOOP PRINCIPAL
// ----------------------------
void loop() {

  int ir0 = analogRead(IR0_PIN);
  int bleu = digitalRead(BP_BLEU);
  int pos = getPos();

  switch(etat) {

    // ---------------------------------------------------------
    // 1. Recherche position initiale
    // ---------------------------------------------------------
    case INIT_POS:
      lcd.setCursor(0,1);
      lcd.print("Init position   ");

      CONSIGNE = 1;

      if (ir0 < 1500) {
        CONSIGNE = 0;
        encoder.clearCount();
        integ = 0;
        etat = ATTENTE_BALLE;
        delay(500);
      }
      break;

    // ---------------------------------------------------------
    // 2. Attente balle + bouton bleu
    // ---------------------------------------------------------
    case ATTENTE_BALLE:
      lcd.setCursor(0,1);
      lcd.print("Poser balle + bleu");

      if (bleu == LOW) {
        etat = ALLER_MESURE;
        delay(200);
      }
      break;

    // ---------------------------------------------------------
    // 3. Aller à la position mesure couleur
    // ---------------------------------------------------------
    case ALLER_MESURE:
      lcd.setCursor(0,1);
      lcd.print("Vers mesure     ");

      allerA(POS_MESURE);
      etat = TEST_COULEUR;
      break;

    // ---------------------------------------------------------
    // 4. Test couleur (exemple constructeur)
    // ---------------------------------------------------------
    case TEST_COULEUR:
      lcd.setCursor(0,1);
      lcd.print("Test couleur    ");

      uint16_t r, g, b, c;
      tcs.getRawData(&r, &g, &b, &c);

      Serial.print("R="); Serial.print(r);
      Serial.print(" G="); Serial.print(g);
      Serial.print(" B="); Serial.print(b);
      Serial.print(" C="); Serial.println(c);

      if (r > 2000 && g > 2000 && b > 2000) {
        etat = ALLER_DETECTION;
      } else {
        etat = ALLER_SERVOMOTEUR;
      }
      break;

    // ---------------------------------------------------------
    // 5. Aller position détection balle
    // ---------------------------------------------------------
    case ALLER_DETECTION:
      lcd.setCursor(0,1);
      lcd.print("Vers detection  ");

      allerA(POS_DETECTION);
      etat = ATTENTE_RETRAIT;
      break;

    // ---------------------------------------------------------
    // 6. Attente retrait balle
    // ---------------------------------------------------------
    case ATTENTE_RETRAIT:
      lcd.setCursor(0,1);
      lcd.print("Retirer balle   ");

      if (ir0 > 3000) {
        etat = INIT_POS;
      }
      break;

    // ---------------------------------------------------------
    // 7. Aller position servomoteur
    // ---------------------------------------------------------
    case ALLER_SERVOMOTEUR:
      lcd.setCursor(0,1);
      lcd.print("Vers servo      ");

      allerA(POS_SERVOMOTEUR);
      etat = RETOUR_INIT;
      break;

    // ---------------------------------------------------------
    // 8. Pause 5s puis retour init
    // ---------------------------------------------------------
    case RETOUR_INIT:
      lcd.setCursor(0,1);
      lcd.print("Pause 5s        ");
      delay(5000);
      etat = INIT_POS;
      break;
  }

  delay(20);
}
