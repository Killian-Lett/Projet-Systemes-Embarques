# Structures logicielles du programme

## Structure de configuration EEPROM

Pour pouvoir conserver les paramètres du système même après un redémarrage, le programme utilise la mémoire EEPROM du microcontrôleur. Afin de faciliter la gestion de ces paramètres, toutes les variables de configuration sont regroupées dans une seule structure appelée `ConfigEEPROM`.

L’objectif de cette structure est de centraliser l’ensemble des paramètres modifiables du système, comme les intervalles de mesure, certains délais de fonctionnement ou encore les seuils utilisés par les capteurs. En regroupant ces données dans une seule structure, il devient plus simple de sauvegarder et de charger la configuration complète depuis l’EEPROM.

Cette approche permet également de garder un code plus organisé, car tous les paramètres importants sont regroupés au même endroit.

```c
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
```

Un champ particulier appelé `magic` est utilisé pour vérifier si les données présentes dans l’EEPROM sont valides. Lors du démarrage, le programme lit ce champ afin de déterminer si la mémoire contient déjà une configuration correcte. Si ce n’est pas le cas, le système initialise alors les paramètres avec leurs valeurs par défaut.

Cette structure permet donc de garantir une gestion claire et fiable des paramètres du système.



## Machine à états (gestion des modes)

Le fonctionnement général du programme repose sur une logique de machine à états. Cela signifie que le système peut se trouver dans différents modes de fonctionnement et que chacun de ces modes possède un comportement spécifique.

Dans le code, les différents états du système sont définis grâce à une énumération appelée `ModeCapteur`.

```c
enum ModeCapteur {
  STANDARD,
  CONFIGURATION,
  MAINTENANCE,
  ECONOMIQUE
};
```

Chaque valeur de cette énumération correspond à un mode particulier du système. Par exemple, le mode standard correspond au fonctionnement normal de la station météo, tandis que le mode configuration permet de modifier les paramètres via l’interface série.

Le mode courant est stocké dans une variable appelée `modeActuel`.

```c
ModeCapteur modeActuel;
```

En fonction de la valeur de cette variable, le programme adapte son comportement et exécute les fonctions correspondantes. Cette organisation sous forme de machine à états permet de structurer clairement le programme et de gérer facilement les transitions entre les différents modes.



## Gestion dynamique des modes avec des pointeurs de fonction

Afin de simplifier la gestion des différents modes du système, le programme utilise également un pointeur de fonction. Cette technique permet d’associer dynamiquement une fonction au mode actif.

Un type de pointeur de fonction est d’abord défini dans le programme.

```c
typedef void (*ModeHandler)();
ModeHandler modeHandler;
```

Chaque mode possède ensuite une fonction dédiée qui décrit son comportement.

```c
void modeStandard();
void modeConfiguration();
void modeMaintenance();
void modeEconomique();
```

Une fonction appelée `updateModeHandler()` permet d’associer la bonne fonction au mode courant.

```c
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
```

Dans la boucle principale du programme, il suffit ensuite d’appeler :

```c
modeHandler();
```

Cela permet d’exécuter directement la fonction correspondant au mode actif. Cette méthode rend le programme plus clair et évite de multiplier les conditions dans la boucle principale.



## Gestion des commandes série

Le mode configuration permet de modifier certains paramètres du système à travers l’interface série. Pour cela, le programme doit être capable de recevoir et d’interpréter les commandes envoyées par l’utilisateur.

Les caractères reçus via la liaison série sont d’abord stockés dans un buffer.

```c
char cmdBuffer[40];
uint8_t cmdPos = 0;
```

Lorsque l’utilisateur valide une commande, celle-ci est analysée par la fonction `traiterCommande()`.

```c
void traiterCommande(char *cmd);
```

Le programme utilise ensuite différentes conditions pour reconnaître les commandes et appliquer les modifications correspondantes.

```c
if (startsWith(cmd, "LOG_INTERVAL=")) {
   ...
}
```

Ce mécanisme permet par exemple de modifier l’intervalle de mesure, d’activer ou désactiver certains capteurs ou encore de changer différents seuils. Une fois les paramètres modifiés, ils sont sauvegardés dans l’EEPROM afin de conserver la configuration même après un redémarrage du système.



## Gestion des boutons

L’utilisateur peut également interagir avec le système grâce à des boutons physiques. Afin d’améliorer la réactivité du programme, la gestion de ces boutons repose sur un système d’interruptions.

Lorsqu’un bouton est pressé ou relâché, une interruption est déclenchée et modifie un indicateur dans le programme.

```c
volatile bool rougeEvent = false;
volatile bool vertEvent = false;
```

Ces indicateurs sont ensuite analysés dans la boucle principale à l’aide d’une fonction appelée `traiterBouton()`.

```c
void traiterBouton(...);
```

Cette fonction permet notamment de distinguer les appuis courts et les appuis longs, ce qui permet d’associer plusieurs actions différentes à un même bouton. Grâce à ce mécanisme, le système reste réactif tout en évitant de bloquer l’exécution du programme.



## Organisation générale du programme

Le programme suit la structure classique d’un programme Arduino avec deux fonctions principales : `setup()` et `loop()`.

La fonction `setup()` est exécutée une seule fois au démarrage du système. Elle permet d’initialiser les différents périphériques, de configurer les entrées et sorties du microcontrôleur, de démarrer les communications nécessaires et de charger les paramètres sauvegardés dans l’EEPROM.

La fonction `loop()` constitue la boucle principale du programme. Elle est exécutée en continu tant que le système est alimenté. Dans cette boucle, le programme gère les interactions utilisateur, lit les données provenant de certains capteurs et exécute le comportement correspondant au mode actif.

Cette organisation permet au système de fonctionner de manière continue tout en restant capable de s’adapter aux différents modes de fonctionnement.
