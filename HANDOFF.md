# Passation — RumScale

Pour l'assistant qui reprend ce projet. Le [README](README.md) explique comment
monter et utiliser l'appareil ; ce document-ci explique **l'état réel du code, les
décisions non évidentes et les pièges déjà payés**. Lis-le avant de modifier quoi
que ce soit.

---

## 1. L'avertissement qui prime sur tout le reste

**Rien n'a jamais tourné sur du matériel.** Pas une ligne. La carte cible n'était
pas encore achetée au moment d'écrire ce code.

Ce qui est réellement vérifié :

- les trois cibles compilent (`ideaspark`, `esp32dev`, `esp32s3`) ;
- l'app web a été testée dans un navigateur contre un simulateur HTTP
  (`tools/mock_ui.py`), y compris les états dégradés ;
- la géométrie de l'écran couleur a été contrôlée en reconstruisant le layout en
  SVG à partir des constantes du source.

Ce qui n'est **pas** vérifié : la lecture des HX711, l'affichage ST7789, le
bouton, le Wi-Fi, Telegram, la persistance NVS/LittleFS, le cycle
extinction/rallumage. Ne présente aucune de ces parties comme fonctionnelle à
l'utilisateur, et ne laisse pas un futur lecteur du code le croire.

Les trois points à vérifier en priorité au premier flash sont listés en §6.

---

## 2. Ce que fait l'appareil

Un tonneau de vieillissement (cadeau) posé sur une base qui le pèse en continu.
Deux cellules de charge → deux HX711 → un ESP32 qui en déduit le volume restant,
compte les jours de vieillissement, sert une app web, et prévient par Telegram
quand quelqu'un se sert.

## 3. Architecture

Un module par responsabilité, sans framework, tout en `namespace` (pas de
classes — il n'y a qu'une instance de chaque chose).

| Fichier | Rôle |
|---|---|
| `main.cpp` | `setup()` / `loop()`, cinq appels, rien d'autre |
| `config.h` | **tout le brochage et tous les seuils**, par cible (`#if defined(BOARD_*)`) |
| `store.{h,cpp}` | réglages persistants (NVS), un seul blob binaire |
| `scale.{h,cpp}` | lecture non bloquante des HX711, médiane + lissage, calibration |
| `barrel.{h,cpp}` | **le cœur métier** : paliers, événements, volume, vieillissement |
| `history.{h,cpp}` | anneaux sur LittleFS : points horaires + journal d'événements |
| `button.{h,cpp}` | antirebond et appui long, partagé par les deux interfaces |
| `ui_tft.cpp` | interface ST7789 couleur (cible `ideaspark`) |
| `ui_oled.cpp` | interface OLED SSD1306 (cibles `esp32dev`, `esp32s3`) |
| `ui.h` | l'interface commune aux deux ci-dessus |
| `net.{h,cpp}` | Wi-Fi, portail de configuration, serveur web, WebSocket, API REST |
| `notify.{h,cpp}` | file d'attente Telegram |
| `web_ui.h` | l'app web entière (HTML+CSS+JS) en PROGMEM |

`ui_tft.cpp` et `ui_oled.cpp` sont tous deux gardés par `#if defined(BOARD_*)` :
PlatformIO compile tout `src/`, la sélection se fait donc dans le fichier.

---

## 4. Les décisions non évidentes

Chacune a une raison. Si tu veux en défaire une, sache ce que tu casses.

### On ne se fie jamais au poids absolu instantané

Une cellule de charge sous charge statique **dérive** pendant des mois
(température, fluage du métal). Afficher le poids brut donnerait un niveau qui
s'éloigne lentement du réel.

D'où la machine à paliers dans `barrel.cpp` : quand le poids reste dans une bande
de 12 g pendant 20 s, le palier est figé ; l'écart avec le palier précédent
devient un événement. Sous 25 g d'écart, c'est de la dérive. Une baisse lente
(< 25 g/jour) est de l'évaporation ; une baisse franche est un service.

**Ne remplace pas ça par une comparaison de poids absolu.** C'est la valeur
centrale du projet.

### Un événement attend 3 minutes avant d'être confirmé

Un verre posé sur la base produit une hausse puis une baisse équivalentes. Un
événement qui se fait annuler par son opposé dans la fenêtre disparaît, les deux.
C'est pourquoi une alerte Telegram arrive **quelques minutes après** le service,
et non instantanément. Ce délai est voulu (`EVENT_CONFIRM_MS`).

### Au redémarrage, on ne fabrique jamais un service

`reconcileAfterBoot()` compare le poids au dernier point d'historique pour
rattraper l'évaporation survenue hors tension. Si l'écart est trop rapide pour de
l'évaporation, il journalise un manque « d'origine indéterminée » et **n'invente
pas de service** : un tonneau soulevé puis reposé pendant la coupure suffirait à
en créer un faux, avec alerte à la clé.

### `agingDays()` a trois retours, pas deux

`AGING_NEVER` (−1) et `AGING_UNKNOWN` (−2) sont distincts. Renvoyer `0` quand
l'heure n'est pas synchronisée afficherait « J+0 » à quelqu'un dont le rhum
vieillit depuis trois mois — c'est le pire message possible. Les trois interfaces
(TFT, OLED, web) traitent explicitement `−2`.

De même, `resetAging()` sans heure n'écrit pas `0` (qui signifie « jamais
démarré ») : il pose `agingPending`, que `resolveAgingPending()` date au retour
du NTP.

