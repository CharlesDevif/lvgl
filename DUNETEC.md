# Branche `dunetec/9.5.0`

LVGL **v9.5.0** plus les correctifs que le projet Dunetec a isolés sur
STM32U5G9J-DK2 (NeoChrom GPU2D / NemaVG) et STM32F769I-DISCO.

La base est le tag `v9.5.0`, pas `master` : le projet fige une version, et un
correctif se juge sur la version qu'on livre. `amont` reste configuré pour
suivre lvgl/lvgl.

```
git remote -v
  origin  https://github.com/CharlesDevif/lvgl.git   (ce fork)
  amont   https://github.com/lvgl/lvgl.git           (le dépôt d'origine)
```

## Ce que la branche ajoute

| # | Correctif | État en amont |
|---|-----------|---------------|
| 1 | `nema_gfx` : poser la règle de remplissage avant un fond dégradé | **PR #10688** ouverte |
| 2 | `nema_gfx` : résoudre les pourcentages des dégradés radiaux | à soumettre |
| 3 | `nema_gfx` : dessiner les dégradés coniques au lieu de noir | à soumettre |
| 4 | `nema_gfx` : `LV_MIN` et non `LV_MAX` sur le nombre de paliers | à soumettre |
| 5 | `draw` : attendre le GPU même sans système d'exploitation | à soumettre |
| 6 | `draw` : ne pas figer quand un tampon de couche manque | porté depuis `amont/master` |

### 1 — Règle de remplissage

`lv_draw_nema_gfx_vector.c` laisse `NEMA_VG_STROKE` posé après chaque tracé
qui a un contour, et ne le restaure pas. `lv_draw_nema_gfx_fill.c` ne pose
jamais la règle. Un pictogramme vectoriel en trait seul laissait donc le GPU
en mode contour, et le fond dégradé suivant sortait détouré au lieu d'être
rempli. Un état global qu'on croit local.

### 2 — Pourcentages des dégradés radiaux

La branche linéaire résout ses coordonnées avec `lv_pct_to_px()`, et
`lv_draw_sw_grad_radial_setup()` fait de même pour les radiales. La branche
radiale du pilote GPU les prenait brutes. `LV_GRAD_CENTER` vaut `LV_PCT(50)`,
soit l'entier 536 870 962 : la façon documentée de centrer un dégradé radial
était précisément celle qui le cassait. Le rayon était aussi pris comme la
composante horizontale du vecteur d'extension au lieu de sa longueur.

### 3 — Dégradés coniques

Aucune branche n'existait pour `LV_GRAD_DIR_CONICAL` : l'objet de peinture
gardait l'état laissé par `nema_vg_paint_clear()` et le fond sortait noir,
sans le moindre avertissement. NemaVG expose `NEMA_VG_PAINT_GRAD_CONICAL`,
donc le centre est honoré. L'API ne prend pas de plage angulaire : un dégradé
défini sur une portion de tour est rendu sur le tour entier. C'est écrit dans
le code.

### 4 — Nombre de paliers

`lv_nemagfx_grad_set()` bornait sa boucle avec `LV_MAX(stops_count,
LV_GRADIENT_MAX_STOPS)` alors que les tableaux font `LV_GRADIENT_MAX_STOPS`.
Au-delà de la limite, la boucle écrit après la fin de deux tableaux de pile.
En deçà, elle lit la queue non initialisée du tableau de paliers et la donne
au GPU. `lv_draw_nema_gfx_fill.c` borne la même valeur avec `LV_MIN`.

La fonction n'a aucun appelant dans l'arbre : le débordement n'est pas
atteignable aujourd'hui, mais le symbole est exporté.

### 5 — Attente du GPU sans système d'exploitation

Le corps entier de `lv_draw_wait_for_finish()` était sous `#if LV_USE_OS`.
C'est juste pour le rendu logiciel, qui dessine dans son rappel de répartition
et n'a rien en attente. Ça ne l'est pas pour un accélérateur : `lv_draw_nema_gfx`
enregistre un `wait_for_finish_cb` qui soumet la liste de commandes et attend
le GPU, et ce rappel n'était jamais appelé sans système d'exploitation.

Conséquence observée : le rappel de vidage arme la permutation de tampon du
LTDC alors que le NeoChrom écrit peut-être encore dans le tampon que le
contrôleur va balayer. La course est courte — le retour de trame la couvre le
plus souvent — ce qui est exactement ce qui la rend pénible à traquer.

Les unités de dessin qui n'enregistrent pas le rappel ne changent pas de
comportement ; le rendu logiciel n'en enregistre aucun.

### 6 — Gel quand un tampon de couche ne s'alloue pas

Quand `lv_draw_layer_alloc_buf()` rend NULL, les unités de dessin rendaient
`LV_DRAW_UNIT_IDLE` en laissant la tâche dans son état précédent. Plus rien ne
la faisait avancer : jamais redistribuée avec un tampon, jamais retirée. La
couche ne se déclarait donc jamais terminée, et l'image non plus.

Avec un système d'exploitation, l'appelant se bloque indéfiniment sur le
sémaphore de dessin — processus vivant, 0 % de CPU, rien d'affiché. Sans, la
boucle de rafraîchissement tourne sur la même couche. Dans les deux cas
l'interface est figée, et la seule cause visible est qu'une couche dépassait
ce que `LV_MEM_SIZE` autorise : un objet avec une transformation ou une
opacité partielle suffit.

La correction ajoute `LV_DRAW_TASK_STATE_FAILED`, le pose partout où une
allocation de couche échoue, et retire les tâches en échec comme les tâches
finies en journalisant l'erreur. L'image se termine alors avec cette tâche en
moins — dégradé visible, donc récupérable — au lieu de ne pas se terminer.

Mesuré : un objet de 300 × 300 transformé avec `LV_MEM_SIZE` à 48 Ko bloquait
le programme de test 2 min 49 s à 0 % de CPU ; après correction il rend la
main immédiatement en journalisant la tâche en échec.

Porté depuis `amont/master`, qui porte le même changement. Absent de la
v9.5.0 sur laquelle cette branche est basée.

## Mesures sur carte

STM32U5G9J-DK2, démonstration Dunetec, après les cinq correctifs :

```
écran GPU (cube en perspective)   60,6 i/s   0 famine FIFO   aucune faute
7 allers-retours entrée/sortie    60,3 i/s   liste de commandes recyclée
```

## Ce qui reste ouvert

`lv_draw_image.c`, branche « child layer » : `layer->_clip_area` est écrasé
par la zone de l'image et jamais restauré. Tout ce qui est dessiné après dans
la même couche devrait s'en trouver découpé. **Non confirmé** : le banc monté
pour le démontrer n'atteint pas le rendu, la couche dépassant `LV_MEM_SIZE`
(c'est ce qui a mené au correctif 6). À reprendre avec une couche plus petite.

L'issue amont #9778 — pictogrammes SVG qui disparaîtraient sous un
`transform_scale` de parent — **ne se reproduit pas** ici. Encre mesurée en
pixels sur un tracé identique :

```
sans parent                  1412        parent transform_scale 200    320
parent sans transformation   1412        parent transform_scale 128    144
parent transform_scale 256   1412
```

L'icône rétrécit, elle ne s'efface pas. Aucun correctif spéculatif n'est
porté ici pour un défaut qu'on n'observe pas.

## Remonter un correctif en amont

```
git checkout -b fix/<sujet> amont/master
git cherry-pick <commit de cette branche>
```

Les messages de commit sont rédigés en anglais et prêts pour une PR.
