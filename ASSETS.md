# Assets et droits de redistribution

La licence MIT du fichier `LICENSE` couvre le code source et la documentation
créés pour ce projet. Elle ne place pas automatiquement les polices, images,
sons ou musiques de `assets/` sous licence MIT.

Ce document suit la **provenance** et les **conditions de redistribution** des
assets. L'objectif du jalon P3 est simple : ne publier aucune ressource dont la
source ou les conditions d'utilisation restent inconnues.

## Convention de statut

- 🟢 **prêt** : provenance et conditions documentées pour la distribution du jeu ;
- 🟡 **documenté avec contrainte** : source connue, mais une condition de
  redistribution doit être respectée ;
- 🟠 **à identifier** : provenance ou licence encore incomplète ;
- 🔴 **à remplacer** : ressource connue comme issue directement d'une œuvre
  tierce sans autorisation de redistribution documentée.

## État actuel

| Famille | Emplacement | Statut | Provenance / action |
| --- | --- | --- | --- |
| Tilesheet et visuels de tiles | `assets/tiles.png`, `assets/sprites/` | 🟢 | Déclaration du mainteneur : visuels générés spécifiquement pour ce projet avec ChatGPT ; ils ne proviennent pas d'une extraction de ROM. |
| Logo GotooCorp | `assets/ui/gotoocorp_logo.png` | 🟠 | Provenance à confirmer avant publication. |
| SFX rétro générés | `walk.wav`, `dig.wav`, `rock_fall.wav`, `diamond.wav`, `death.wav`, `diamond_fall.wav` | 🟢 | Synthèse procédurale originale et déterministe par `tools/generate_retro_sfx.py`. Les anciens fichiers non publiables/insuffisamment sourcés ont été supprimés du HEAD ; les WAV sont générés par CMake et ne sont pas suivis par Git. Voir `assets/sfx/SOURCES.md`. |
| Autres SFX procéduraux | `assets/sfx/exit_unlock.wav`, `assets/sfx/explosion.wav`, `assets/sfx/time_warning.wav` | 🟢 | Créations procédurales originales du projet ; distribuées avec le projet sous MIT. |
| SFX Game Over | `assets/sfx/game-over-arcade-6435.mp3` | 🟡 | `Game Over Arcade`, myfox14 (Freesound), distribué via Pixabay sous Pixabay Content License. Voir `assets/sfx/SOURCES.md`. |
| Musique du menu | `assets/theme/bd_theme_menu.ogg` | 🟠 | Provenance et licence à identifier. |
| Police UI | `assets/fonts/ui_font.ttf` | 🟢 | Noto Sans JP, SIL Open Font License 1.1. Une copie de la licence est fournie dans `assets/fonts/LICENSE.OFL.txt`. |

## Détail audio

La provenance fichier par fichier des effets sonores est suivie dans
`assets/sfx/SOURCES.md`. Les associations runtime et volumes relatifs sont
configurés par `cfg/audio.json`.

Les six SFX rétro `walk`, `dig`, `rock_fall`, `diamond`, `death` et
`diamond_fall` sont des artefacts reproductibles : CMake exécute
`tools/generate_retro_sfx.py` avant la compilation du jeu et des tests. Le
générateur ne lit aucune ROM ni source audio tierce et produit du PCM mono
16 bits à 22 050 Hz.

## Historique des fichiers remplacés

Les versions précédentes de `walk.wav`, `dig.wav`, `rock_fall.wav`,
`diamond.wav` et `diamond_fall.wav` avaient été enregistrées depuis la version
CPC de Boulder Dash via Caprice. Elles ont été identifiées pendant P3.1 puis
**supprimées du HEAD du dépôt** et remplacées par la synthèse procédurale
originale décrite ci-dessus.

L'ancien `death.wav`, déclaré issu d'une source libre mais dont l'origine exacte
n'avait pas pu être retrouvée, a lui aussi été supprimé et remplacé par un effet
procédural original.

L'objectif n'est pas d'interdire les **inspirations** rétro : un son recréé ou
synthétisé spécifiquement pour le projet peut conserver une esthétique CPC sans
réutiliser l'enregistrement d'origine.

## Sources documentées

### Police Noto Sans JP

- Projet : Noto Sans CJK / Noto Sans Japanese
- Licence : SIL Open Font License 1.1
- Source licence : https://github.com/notofonts/noto-cjk/blob/main/Sans/LICENSE

### `game-over-arcade-6435.mp3`

- Titre : `Game Over Arcade`
- Auteur indiqué par Pixabay : myfox14 (Freesound)
- Source : https://pixabay.com/sound-effects/film-special-effects-game-over-arcade-6435/
- Licence : Pixabay Content License
- Résumé de licence : https://pixabay.com/service/license-summary/
- Note : l'asset doit rester intégré au jeu et ne pas être redistribué comme
  produit sonore autonome.

## Gate avant GitHub Pages

Avant d'activer une publication publique :

- [x] documenter la provenance des tiles générées pour le projet ;
- [x] joindre la licence OFL de la police ;
- [x] identifier la provenance et la licence du son Game Over ;
- [x] classer individuellement les six SFX initialement indéterminés ;
- [x] remplacer les cinq captures CPC par des SFX procéduraux originaux et reproductibles ;
- [x] remplacer le `death.wav` tiers insuffisamment sourcé par un SFX procédural original ;
- [ ] documenter ou remplacer la musique du menu ;
- [ ] confirmer la provenance du logo GotooCorp ;
- [ ] vérifier qu'aucun asset 🟠 ou 🔴 n'est inclus dans l'artefact public final.
