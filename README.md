Structures logicielles du programme
1. Structure de configuration EEPROM

Pour pouvoir conserver les paramètres du système même après un redémarrage, le programme utilise l’EEPROM du microcontrôleur. Afin de faciliter la gestion de ces paramètres, nous avons regroupé toutes les variables de configuration dans une seule structure appelée ConfigEEPROM.

Cette structure contient l’ensemble des paramètres modifiables du système, comme les intervalles de mesure, les seuils des capteurs ou encore certains paramètres de fonctionnement. Le fait de regrouper ces données dans une structure permet de simplifier la sauvegarde et la lecture dans l’EEPROM, car il devient possible d’écrire ou de lire l’ensemble des paramètres en une seule opération.

struct ConfigEEPROM {

  uint16_t magic;
  uint32_t dureeAppuiLong;
  uint32_t configTimeout;
  uint32_t logInterval;

  uint16_t fileMaxSize;
  uint8_t timeoutCapteur;

  uint8_t luminActif;
  int16_t luminLow;
  int16_t luminHigh;

  uint8_t tempAirActif;
  int8_t minTempAir;
  int8_t maxTempAir;

  uint8_t hygrActif;
  int8_t hygrMint;
  int8_t hygrMaxt;

  uint8_t pressureActif;
  uint16_t pressureMin;
  uint16_t pressureMax;

  char jourSemaine[4];
};

Un champ particulier appelé magic est utilisé pour vérifier si les données présentes dans l’EEPROM sont valides. Si la valeur ne correspond pas à celle attendue, le système considère que la mémoire n’est pas initialisée et recharge alors les paramètres par défaut.

Grâce à cette organisation, la gestion des paramètres reste claire et facilement maintenable.

2. Machine à états (gestion des modes)

Le fonctionnement du système repose sur une logique de machine à états. Cela signifie que le programme peut se trouver dans plusieurs modes de fonctionnement différents, et que chacun de ces modes possède son propre comportement.

Dans le code, ces modes sont définis à l’aide d’une énumération appelée ModeCapteur.

enum ModeCapteur {
  STANDARD,
  CONFIGURATION,
  MAINTENANCE,
  ECONOMIQUE
};

Chaque état correspond à une situation précise du système. Par exemple, le mode standard correspond au fonctionnement normal de la station météo, tandis que le mode configuration permet de modifier certains paramètres via l’interface série.

Une variable appelée modeActuel permet de stocker l’état courant du système. En fonction de la valeur de cette variable, le programme adapte son comportement et exécute les fonctions correspondantes.

Ce fonctionnement sous forme de machine à états présente plusieurs avantages. Il permet d’organiser le programme de manière plus claire, de simplifier les transitions entre les différents modes et de rendre le code plus lisible et plus facile à maintenir.

3. Gestion dynamique des modes avec des pointeurs de fonction

Pour simplifier encore davantage la gestion des différents modes, le programme utilise un pointeur de fonction. Cette technique permet d’associer dynamiquement une fonction à un mode donné.

Dans le code, un type de pointeur de fonction est défini :

typedef void (*ModeHandler)();
ModeHandler modeHandler;

Chaque mode possède ensuite sa propre fonction :

void modeStandard();
void modeConfiguration();
void modeMaintenance();
void modeEconomique();

Une fonction appelée updateModeHandler() permet de mettre à jour le pointeur de fonction en fonction du mode actif.

void updateModeHandler() {

  switch (modeActuel) {

    case STANDARD:
      modeHandler = &modeStandard;
      break;

    case CONFIGURATION:
      modeHandler = &modeConfiguration;
      break;

    case MAINTENANCE:
      modeHandler = &modeMaintenance;
      break;

    case ECONOMIQUE:
      modeHandler = &modeEconomique;
      break;
  }
}

Dans la boucle principale du programme, il suffit ensuite d’appeler :

modeHandler();

Cela permet d’exécuter directement la fonction correspondant au mode actif. Cette approche évite d’avoir de nombreuses conditions dans la boucle principale et rend la structure du programme plus propre et plus modulaire.

4. Gestion des commandes série

Le mode configuration permet à l’utilisateur de modifier certains paramètres du système à travers l’interface série. Pour cela, le programme doit être capable de recevoir et d’analyser les commandes envoyées par l’utilisateur.

Les caractères reçus sur le port série sont d’abord stockés dans un buffer :

char cmdBuffer[40];
uint8_t cmdPos = 0;

Lorsque l’utilisateur valide une commande (par exemple en appuyant sur Entrée), celle-ci est transmise à une fonction appelée traiterCommande() qui se charge de l’interpréter.

void traiterCommande(char *cmd);

Le programme utilise ensuite des conditions pour identifier la commande et appliquer l’action correspondante.

if (startsWith(cmd, "LOG_INTERVAL=")) {
   ...
}

Ce mécanisme permet par exemple de modifier l’intervalle de mesure, d’activer ou désactiver certains capteurs ou encore de modifier des seuils. Une fois la modification effectuée, les nouveaux paramètres sont enregistrés dans l’EEPROM afin qu’ils soient conservés même après un redémarrage du système.

5. Gestion des boutons

L’interaction utilisateur est également possible grâce à deux boutons présents sur le système. Pour détecter efficacement les actions de l’utilisateur, la gestion des boutons repose sur un système d’interruptions.

Lorsqu’un bouton est pressé ou relâché, une interruption est déclenchée et modifie un indicateur dans le programme.

volatile bool rougeEvent = false;
volatile bool vertEvent = false;

Ces indicateurs sont ensuite analysés dans la boucle principale à l’aide d’une fonction générique appelée traiterBouton().

void traiterBouton(...);

Cette fonction permet notamment de distinguer les appuis courts et les appuis longs, ce qui permet d’associer différentes actions à un même bouton.

L’utilisation des interruptions permet d’améliorer la réactivité du système tout en évitant de bloquer l’exécution du programme.

6. Organisation générale du programme

Le programme suit la structure classique d’un programme Arduino avec deux fonctions principales : setup() et loop().

La fonction setup() est exécutée une seule fois au démarrage du système. Elle sert principalement à initialiser tous les éléments nécessaires au fonctionnement du programme, comme les capteurs, la communication série, les entrées et sorties du microcontrôleur ou encore le chargement des paramètres sauvegardés dans l’EEPROM.

La fonction loop() constitue la boucle principale du programme. Elle est exécutée en continu tant que le système est alimenté. C’est dans cette boucle que sont gérées les différentes tâches du système, comme la lecture des boutons, la réception des données GPS, l’exécution du mode actif et la mise à jour de l’affichage.

Grâce à cette organisation, le programme peut fonctionner en continu et adapter son comportement en fonction du mode dans lequel se trouve le système.
