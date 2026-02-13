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

Mode normal

Mode maintenance

Mode erreur

Mode économie d’énergie
