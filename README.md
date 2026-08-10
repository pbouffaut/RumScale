# RumScale

Un tonneau de vieillissement posé sur une base qui le pèse. Il sait combien il
reste dedans, depuis combien de jours le rhum vieillit, et il prévient quand le
niveau baisse.

- **Écran couleur + un bouton** sur le tonneau : jauge de tonneau qui se vide,
  niveau en %, jours de vieillissement, QR code pour ouvrir l'app, remise à zéro
  du vieillissement.
- **App web** servie par l'ESP32 lui-même : jauge, historique, journal des
  services, assistant d'initialisation. Aucune application à installer — ça
  s'ouvre dans le navigateur, sur iPhone comme sur Android.
- **Alertes Telegram** à chaque service et aux seuils de 50 %, 25 % et 10 %.

---

## 1. Matériel

| Élément | Détail |
|---|---|
| Carte | **ideaspark ESP32 avec écran ST7789 1,9" 170×320 intégré** (cible principale). L'ESP32 et l'écran sont sur la même carte, déjà câblés. |
| 2 × HX711 | modules ampli/ADC pour cellule de charge |
| 2 × cellules de charge | **4 fils (pont complet)**, barres à flexion. Pour un tonneau de 5 L, prends du 10 kg chacune : le tout chargé pèse ~10 kg, et la marge protège des chocs. |
| Bouton | poussoir momentané, câblé vers la masse |
| Base | deux plaques rigides (contreplaqué 18 mm ou alu) + entretoises + vis M4/M5 |
| Alimentation | USB 5 V, 1 A suffit |

Deux autres combinaisons sont maintenues dans le même code, avec un écran OLED
SSD1306 0,96" I²C à la place : un ESP32-WROOM-32 nu, ou un ESP32-S3 nu (Waveshare
ESP32-S3-DEV-KIT, Freenove ESP32-S3-WROOM). Voir la section 4.

**Pourquoi un petit écran plutôt qu'une dalle 4,3"** — au-delà de l'allure, une
dalle de 4,3" tire 150 à 250 mA en permanence et réchauffe la base de quelques
degrés. Or la dérive thermique des cellules de charge est exactement ce que
l'algorithme de la section 9 passe son temps à compenser. Sur la cible ideaspark,
le rétroéclairage est d'ailleurs piloté en PWM et volontairement réglé à 170/255
(`TFT_BACKLIGHT` dans [config.h](src/config.h)).

## 2. Câblage

Tout est en 3,3 V. **N'alimente pas les HX711 en 5 V** : leur sortie DOUT
suivrait le 5 V et attaquerait les GPIO de l'ESP32 hors spécification. On perd un
peu de signal en 3,3 V, largement compensé par la résolution du HX711.

Sur la carte ideaspark, l'écran occupe déjà les GPIO 2, 4, 15, 18, 23 et 32 : il
ne reste plus qu'à câbler les HX711 et le bouton.

| Signal | ideaspark (ST7789) | ESP32-WROOM + OLED | ESP32-S3 + OLED |
|---|---|---|---|
| HX711 **A** — DOUT | 16 | 16 | 4 |
| HX711 **A** — SCK | 17 | 4 | 5 |
| HX711 **B** — DOUT | 25 | 17 | 6 |
| HX711 **B** — SCK | 26 | 5 | 7 |
| OLED — SDA / SCL | — | 21 / 22 | 8 / 9 |
| Bouton | 27 | 27 | 10 |
| VCC des HX711 | 3V3 | 3V3 | 3V3 |
| GND | GND commun, obligatoire | | |

Cellule de charge → HX711, avec le code couleur le plus répandu :

| Fil | Borne HX711 |
|---|---|
| rouge | E+ |
| noir | E− |
| blanc | A− |
| vert | A+ |

> Si le poids diminue quand tu appuies sur le plateau, inverse vert et blanc sur
> ce HX711. Les codes couleur varient d'un fabricant à l'autre.

Le brochage se change en haut de [config.h](src/config.h). Si tu n'as qu'une
seule cellule, mets `NUM_CHANNELS` à `1` : seul le HX711 A est lu, mais le
tonneau devra être bien centré dessus.

