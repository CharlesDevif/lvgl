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

## Mesures sur carte

STM32U5G9J-DK2, démonstration Dunetec, après les cinq correctifs :

```
écran GPU (cube en perspective)   60,3 i/s   0 famine FIFO   aucune faute
7 allers-retours entrée/sortie    60,3 i/s   liste de commandes recyclée
```

## Remonter un correctif en amont

```
git checkout -b fix/<sujet> amont/master
git cherry-pick <commit de cette branche>
```

Les messages de commit sont rédigés en anglais et prêts pour une PR.
