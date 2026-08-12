# Roadmap Boulderdash Reborn

Dernière mise à jour : 12 août 2026.

## État actuel

Le projet est un prototype jouable sur desktop. Le build natif, les tests et la
cible WebAssembly sont opérationnels. Le build Web a été reconstruit avec
Emscripten 6.0.6, puis le menu et le premier niveau ont été chargés dans Firefox
et par le smoke test Chromium automatisé.

| Domaine | État | Validation actuelle |
| --- | --- | --- |
| Gameplay principal | ✅ Fonctionnel | Déplacement, creusement, poussée, gravité, ennemis, score et vies |
| Build desktop | ✅ Fiabilisé | Build CMake vierge réussi sur Fedora ARM64 avec les dépendances Conan existantes |
| Tests automatisés | ✅ Actifs | 21 scénarios, exécutés par CTest, dont une passe ASan/UBSan |
| Données de niveaux | ✅ Contrôlées | Dimensions, bordures, joueur et sortie validés automatiquement |
| Traductions | ✅ Contrôlées | Parité des clés EN/FR/JP validée automatiquement |
| Exécution desktop | ✅ Vérifiée | Démarrage headless avec chargement des assets |
| Packaging Web | ✅ Vérifié | Build Emscripten 6.0.6, menu et niveau 1 rendus dans Firefox et Chromium avec `assets/` et `cfg/` |
| Outils Python/Conan | ✅ Reproductibles | UV, Python 3.12, Conan 2.31.2 et Pillow sont verrouillés |
| Gestion de versions | ✅ Active | `main` suit `origin/main` et l’état initial est publié |
| Visibilité GitHub | ✅ Privée | `gotoo77/boulderdash_reborn` est privé pendant l’audit des assets |
| CI | ✅ Validée | Workflow GitHub Actions desktop + Web/Chromium exécuté avec succès |

## Terminé — fiabilisation initiale

- [x] Forcer la détection de la bibliothèque PNG partagée sur les systèmes où
  le package Fedora référence une archive statique absente.
- [x] Supprimer le chemin d’include SDL2 spécifique à `/usr/include`.
- [x] Déclarer les tests avec CTest.
- [x] Tester les déplacements, la collecte, la poussée d’un rocher, la gravité,
  le demi-tour d’un ennemi et la navigation des menus.
- [x] Valider automatiquement toutes les cartes livrées.
- [x] Refuser à l’exécution les niveaux trop petits, non rectangulaires ou sans
  joueur et sortie uniques.
- [x] Corriger la largeur de `level01.txt`.
- [x] Vérifier automatiquement la parité des catalogues de traduction.
- [x] Ajouter la traduction japonaise manquante pour le plein écran.
- [x] Inclure `cfg/` dans le système de fichiers virtuel Emscripten.
- [x] Ajouter un `.gitignore` pour les builds, caches, environnements virtuels et
  fichiers temporaires.
- [x] Ajouter un lanceur `run.sh` et une console d’administration Python.
- [x] Gérer Python, Conan et Pillow avec UV et un lockfile partagé.
- [x] Ajouter au menu d’administration la configuration, la compilation, la
  vérification et le serveur local WebAssembly.
- [x] Rendre la boucle principale au navigateur, préférer un renderer SDL
  logiciel sur le Web et reprendre Web Audio après la première interaction.
- [x] Ajouter la licence MIT au code et séparer explicitement le statut des assets.

## P0 — rendre chaque livraison reproductible ✅ Terminé

Objectif : tout changement doit être compilé et testé automatiquement.

- [x] Initialiser le dépôt Git sur `main`, configurer le remote et exclure les
  répertoires `build*`, `tools/venv` et les fichiers de travail locaux.
- [x] Publier le premier état sur le dépôt GitHub privé avec la licence MIT et
  l’avertissement de provenance des assets.
- [x] Réinstaller Conan avec le Python actif et régénérer ses fichiers CMake.
- [x] Ajouter une CI Linux qui exécute Conan, CMake, le build et CTest.
- [x] Documenter ou fournir un profil Conan Emscripten.
- [x] Reconstruire la cible Web et vérifier que `/assets` et `/cfg` sont présents
  dans le fichier `.data`.
