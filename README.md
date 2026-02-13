# Projet Systèmes Embarqués
Projet du bloc systèmes Embarqués en A2 S3E (CESI)



# Découpage en modules

**Module Acquisition**

Lecture pression (I2C/SPI)

Lecture température

Lecture humidité

Lecture luminosité (analogique)

Lecture GPS (UART)

**Module Gestion du temps**

Communication RTC (I2C)

Timestamp des données

**Module Enregistrement**

Formatage des données

Écriture sur carte SD (SPI)

**Module Interface utilisateur**

Lecture boutons

Gestion des états

Gestion LED RGB (état système)

**Module Gestion des modes**

Mode standard

Mode maintenance

Mode configuration

Mode économique 




**Mode maintenance**

En mode Maintenance, aucune écriture n’est effectuée sur la carte SD afin de garantir l’intégrité des données. Les mesures des capteurs restent cependant visibles en temps réel via le port série pour permettre un diagnostic du système. La carte SD peut ainsi être retirée en toute sécurité. Un appui long sur le bouton rouge permet de quitter ce mode et de revenir au mode de fonctionnement précédent.

Pour ce mode on aura la fonction runMaintenance qui contiendra 3 sous-fonctions. La premiére fonction permet de         ensuite une autre qui permet de retirer la carte SD en sécurité et enfin un si avec la fonction qui permet de 


void runMaintenance() {

  displaySensorsOnSerial();
  allowSafeSDRemoval();

  if (longRedPress()) {
    currentMode = previousMode;
  }
}