### Une seule pente de calibration pour deux cellules

`grams()` fait `(Σ(brut − offset)) / countsPerGram`. C'est exact tant que la
répartition de la charge est constante — ce qui est le cas d'un tonneau qui ne
bouge plus. Calibrer chaque cellule séparément demanderait de poser un poids connu
sur chacune, sans gain réel ici.

### `MAGIC` dans `store.cpp` doit être incrémenté à chaque changement de `Settings`

Le blob NVS est relu tel quel. Un ancien blob de même taille relu au nouveau
format donnerait **une balance qui mesure faux sans le dire**. Le prix d'un
incrément est une recalibration ; le prix de l'oubli est invisible et permanent.
Actuellement : `RSC2`.

### L'app web est en PROGMEM, pas sur LittleFS

Un seul `pio run -t upload` met tout à jour, pas de `uploadfs` à oublier. Le
projet est un cadeau : chaque étape de déploiement en moins est une panne en
moins. LittleFS ne sert qu'à l'historique et se formate seul.

### `huge_app.csv`

Le firmware pèse ~1,55 Mo (TLS pour Telegram + serveur asynchrone) et ne tient pas
dans les 1,31 Mo du schéma par défaut. On perd l'OTA, dont on n'a pas l'usage.

---

## 5. Pièges déjà rencontrés — ne les repaie pas

**Arduino_GFX est figé en `~1.4.0`.** Depuis la 1.5, son pilote SPI ESP32 inclut
`esp32-hal-periman.h`, qui n'existe que dans le core Arduino 3.x, **sans garde de
version**. La plateforme `espressif32@6.x` fournit le core 2.0.17. Remonter cette
borne casse la compilation, sauf à migrer aussi vers le core 3.x (pioarduino).

**`WiFiClientSecure::setTimeout()` est en secondes**, contrairement à
`HTTPClient::setTimeout()` qui est en millisecondes. Les deux sont utilisés dans
`notify.cpp`, avec un commentaire.

**`PIN_LED` vaut −1 sur ESP32-S3.** La LED embarquée de ces cartes est une WS2812
adressable qu'un `digitalWrite` ne pilote pas. Tout le code de LED est gardé par
`if (PIN_LED >= 0)`.

**Sur ESP32-S3 à PSRAM octale (`R8`), les GPIO 33 à 37 sont pris.** Ne les utilise
jamais pour les HX711.

**Sur la carte ideaspark, l'écran occupe les GPIO 2, 4, 15, 18, 23 et 32.** Le
brochage OLED d'origine utilisait le 2 et le 4 : il a fallu déplacer les HX711.

**Une dalle ST7789 de 170 px de large exige un décalage de colonne** (`TFT_COL_OFFSET`,
à 35). Si l'image est glissée horizontalement au premier flash, c'est là.

---

## 6. Ce qui reste à faire

### À vérifier au tout premier flash, dans cet ordre

1. **Le décalage de l'écran.** Image glissée → passer `TFT_COL_OFFSET` de 35 à 0.
2. **La métrique verticale des polices.** `ui_tft.cpp` suppose que
   `u8g2_font_logisoso38_tn` monte 38 px au-dessus de la ligne de base. Si les
   chiffres sont rognés ou trop hauts, décaler `PCT_BASE` et ses voisines.
3. **Le sens des cellules.** Appuyer sur le plateau doit faire *monter* la valeur
   brute (page Diagnostic). Sinon, échanger vert et blanc sur le HX711 concerné.
4. **Le cycle de coupure.** Initialiser complètement, débrancher, rebrancher :
   la calibration et le nombre de jours doivent être intacts, et la tuile
   « dernier service » repeuplée depuis le journal.

### Idées non faites, par ordre d'utilité

- **Compensation de température** : un DS18B20 collé sur une cellule, et corriger
  la pente. C'est la vraie limite de précision actuelle.
- **Export/import des réglages** depuis l'app, pour survivre à un changement de
  structure `Settings` sans recalibrer.
- **Authentification** sur l'app : il n'y en a aucune. Assumé (réseau domestique,
  c'est un tonneau), mais à savoir. Ne jamais exposer l'appareil sur Internet.
- Un RTC DS3231 a été explicitement écarté par l'utilisateur : le NTP suffit,
  puisque la date de départ est stockée en absolu.

---

## 7. Travailler sans la carte

```bash
python3 tools/mock_ui.py
```

Sert la même app web que l'ESP32 (extraite de `web_ui.h`) sur
`http://127.0.0.1:8777`, avec 95 jours d'historique et une dizaine de services
inventés. Toute modification de l'app web se teste comme ça. Édite les valeurs en
haut du script pour simuler un état dégradé (`aging_days: -2` pour l'heure non
synchronisée, `pct: -1` pour un tonneau non initialisé).

Pour compiler les trois cibles :

```bash
pio run -e ideaspark && pio run -e esp32dev && pio run -e esp32s3
```

---

## 8. Conventions

- **Le code et les commentaires sont en français**, comme l'interface. Continue.
- Les commentaires expliquent **pourquoi**, jamais quoi. S'il n'y a pas de
  pourquoi, il n'y a pas de commentaire.
- Tous les seuils réglables vivent dans `config.h`, jamais en dur dans la logique.
- Les textes destinés à l'utilisateur sont en UTF-8 direct dans le source (pas de
  séquences `\xC3\xA9`) — le core ESP32 et U8g2 les gèrent.
