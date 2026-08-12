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

Si `fzf` est disponible, le menu se parcourt avec les flèches haut/bas et se
filtre directement en saisissant quelques lettres. Entrée lance l’action et
Échap ferme la console. Sans `fzf`, le menu numérique reste disponible.

Les mêmes actions sont disponibles sans interaction, par exemple
`uv run manage.py test`, `uv run manage.py verify` ou
`uv run manage.py status`.

Pour repartir d’un build vierge :

```bash
uv run manage.py clean
uv run manage.py rebuild
uv run manage.py clean-web
uv run manage.py rebuild-web
```

### Build WebAssembly

Installer une copie locale d’[Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html),
puis configurer et compiler :

```bash
uv run manage.py install-web-sdk
uv run manage.py configure-web
uv run manage.py build-web
uv run manage.py verify-web
uv run playwright install chromium
uv run manage.py smoke-web
```

La version du SDK est épinglée à Emscripten 6.0.6. Conan utilise le profil
cross-compilation versionné `profiles/emscripten`. Le smoke test ouvre Chromium,
attend le menu, simule Entrée et vérifie le chargement du niveau 1.

Pour compiler, vérifier, démarrer un serveur local et ouvrir le navigateur :

```bash
./web.sh
```

Le serveur utilise par défaut `http://127.0.0.1:8000/boulderdash.html`.
Un autre port peut être choisi avec `./web.sh --port 8080`.
Comme l’exigent les navigateurs, le contexte audio est activé au premier clic
ou au premier appui sur une touche dans la page.

### Validation stricte et CI

Pour reproduire localement les avertissements traités comme erreurs :

```bash
BOULDERDASH_STRICT_WARNINGS=1 uv run manage.py verify
BOULDERDASH_STRICT_WARNINGS=1 uv run manage.py smoke-web
```

Le workflow `.github/workflows/ci.yml` exécute ces validations sur Linux pour
le build desktop et pour WebAssembly dans Chromium. Les tests natifs sont aussi
recompilés et exécutés avec AddressSanitizer et UndefinedBehaviorSanitizer.

Pour reproduire cette validation mémoire localement :

```bash
uv run conan install . --output-folder=build-sanitizers --build=missing -s build_type=Debug -s compiler.cppstd=17
cmake -S . -B build-sanitizers \
  -DCMAKE_TOOLCHAIN_FILE=build-sanitizers/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBOULDERDASH_SANITIZERS=ON
cmake --build build-sanitizers --target boulderdash_tests --parallel
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 \
  ctest --test-dir build-sanitizers --output-on-failure
```

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
- SDL2_ttf (requis pour l’affichage UTF-8, notamment le japonais)

## Configuration

Les paramètres runtime sont dans `cfg/config.json` :

Le fichier est analysé avec un parseur JSON strict. Les clés absentes conservent
leur valeur par défaut ; un JSON mal formé, un type incorrect ou une valeur hors
limites provoque une erreur indiquant le fichier et la clé concernés.

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

Pendant une partie, `Échap` ou `P` ouvre le menu de pause. Le timer, la gravité,
les ennemis et les entrées sont gelés ; le temps passé en pause n'est pas ajouté
lors de la reprise. Ce menu permet de reprendre, d'ouvrir les Options — notamment
pour régler le son — ou de revenir explicitement au menu principal. Dans Options,
`Échap` revient à la pause sans perdre la partie en cours.

Toutes les cartes livrées ont un format fixe de **40 colonnes × 22 lignes**.
Les tests refusent une ligne trop courte ou trop longue, une ligne vide, une
hauteur différente ou un caractère inconnu. Le fichier `.editorconfig` impose
également les fins de ligne LF et supprime les espaces de fin de ligne.
`uv run manage.py validate-levels` permet d'effectuer ce contrôle immédiatement.
Le lancement depuis `manage.py` exécute ce contrôle avant la compilation et le
chargeur C++ refuse également un niveau ne contenant pas exactement un `P` et un
`E`, sans le remplacer silencieusement par une carte de secours.

