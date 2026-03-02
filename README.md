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




# Mode maintenance

Le mode maintenance est un mode spécifique destiné aux opérations de contrôle et de manipulation du système.  
Il est accessible depuis le mode standard ou le mode économique.

Dans notre station météo, ce mode permet principalement :

- consulter les données des capteurs en temps réel  
- accéder aux informations via l’interface série  
- retirer ou remplacer la carte SD en toute sécurité  
- éviter toute corruption des données enregistrées  


## Accès au mode maintenance

Le passage en mode maintenance s’effectue par une action utilisateur :

- appui long de 5 secondes sur le bouton rouge  

Le système bascule alors depuis le mode standard (ou économique) vers le mode maintenance.

Pour quitter ce mode :

- un nouvel appui long de 5 secondes sur le bouton rouge  
- retour au mode précédent  


## Acquisition des capteurs

En mode maintenance :

- les capteurs restent actifs  
- les mesures sont toujours effectuées  

Cependant :

- aucune donnée n’est enregistrée sur la carte SD  

Les valeurs mesurées sont transmises directement via l’interface série, ce qui permet :

- une vérification immédiate du fonctionnement des capteurs  
- un diagnostic rapide en cas d’anomalie  


## Interface série

Le port série permet d’afficher :

- les valeurs instantanées des capteurs  
- l’état général du système  

Ce mode facilite :

- le débogage  
- la maintenance logicielle  
- les tests sans impact sur les fichiers de log  


## Gestion de la carte SD

En mode maintenance :

- l’écriture sur la carte SD est totalement désactivée  

Cela permet :

- le retrait sécurisé de la carte SD  
- l’insertion d’une nouvelle carte  
- l’élimination du risque de corruption des fichiers  

Une fois la carte remise en place, le système pourra reprendre l’enregistrement normal après retour en mode standard.


## Indication visuelle

En mode maintenance :

- LED orange continue  

Cette indication visuelle permet d’identifier clairement que le système n’est pas en fonctionnement normal et qu’aucune donnée n’est sauvegardée.

# En Mode configuration:

-Pas d’acquisition capteurs

-Interaction série UART

-Paramètres sauvegardés EEPROM

-Retour auto après 30 min

**Structure:**
```c
void runConfig() {

  handleSerialCommands();

  if (noActivityFor30min()) {
    currentMode = STANDARD;
 }
}
```
**Gestion logicielle du mode configuration**

Le mode configuration est piloté par une fonction dédiée runConfig(), exécutée lorsque le système est dans l’état correspondant. Cette fonction assure en priorité le traitement des commandes série via handleSerialCommands(), permettant la modification des paramètres internes.
Un mécanisme de surveillance d’activité est intégré à travers la fonction noActivityFor30min(). En cas d’inactivité prolongée, le système quitte automatiquement le mode configuration en réaffectant la variable d’état currentMode au mode standard.
Cette approche repose sur une logique de machine à états simple, garantissant un fonctionnement robuste, structuré et sécurisé.

# En Mode économique : 

**Structure:**

## Mode Économie – Structure du programme

```c
void runEco() {

  if (timeToMeasureEco()) {
     readSensorsReduced();
     saveToSD();
  }

  if (longRedPress()) {
     currentMode = STANDARD;
  }
}
```


Dans ce mode, les acquisitions ne sont pas continues mais déclenchées à intervalle réduit via la fonction timeToMeasureEco(), permettant d’espacer les mesures (par exemple toutes les 20 minutes au lieu de 10). Seuls les capteurs essentiels sont utilisés à travers la fonction readSensorsReduced(), ce qui limite la consommation des ressources matérielles.
Les données collectées sont ensuite enregistrées sur la carte SD via la fonction saveToSD(), garantissant la continuité du suivi météorologique.
Par ailleurs, une interaction utilisateur est maintenue : un appui long sur le bouton rouge (longRedPress()) permet de quitter le mode économique et de revenir au mode standard en modifiant la variable d’état currentMode.

