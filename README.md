# Boulderdash Reborn

Remake personnel et non officiel de Boulder Dash en C++17 avec SDL2 et CMake.
Architecture allégée :

- `core/` : données et règles du jeu (grille, cellules, état global)
- `systems/` : logique déterministe (joueur, gravité, ennemis plus tard)
- `render/` : affichage SDL2 uniquement

## Build

```bash
cmake -S . -B build
cmake --build build
```

Exécuter `./build/boulderdash`.

### Lancement et administration

Pour compiler si nécessaire puis lancer directement le jeu :

```bash
./run.sh
```

Le script utilise UV pour créer automatiquement l’environnement Python 3.12 et
installer les outils versionnés dans `uv.lock`.

Pour ouvrir le menu d’administration interactif :

```bash
uv run manage.py
```

Les mêmes actions sont disponibles sans interaction, par exemple
`uv run manage.py test`, `uv run manage.py verify` ou
`uv run manage.py status`.

### Build avec Conan (nlohmann::json)

Conan 2.31.2 est fourni par l’environnement UV du projet. Synchroniser les
outils puis configurer CMake :

```bash
uv sync
uv run manage.py configure
uv run manage.py build
```

`find_package(nlohmann_json CONFIG REQUIRED)` est utilisé côté CMake. Sans Conan, un paquet système `nlohmann_json` suffit (CONFIG package).

### Dépendances

- SDL2
- SDL2_mixer
- nlohmann_json (via Conan ou paquet système)
- SDL2_ttf (optionnel mais recommandé pour l’affichage UTF-8 complet)

## Configuration

Les paramètres runtime sont dans `cfg/config.json` :

- `tileSize` : taille d'une tuile (pixels)
- `tickMs` : fréquence de mise à jour du gameplay
- `moveRepeatMs` : délai minimum entre deux déplacements automatiques quand on maintient une direction
- `timeLimitMs` : temps disponible par niveau (millisecondes)
- `diamondValue` : score gagné par diamant ramassé
- `exitBonus` : bonus fixe à la fin d'un niveau
- `timeBonusPerSecond` : bonus de score par seconde restante
- `logLevel` : `trace`,`debug`, `info`, `warn`, `error`
- `gameOverDelayMs` : durée d'affichage de l'écran "Game Over" avant retour au menu (en millisecondes)
- `language` : code de langue pour les textes UI (charge `assets/i18n/<lang>.json`)
- `lives` : nombre de vies/essais disponibles au démarrage
- `fullscreenToggleKey` : touche qui bascule le plein écran (nom SDL lisible, ex. `f` par défaut)

Les niveaux (`assets/levels/level*.txt`) sont enchaînés automatiquement. Le HUD affiche niveau courant, score, timer et état de l'objectif (diamants restants / sortie ouverte / niveau terminé), sans dépendre du titre de fenêtre.

L'interface embarque maintenant un menu principal/New Game/Options/Exit et un écran "Game Over" qui revient automatiquement vers le menu après le délai configuré. Les libellés sont traduits via des fichiers JSON simples (`assets/i18n/en.json`, `assets/i18n/fr.json` par défaut) : ajouter un nouveau langage consiste à fournir un fichier supplémentaire portant le code ISO désiré.

Une musique de menu peut être déposée dans `assets/theme/` (nommée `bd_theme_menu.ogg` ou `bd_theme_menu.wav`). Si elle est présente, elle sera jouée en boucle tant que l'on reste dans les écrans de menu.

### Police UI (UTF-8)

Pour afficher correctement les accents et les caractères CJK, placez une police TTF dans `assets/ui/ui_font.ttf` (ex : Noto Sans CJK). Si SDL2_ttf n’est pas présent ou si le fichier manque, le moteur retombe sur la bitmap ASCII historique.

## Gameplay

- `d` représente la terre : la taupe peut la creuser pour avancer et elle supporte les rochers tant qu'elle reste dessous.
- `b` représente un mur destructible : il bloque le joueur comme un mur classique mais il peut être détruit par une explosion (rocher qui tombe, ennemi ou joueur).
- `X` représente un ennemi qui se déplace horizontalement et tue le joueur au contact.
- Le joueur dispose d'un nombre limité de vies (défini dans `config.json`) avant le Game Over.
- Être touché par un ennemi ou se faire écraser par un rocher qui tombe provoque l'échec immédiat du niveau.
- Une couche audio simple (SDL2_mixer) joue les SFX pour la marche, le creusement, la chute d'un rocher, la collecte d'un diamant et la mort du joueur. Les fichiers attendus se trouvent dans `assets/sfx/`.

## Système de menus data-driven

Un framework de menus indépendant de SDL est disponible dans `src/menu/`. Les menus sont décrits en JSON (ex. `cfg/main_menu.json`), chargés via `menu::MenuLoader` et rendus via une implémentation d'`IMenuRenderer`. La documentation détaillée (architecture, exemple d'intégration, mocks de tests) se trouve dans `docs/MenuSystem.md`.

## TODO

L’état détaillé, les travaux terminés et les prochaines priorités sont suivis
dans [`ROADMAP.md`](ROADMAP.md).

## Tests

Après configuration du build, compiler puis lancer les contrôles automatisés :

```bash
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Les tests couvrent actuellement les systèmes principaux, la navigation des
menus, la structure des niveaux et la cohérence des traductions.

## INSTALL REMARKS
j ai du faire
```bash
sudo dnf install zlib-ng-compat-static
```
pour installer ZLIB (requis par la lib PNG apparement)

FindPNG déclenche find_package(ZLIB) en mode “CONFIG” (il charge /usr/lib64/cmake/ZLIB/zlib-config.cmake), et ce ZLIBConfig.cmake Fedora essaie toujours de définir ZLIB::zlibstatic vers /usr/lib64/libz.a (absent).
