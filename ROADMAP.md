# Roadmap Boulderdash Reborn

Dernière mise à jour : 12 août 2026.

## État actuel

Le projet est un prototype jouable sur desktop. Le build natif et les tests sont
opérationnels. La cible WebAssembly existe, mais sa reconstruction reste à
valider avec une installation Emscripten et une dépendance `nlohmann_json`
disponible pour cette toolchain.

| Domaine | État | Validation actuelle |
| --- | --- | --- |
| Gameplay principal | ✅ Fonctionnel | Déplacement, creusement, poussée, gravité, ennemis, score et vies |
| Build desktop | ✅ Fiabilisé | Build CMake vierge réussi sur Fedora ARM64 avec les dépendances Conan existantes |
| Tests automatisés | ✅ Actifs | 7 scénarios, exécutés par CTest |
| Données de niveaux | ✅ Contrôlées | Dimensions, bordures, joueur et sortie validés automatiquement |
| Traductions | ✅ Contrôlées | Parité des clés EN/FR/JP validée automatiquement |
| Exécution desktop | ✅ Vérifiée | Démarrage headless avec chargement des assets |
| Packaging Web | 🟡 Corrigé, non validé | `assets/` et `cfg/` sont préchargés ; rebuild Emscripten restant |
| Outils Python/Conan | ✅ Reproductibles | UV, Python 3.12, Conan 2.31.2 et Pillow sont verrouillés |
| Gestion de versions | 🟡 Préparée | Dépôt Git initialisé sur `main`, remote configuré, premier commit en attente |
| Visibilité GitHub | ✅ Privée | `gotoo77/boulderdash_reborn` est privé pendant l’audit des assets |
| CI | ⬜ À faire | Aucun workflow automatique |

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
- [x] Ajouter la licence MIT au code et séparer explicitement le statut des assets.

## P0 — rendre chaque livraison reproductible

Objectif : tout changement doit être compilé et testé automatiquement.

- [x] Initialiser le dépôt Git sur `main`, configurer le remote et exclure les
  répertoires `build*`, `tools/venv` et les fichiers de travail locaux.
- [ ] Créer le premier commit après validation de la licence et de la provenance
  des assets publiés.
- [x] Réinstaller Conan avec le Python actif et régénérer ses fichiers CMake.
- [ ] Ajouter une CI Linux qui exécute Conan, CMake, le build et CTest.
- [ ] Documenter ou fournir un profil Conan Emscripten.
- [ ] Reconstruire la cible Web et vérifier que `/assets` et `/cfg` sont présents
  dans le fichier `.data`.
- [ ] Ajouter un smoke test navigateur pour le chargement du menu et d’un niveau.
- [ ] Activer les avertissements compilateur (`-Wall -Wextra -Wpedantic`) dans
  la CI, puis les traiter sans masquer les diagnostics.

Critère de sortie : un clone vierge produit les builds desktop et Web documentés,
et toutes les vérifications passent en CI.

## P1 — sécuriser le cycle complet du jeu

Objectif : couvrir les règles qui peuvent casser le score ou la progression.

- [ ] Tester `Game` : chargement, timer, vies, respawn, score et changement de
  niveau.
- [ ] Tester les collisions rocher/joueur et rocher/ennemi ainsi que les
  explosions.
- [ ] Ajouter un écran de victoire après le dernier niveau au lieu de fermer
  directement l’application.
- [ ] Tester les fichiers de configuration valides, incomplets et corrompus.
- [x] Désactiver `devMode` dans la configuration destinée aux releases.
- [ ] Ajouter un mode pause et définir le comportement du timer pendant la pause.

Critère de sortie : le parcours des quatre niveaux, le game over et la victoire
sont reproductibles par des tests de logique déterministes.

## P2 — réduire la dette technique

Objectif : rendre les changements d’interface et de gameplay plus simples.

- [ ] Découper `main.cpp` en application, gestion des écrans, gestion des entrées
  et boucle gameplay.
- [ ] Retirer la dépendance audio de `Game` et produire des événements consommés
  par la couche SDL.
- [ ] Remplacer le parseur JSON manuel de `Config` par `nlohmann_json` avec une
  validation explicite et des messages d’erreur précis.
- [ ] Séparer les exemples de menu du binaire de production.
- [ ] Nettoyer les fichiers expérimentaux (`tmp_sim*.cpp`, `dev_backup.py`) après
  récupération éventuelle de leur contenu utile.
- [ ] Ajouter des tests sous AddressSanitizer et UndefinedBehaviorSanitizer.

Critère de sortie : le cœur du jeu se compile et se teste sans SDL ni audio, et
le point d’entrée ne porte plus la logique des différents écrans.

## P3 — préparer une version distribuable

- [x] Ajouter une licence MIT au code.
- [ ] Documenter les licences et crédits de tous les assets avant passage en public.
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