## 3. La base mécanique — la partie qui compte vraiment

Une cellule à flexion ne mesure que si elle peut **fléchir**. Elle se monte en
porte-à-faux : une extrémité vissée sur la plaque du bas, l'autre sur la plaque
du haut, avec de l'air entre les deux.

```
        plateau (le tonneau est posé dessus)
   ═══════════════════════════════════════════
     │ entretoise            entretoise │
     ▓▓▓▓▓▓▓▓▓▓▓            ▓▓▓▓▓▓▓▓▓▓▓        <- cellules de charge
              │ entretoise │
   ═══════════════════════════════════════════
        plaque de fond (posée au sol)
```

Quatre règles, dans l'ordre d'importance :

1. **Le plateau ne doit toucher que les cellules.** Aucun autre point de contact,
   aucun câble coincé, aucune vis qui frotte : tout appui parasite se soustrait
   directement de la mesure.
2. **Entretoises des deux côtés de chaque cellule**, pour que la barre travaille
   dans le vide. Une cellule vissée à plat sur toute sa longueur ne mesure rien.
3. **Serre fort.** Une vis qui se desserre lentement, c'est une dérive lente.
4. **Cellules symétriques**, de part et d'autre du centre, et le tonneau posé
   toujours au même endroit — c'est ce qui rend une pente de calibration unique
   suffisante.

Passe les câbles sans tension et fixe l'électronique sur la plaque du bas.

## 4. Compiler et flasher

Trois cibles, une seule base de code. Choisis la tienne avec `-e` :

```bash
pio run -e ideaspark -t upload
```

```bash
pio run -e esp32dev -t upload
```

```bash
pio run -e esp32s3 -t upload
```

`ideaspark` = ESP32-WROOM-32 + écran couleur ST7789 1,9". `esp32dev` = ESP32-WROOM-32
+ OLED SSD1306. `esp32s3` = ESP32-S3 nu + OLED SSD1306. Le brochage de chacune est
dans [config.h](src/config.h), l'interface locale dans
[ui_tft.cpp](src/ui_tft.cpp) ou [ui_oled.cpp](src/ui_oled.cpp) selon la cible.

Le HTML de l'app est embarqué dans le firmware, il n'y a **pas** de
`pio run -t uploadfs` à faire. LittleFS ne sert qu'à l'historique et se formate
tout seul au premier démarrage.

Console série pour suivre ce qui se passe :

```bash
pio device monitor
```

Pour retoucher l'app sans flasher, un simulateur sert la même page avec des
données inventées — 95 jours d'historique et une dizaine de services :

```bash
python3 tools/mock_ui.py
```

## 5. Premier démarrage

1. L'ESP32 ouvre un réseau Wi-Fi ouvert nommé **`RumBarrel-setup`**, et l'écran
   l'affiche.
2. Avec un téléphone, rejoins ce réseau : la page de configuration s'ouvre
   d'elle-même. Choisis le Wi-Fi de la maison et saisis le mot de passe.
3. L'ESP32 redémarre sur le réseau. Appuie sur le bouton jusqu'à la page réseau :
   elle affiche un **QR code**. Le scanner ouvre l'app. C'est la façon la plus
   simple de la donner à quelqu'un sans savoir ce qu'il a comme téléphone.
4. L'app est aussi accessible sur `http://rum.local` et sur l'IP affichée.

Si le Wi-Fi n'est pas configuré dans les 5 minutes, l'appareil reste en point
d'accès autonome : l'app fonctionne en s'y connectant, mais sans alertes
Telegram.

## 6. Initialisation (à faire une fois)

Onglet **Réglages** de l'app, section *Initialisation*. Le poids brut s'affiche
en direct en haut, ce qui permet de vérifier chaque étape.

1. **Base vide** → *Faire le zéro*.
2. **Poids connu** → pose une bouteille d'eau d'un litre pleine (1000 g), saisis
   la masse, *Calibrer*. Attends deux ou trois secondes que la valeur se pose.