L'interface embarque maintenant un menu principal/New Game/Options/Exit, un
écran "Game Over" qui revient automatiquement vers le menu après le délai
configuré et un écran de victoire après le dernier niveau. Les libellés sont
traduits via des fichiers JSON simples (`assets/i18n/en.json`,
`assets/i18n/fr.json` par défaut) : ajouter un nouveau langage consiste à fournir
un fichier supplémentaire portant le code ISO désiré.

Une musique de menu peut être déposée dans `assets/theme/` (nommée `bd_theme_menu.ogg` ou `bd_theme_menu.wav`). Si elle est présente, elle sera jouée en boucle tant que l'on reste dans les écrans de menu.

### Police UI (UTF-8)

La police UTF-8 livrée avec le jeu se trouve dans `assets/fonts/ui_font.ttf`. SDL2_ttf est requis sur desktop et son port Emscripten est activé dans le build Web afin d’afficher les accents et les caractères CJK. La police bitmap historique reste uniquement un secours d’exécution si le fichier TTF ne peut pas être chargé.

## Gameplay

- `d` ou `D` représente la terre : la taupe peut la creuser pour avancer et elle supporte les rochers tant qu'elle reste dessous.
- `E` représente la sortie vers le niveau suivant. Elle reste fermée tant que les
  diamants requis ne sont pas collectés, puis son apparence indique qu'elle est
  ouverte.
- `b` représente un mur destructible : il bloque le joueur comme un mur classique mais il peut être détruit par une explosion (rocher qui tombe, ennemi ou joueur).
- `X` représente un ennemi qui se déplace horizontalement et tue le joueur au contact.
- Le joueur dispose d'un nombre limité de vies (défini dans `config.json`) avant le Game Over.
- Être touché par un ennemi ou se faire écraser par un rocher qui tombe provoque l'échec immédiat du niveau.
- Une couche audio simple (SDL2_mixer) joue les SFX pour la marche, le creusement,
  la chute d'un rocher ou d'un diamant, la collecte d'un diamant, le
  déverrouillage de la sortie, les explosions et la mort du joueur. Les fichiers
  attendus se trouvent dans `assets/sfx/`.

Pendant les 15 dernières secondes d'un niveau, le chronomètre alterne entre
orange et rouge toutes les 500 ms et un bip d'alerte retentit une fois par seconde.

Les associations entre événements et fichiers audio sont centralisées dans
`cfg/audio.json`. Chaque effet accepte un chemin relatif à `assets/` et un volume
de 0 à 128. Ce fichier configure aussi le périphérique audio, le nombre de canaux
de mixage, les volumes généraux `music`/`effects` (de 0 à 100) et les variantes de musique du menu. Dans l’écran Options, `↑/↓` sélectionne la langue, la musique ou les effets, puis `←/→` ou `+/-` modifie la valeur. Hors de cet écran, `+/-` ajuste les deux volumes ensemble. Le son `game_over` est joué une
seule fois lors de l'entrée dans l'écran Game Over.

## Système de menus data-driven

Un framework de menus indépendant de SDL est disponible dans `src/menu/`. Les menus sont décrits en JSON (ex. `cfg/main_menu.json`), chargés via `menu::MenuLoader` et rendus via une implémentation d'`IMenuRenderer`. La documentation détaillée (architecture, exemple d'intégration, mocks de tests) se trouve dans `docs/MenuSystem.md`.
Le code d'exemple est exclu du binaire de production et peut être compilé
séparément avec `-DBOULDERDASH_BUILD_EXAMPLES=ON`.

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

Le code est organisé autour de `boulderdash_core`, bibliothèque de gameplay
sans SDL ni audio. `src/app/` porte l'application SDL, les états d'écran, les
entrées et la cadence de jeu; le `main.cpp` ne fait qu'appeler l'application.

## INSTALL REMARKS
j ai du faire
```bash
sudo dnf install zlib-ng-compat-static
```
pour installer ZLIB (requis par la lib PNG apparement)

FindPNG déclenche find_package(ZLIB) en mode “CONFIG” (il charge /usr/lib64/cmake/ZLIB/zlib-config.cmake), et ce ZLIBConfig.cmake Fedora essaie toujours de définir ZLIB::zlibstatic vers /usr/lib64/libz.a (absent).