- [x] Automatiser en CI le smoke test navigateur du menu et d’un niveau.
- [x] Activer les avertissements compilateur (`-Wall -Wextra -Wpedantic`) dans
  la CI, puis les traiter sans masquer les diagnostics.

Critère de sortie : un clone vierge produit les builds desktop et Web documentés,
et toutes les vérifications passent en CI.

## P1 — sécuriser le cycle complet du jeu ✅ Terminé

Objectif : couvrir les règles qui peuvent casser le score ou la progression.

- [x] Tester `Game` : chargement, timer, vies, respawn, score et changement de
  niveau.
- [x] Afficher distinctement les états verrouillé et ouvert de la sortie `E`.
- [x] Corriger le cadrage du sprite joueur pour afficher le personnage en entier.
- [x] Jouer un son dédié lorsque la collecte du dernier diamant déverrouille la sortie.
- [x] Sonoriser la chute des diamants et leur explosion au contact d'un ennemi.
- [x] Alerter pendant les 15 dernières secondes avec un bip et un timer clignotant.
- [x] Refuser avant lancement et à l'exécution les niveaux sans exactement un joueur et une sortie.
- [x] Centraliser les associations audio dans `cfg/audio.json` et sonoriser le Game Over.
- [x] Tester les collisions rocher/joueur et rocher/ennemi ainsi que les
  explosions.
- [x] Ajouter un écran de victoire après le dernier niveau au lieu de fermer
  directement l’application.
- [x] Tester les fichiers de configuration valides, incomplets et corrompus.
- [x] Désactiver `devMode` dans la configuration destinée aux releases.
- [x] Afficher correctement l’interface UTF-8, dont le japonais, sur desktop et Web.
- [x] Séparer les volumes musique/effets et permettre leur réglage dans les Options.
- [x] Ajouter un menu de pause avec reprise, Options et retour explicite au menu
  principal, en gelant le timer et les systèmes pendant toute la navigation.

Critère de sortie : le parcours des quatre niveaux, le game over et la victoire
sont reproductibles par des tests de logique déterministes.

## P2 — réduire la dette technique ✅ Terminé

Objectif : rendre les changements d’interface et de gameplay plus simples.

- [x] Découper `main.cpp` en application, gestion des écrans, gestion des entrées
  et boucle gameplay.
- [x] Retirer la dépendance audio de `Game` et produire des événements consommés
  par la couche SDL.
- [x] Remplacer le parseur JSON manuel de `Config` par `nlohmann_json` avec une
  validation explicite et des messages d’erreur précis.
- [x] Séparer les exemples de menu du binaire de production.
- [x] Nettoyer les fichiers expérimentaux (`tmp_sim*.cpp`, `dev_backup.py`) après
  récupération éventuelle de leur contenu utile.
- [x] Ajouter des tests sous AddressSanitizer et UndefinedBehaviorSanitizer.
- [x] Décomposer les responsabilités auxiliaires de `Application.cpp` et séparer
  les 21 scénarios de test en suites Game/System/Data.

Critère de sortie : le cœur du jeu se compile et se teste sans SDL ni audio, et
le point d’entrée ne porte plus la logique des différents écrans.

## P3 — préparer une version distribuable

- [x] Ajouter une licence MIT au code.
- [ ] Documenter les licences et crédits de tous les assets avant passage en public.
- [x] Remplacer les cinq captures audio CPC identifiées par des SFX procéduraux
  originaux, reproductibles et générés pendant le build.
- [x] Remplacer le son de mort tiers insuffisamment sourcé par un SFX procédural
  original généré par le même synthétiseur.
- [ ] Produire une archive desktop versionnée et une publication Web versionnée.
- [ ] Ajouter les commandes de lancement et les contrôles clavier complets au
  README.
- [ ] Tester au minimum Linux x86_64/ARM64 et les navigateurs Firefox/Chromium.
- [ ] Réduire la taille des assets audio et supprimer les variantes inutilisées.

## Commandes de validation desktop

```bash
conan install . --output-folder=build --build=missing
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Un démarrage sans affichage peut être vérifié sous Linux avec :

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
SDL_RENDER_DRIVER=software timeout 3s ./build/boulderdash
```