3. **Tonneau vide** → pose le tonneau vide, bonde et robinet compris, attends la
   stabilisation, *Enregistrer le tonneau vide*.
4. **Tonneau plein** → remplis, repose, attends, saisis la capacité, *Enregistrer
   le tonneau plein*. La densité réelle du liquide est calculée au passage.

> **Tonneau neuf** : le bois s'imbibe pendant les premières semaines et gagne
> plusieurs centaines de grammes. Le niveau affiché sera alors un peu optimiste.
> Quand le tonneau sera vidé une première fois, refais l'étape 3 avec le tonneau
> vide mais imbibé : les mesures suivantes seront justes.

## 7. Au quotidien

**Le bouton :**

- appui court → page suivante de l'écran (niveau → vieillissement → réseau →
  diagnostic, puis retour automatique au bout de 30 s) ;
- appui maintenu 3 s → l'écran demande confirmation, une barre se remplit ;
- maintenu jusqu'à 6 s → le compteur de vieillissement repart à J+0 ;
- relâché avant la fin → rien ne se passe.

**L'app** se met à jour en direct par WebSocket. Le graphique montre le niveau en
litres, un point par heure, avec les marches d'escalier de chaque service.

## 8. Alertes Telegram

1. Sur Telegram, écris à **@BotFather**, envoie `/newbot`, suis les questions,
   récupère le jeton.
