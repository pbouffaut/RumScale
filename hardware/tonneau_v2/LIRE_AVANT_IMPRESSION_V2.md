# Tonneau ESP32 — V2 compacte, carte vissée

Cette version intègre les mesures fournies après l'impression de la V1: carte de **32,6 mm** de large, ensemble écran/carte de **5 mm** d'épaisseur, entraxe de fixation de **26 mm**, boutons Reset/Boot contenus dans une largeur de **22 mm**, écran utile de **46 × 25 mm**.

## Modifications

| Élément | V1 | V2 |
|---|---:|---:|
| Longueur du tonneau | 96 mm | 74 mm |
| Largeur extérieure | Environ 88 mm | Environ 82 mm |
| Hauteur extérieure | 78 mm | 75 mm |
| Retrait nominal du port USB par rapport au bout du tonneau | 16,1 mm | 5,1 mm |
| Passage USB intérieur | 16 × 12 mm | 13 × 8 mm |
| Bouche USB extérieure | 18 × 14 mm | 13,8 × 8,8 mm |
| Fenêtre écran | 52 × 26,6 mm | **46 × 25 mm** |
| Position de la fenêtre | Référence V1 | Décalée de **3 mm vers l'USB** pour masquer la marque |
| Largeur du logement | 33 mm | 34 mm pour la carte de 32,6 mm |
| Fixation de la carte | Colle chaude | **Deux supports de vis intégrés**, entraxe 26 mm |

Le plan de l'écran reste à **30° de la table**. Le fond reste plat, les fils sortent par le passage arrière de 10 mm, et les deux coques se ferment avec quatre clips sans colle. Le volume prévu derrière la carte reste profond de **36 mm**. Les clips sont placés plus bas pour que leur flexion ne touche ni les nouveaux supports, ni la carte, ni les têtes de vis.

**Les coques V1 et V2 ne sont pas interchangeables. Il faut réimprimer les deux moitiés V2.** Les fichiers V1 sont conservés séparément.

## Fichiers

- `01_demi_tonneau_bas_v2.stl`: imprimer une fois.
- `02_demi_tonneau_haut_v2.stl`: imprimer une fois.
- `03_gabarit_carte_et_vis_v2.stl`: essai de la fenêtre, du logement et des deux supports de vis.
- `04_test_clip_male_v2.stl` et `05_test_clip_femelle_v2.stl`: essai facultatif du nouvel emplacement des clips.
- `tonneau_v2_parametrique.py`: modèle modifiable.
- `verification_geometrique.json`: contrôle des solides et des STL relus après export, avec empreinte de chaque fichier.

Unités: **millimètres**, échelle **100 %**. Les STL sont déjà orientés pour l'impression: bas sur son fond plat; haut sur son plan de joint; gabarit face visible sur le plateau.

## Fixation de la carte

Prévoir **deux vis M2 × 5 mm**, à tête plate dessous (tête cylindrique ou bombée, pas fraisée). Les avant-trous des supports font **1,6 mm** et sont aveugles. Visser doucement dans le plastique, sans insert ni écrou. Le vissage se fait depuis le dos du circuit imprimé, à l'intérieur de la coque haute.

Les deux plots portent sur le PCB côté écran, autour des deux trous de part et d'autre de l'USB. Ils ont un méplat vers les boutons pour laisser 0,5 mm autour de leur enveloppe mesurée. Les vis et les plots restent en dehors de la zone visible de l'écran.

Le doré dans l'aperçu intérieur sert uniquement à repérer les plots: ceux-ci sont imprimés avec la coque, sans pièce métallique à ajouter. Les couleurs des cerclages sont également illustratives.

1. Imprimer le gabarit avec le filament et les réglages destinés au boîtier.
2. Insérer la carte par l'arrière, USB vers les deux plots. Vérifier que la fenêtre montre l'écran, couvre la marque et n'appuie pas sur le verre.
3. Présenter les deux vis dans les trous de la carte. Vérifier leur alignement avec les avant-trous et le contact des deux plots avec le PCB. Ne pas tirer sur la carte avec les vis pour corriger un défaut de position.
4. Serrer seulement jusqu'au maintien de la carte. Vérifier que le cadre de l'écran repose sur sa butée sans flexion du PCB ou pression sur le verre.
5. Après impression de la coque haute, monter la carte et vérifier l'insertion de la fiche USB avant de refermer le tonneau. Brancher les Dupont, ranger les fils loin des clips, puis emboîter les deux coques.

Le tunnel USB laisse passer un surmoulage jusqu'à 13 × 8 mm en nominal. Le centrage réel de la prise et le jeu d'impression réduisent cette enveloppe; essayer la fiche utilisée. Une fiche de 6–7 mm de haut laisse davantage de marge qu'une fiche de 8 mm. Le port est rapproché de 11 mm de la paroi par rapport à la V1.

## Impression

Conserver un profil d'impression déjà maîtrisé. Point de départ: buse 0,4 mm, couches 0,20 mm, quatre parois, cinq couches pleines dessus/dessous et 15–20 % de remplissage. Le PETG offre davantage de souplesse aux clips; avec un filament bois ou du PLA, vérifier leur comportement sur le coupon.

La coque haute demande des supports sous la voûte et certains rebords; vérifier également les surplombs des extrémités du bas. Éviter d'enfermer des supports dans les avant-trous de vis. Retirer toutes les bavures avant d'emboîter les coques. Le gabarit est conçu pour être imprimé face visible sur le plateau, sans support sous les plots.

## Mesures et limites

Mesures utilisateur intégrées: largeur 32,6 mm, épaisseur totale 5 mm, entraxe 26 mm, largeur extérieure des boutons 22 mm et écran 46 × 25 mm. La longueur de 63,8 mm est celle de l'image produit initiale.

Les cotes non mesurées directement restent explicites: épaisseur du PCB supposée de **1,6 mm**, axe des trous situé à **2,1 mm du petit bord côté USB**, centre en hauteur du port estimé d'après les photos. Le diamètre réel des trous de la carte n'a pas été fourni; les vis M2 sont choisies pour laisser du jeu dans les trous visibles. Ces points sont vérifiables sur le gabarit avant les grandes pièces.

Les vérifications numériques portent sur les maillages, l'emboîtement au repos, l'espace derrière la carte et le dégagement des clips vis-à-vis de la carte, des plots et des têtes de vis. Une seconde lecture indépendante des STL a confirmé leur fermeture et le dégagement des quatre clips sur une enveloppe continue de flexion de 1,3 mm. Une fiche théorique de 12 × 6 mm passe sans collision. Ces contrôles ne constituent pas un essai physique de cette V2 ni une simulation de résistance des matériaux.

Pour régénérer, installer les dépendances de `requirements_cad_v2.txt` puis exécuter `tonneau_v2_parametrique.py` avec Python 3.11 ou plus récent. Les mesures et les hypothèses figurent dans le dictionnaire `P` au début du fichier.
