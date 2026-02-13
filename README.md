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

# Mode standard

Le mode standard est le mode de fonctionnement normal du système.  
Il est activé lorsque le système démarre sans bouton appuyé.

Dans notre station météo, cela nous permet quatre choses :

- effectuer des mesures périodiques  
- enregistrer les données sur la carte SD  
- surveiller les erreurs  
- indiquer son état via la LED verte  



## Intervalle de mesure

Nous utilisons donc le `LOG_INTERVAL`, avec une valeur de 10 minutes comme indiqué dans notre sujet.  
Ce paramètre peut être modifié via le mode configuration.

Le système vérifie régulièrement si le temps défini est écoulé avant de relancer une acquisition.

Cela a pour objectif trois choses :

- éviter des mesures trop fréquentes  
- respecter le cahier des charges  
- optimiser la consommation énergétique  


## Lecture des capteurs

Les capteurs activés sont lus séquentiellement et, lors de chaque intervalle, nous avons 5 étapes :

- lecture température  
- lecture humidité  
- lecture pression  
- lecture luminosité  
- lecture GPS  



## Gestion du timeout

`TIMEOUT = 30s`

Chaque capteur dispose d’un temps maximum d’attente.

Si un capteur ne répond pas dans le délai :

- la mesure est abandonnée  
- la valeur enregistrée devient "NA"  

Cela a pour objectif :

- d’éviter le blocage du programme  
- de garantir une ligne de log complète  
- de signaler une anomalie sans arrêter le système  


## Enregistrement sur carte SD

Après l’acquisition :

- récupération de la date et de l’heure via le RTC  
- création d’une ligne horodatée  
- écriture dans un fichier `.LOG`  



## Gestion de la taille

`FILE_MAX_SIZE`

Chaque fichier possède une taille maximale.  
Si cette limite est atteinte :

- archivage du fichier  
- création d’un nouveau fichier principal  
- reprise de l’écriture  

Cela a pour but :

- d’éviter un fichier trop volumineux  
- de préserver l’intégrité des données  
- de structurer le stockage  



## Indication visuelle

En mode standard :

- LED verte continue  

Cela indique que le système fonctionne normalement.



Mode maintenance

Mode configuration

Mode économique 