2. Écris un message à ton nouveau bot (sinon il ne peut pas t'écrire en premier).
3. Ouvre **@userinfobot** pour lire ton identifiant numérique.
4. Colle les deux dans l'app, *Enregistrer*, puis *Envoyer un test*.

Le jeton est stocké dans l'ESP32 et n'est jamais renvoyé par l'API — l'app
affiche seulement s'il est configuré ou non.

## 9. Comment le niveau est calculé

C'est le cœur du projet, et ce n'est pas une simple soustraction.

Une cellule de charge chinoise qui porte une charge statique pendant des mois
**dérive** : la température fait varier la sensibilité, et le métal flue sous
contrainte constante. Afficher le poids absolu brut donnerait un niveau qui
s'éloigne lentement de la réalité.

Le firmware raisonne donc en **paliers** :

- 10 mesures par seconde et par cellule, médiane glissante sur 15 valeurs puis
  lissage exponentiel ;
- quand le poids reste dans une fenêtre de 12 g pendant 20 s, le palier est figé ;
- l'écart avec le palier précédent devient un **événement** ;
- moins de 25 g d'écart → de la dérive, absorbée sans rien signaler ;
- une baisse importante mais très étalée (moins de 25 g/jour) → la **part des
  anges**, comptabilisée à part, sans alerte ;
- une baisse franche → un **service**, journalisé et notifié ;
- une hausse franche → un **remplissage**.

Un verre posé sur la base produirait une hausse puis une baisse équivalentes. Un
événement attend donc 3 minutes avant d'être validé : si son opposé arrive
entre-temps, les deux disparaissent. C'est pour cela qu'une alerte de service
arrive quelques minutes après le fait, et non instantanément.

Tous ces seuils sont regroupés et commentés dans [config.h](src/config.h).

## 10. Après une coupure de courant

Rien d'important ne vit en RAM. Voici précisément ce qui se passe au rallumage.

**Conservé tel quel** (NVS, survit à tout sauf une remise à zéro d'usine) : la
tare et la pente de calibration, les poids vide et plein, la capacité, la densité,
le nom, le jeton Telegram, le cumul d'évaporation, le dernier seuil d'alerte
franchi — et **la date de début de vieillissement**.

**Le compteur de jours est recalculé, pas incrémenté.** On stocke une date
absolue, pas un compteur : l'appareil peut rester débranché trois semaines, au
retour il affichera le bon nombre de jours. C'est aussi pourquoi il a besoin de
l'heure — voir plus bas.

**Conservé sur LittleFS** : l'historique du niveau et le journal des événements.
Le dernier service affiché est relu depuis ce journal au démarrage.

**Ce qui reste inévitablement perdu :**

- **Un trou dans la courbe** pendant la durée de la coupure. Le graphique relie
  les deux points de part et d'autre par une droite.
- **Les services survenus pendant la coupure ne déclenchent aucune alerte.** Au
  premier palier stable, le firmware compare le poids au dernier point
  d'historique. Si la baisse est lente, elle est mise au compte de la part des
  anges. Si elle est trop rapide pour ça, il consigne un manque d'origine
  indéterminée mais **n'invente jamais un service** : un tonneau soulevé et
  reposé pendant la coupure suffirait à en fabriquer un faux.
- **Jusqu'à six heures de cumul d'évaporation**, qui n'est sauvegardé qu'à
  intervalles espacés pour épargner la mémoire NVS. Quelques grammes.

**Si le Wi-Fi ne revient pas**, l'appareil n'a aucun moyen de connaître l'heure —
il n'y a pas de pile de sauvegarde. Dans ce cas l'écran affiche « en cours » et
non « J+0 » : rien n'est perdu, la date de départ est bien en mémoire, seul le
calcul attend l'heure. Et si tu remets le vieillissement à zéro dans cet état, la
demande est enregistrée puis datée dès que l'heure arrive.

Ajouter un module RTC DS3231 sur l'I²C serait la façon de rendre l'appareil
complètement autonome vis-à-vis du réseau.

> **Note pour le développement** : toute modification de la structure `Settings`
> dans [store.h](src/store.h) oblige à incrémenter `MAGIC` dans
> [store.cpp](src/store.cpp), et cela force une recalibration. C'est voulu :
> relire un ancien format de travers donnerait une balance qui mesure faux sans
> le dire.

## 11. Dépannage

| Symptôme | Piste |
|---|---|
| Page *Diagnostic* : `HX A:--` | câblage DOUT/SCK, ou HX711 non alimenté |
| Le poids diminue quand on appuie | inverse les fils vert et blanc de cette cellule |
| Valeur qui saute de plusieurs dizaines de grammes | câble de cellule qui bouge, vis desserrée, ou plateau qui touche autre chose que les cellules |
| Poids toujours à 0 | il manque le GND commun entre l'ESP32 et les HX711 |
| Image décalée horizontalement sur le ST7789 | c'est le décalage de colonne : la dalle fait 170 px sur un contrôleur câblé pour 240. Essaie `TFT_COL_OFFSET` à `0` au lieu de `35` dans [config.h](src/config.h) |
| Écran ST7789 noir | vérifie `TFT_BACKLIGHT` (0 = éteint) et que la carte est bien la version 1,9" 170×320 |
| `esp32-hal-periman.h: No such file` à la compilation | Arduino_GFX a été remonté au-delà de la 1.4.x. Depuis la 1.5, il exige le core Arduino 3.x, absent de la plateforme espressif32 6.x. Garde la borne `~1.4.0` dans platformio.ini |
| Écran OLED noir | adresse I²C : certains modules sont en 0x3D. `u8g2.setI2CAddress(0x3D * 2)` dans `Ui::begin()` |
| `heure non synchronisée` | pas d'accès Internet ; le niveau fonctionne, mais les dates et le compteur de jours attendent le NTP |
| Le % dépasse 100 après un remplissage | refais l'étape 4 : le repère « plein » date d'avant |
| Rien dans le graphique | normal au début, un point par heure |

## 12. Ce que ce projet ne fait pas

- **Aucune authentification sur l'app.** Toute personne sur le réseau local peut
  l'ouvrir, et donc recalibrer la balance. C'est un tonneau, pas un coffre-fort —
  mais ne l'expose pas sur Internet, et ne redirige aucun port vers lui.
- **Pas de compensation de température.** Ajouter un DS18B20 collé sur une
  cellule et corriger la pente serait la première amélioration à faire si la
  dérive saisonnière devient gênante.
- **Pas d'horloge sauvegardée.** Après une coupure de courant, les dates
  reviennent avec le Wi-Fi et le NTP. Le compteur de vieillissement est stocké en
  date absolue, il ne se perd pas.
