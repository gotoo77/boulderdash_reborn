# Provenance des effets sonores

Ce fichier suit la provenance de chaque son référencé par `cfg/audio.json`.

| Fichier | Événement | Provenance | Statut publication |
| --- | --- | --- | --- |
| `walk.wav` | marche | Synthèse procédurale originale par `tools/generate_retro_sfx.py` | 🟢 MIT |
| `dig.wav` | creusement | Synthèse procédurale originale par `tools/generate_retro_sfx.py` | 🟢 MIT |
| `rock_fall.wav` | chute de rocher | Synthèse procédurale originale par `tools/generate_retro_sfx.py` | 🟢 MIT |
| `diamond.wav` | collecte de diamant | Synthèse procédurale originale par `tools/generate_retro_sfx.py` | 🟢 MIT |
| `death.wav` | mort du joueur | Synthèse procédurale originale par `tools/generate_retro_sfx.py` | 🟢 MIT |
| `diamond_fall.wav` | chute de diamant | Synthèse procédurale originale par `tools/generate_retro_sfx.py` | 🟢 MIT |
| `exit_unlock.wav` | ouverture de la sortie | Effet procédural original généré pour ce projet | 🟢 MIT |
| `explosion.wav` | explosion | Effet procédural original généré pour ce projet | 🟢 MIT |
| `time_warning.wav` | alerte de fin de timer | Effet procédural original généré pour ce projet | 🟢 MIT |
| `game-over-arcade-6435.mp3` | Game Over | `Game Over Arcade`, myfox14 (Freesound), via Pixabay | 🟡 Pixabay Content License |

## SFX rétro procéduraux

Les anciennes captures CPC de `walk.wav`, `dig.wav`, `rock_fall.wav`,
`diamond.wav` et `diamond_fall.wav` ont été supprimées du HEAD du dépôt.
L'ancien `death.wav` provenant d'une source tierce non retrouvée a également été
supprimé et remplacé par une synthèse originale.

Ces six noms de fichiers désignent désormais des **artefacts de build** générés
par `tools/generate_retro_sfx.py`.

Le générateur :

- ne lit aucune ROM, aucun enregistrement et aucun sample audio externe ;
- utilise uniquement des oscillateurs simples, du bruit pseudo-aléatoire à
  graines fixes, des enveloppes et des filtres ;
- produit du PCM mono 16 bits à 22 050 Hz ;
- est déterministe : les mêmes sources Python produisent les mêmes octets WAV ;
- est exécuté automatiquement par CMake avant le binaire et les tests ;
- distribue le générateur et les sons produits avec le projet sous licence MIT.

Les WAV générés sont volontairement ignorés par Git : la source de vérité est le
code du synthétiseur. Ils peuvent aussi être régénérés manuellement avec :

```bash
python tools/generate_retro_sfx.py
```

Le mixage relatif reste dans `cfg/audio.json`. Après validation à l'oreille, le
son `dig` est abaissé à 80/128, `walk` à 72/128 et `death` à 112/128. `walk`
utilise une recette proche de `dig`, mais plus courte et filtrée plus bas pour un
rendu plus feutré.

Empreintes SHA-256 attendues pour la version actuelle du générateur :

| Fichier | SHA-256 |
| --- | --- |
| `walk.wav` | `342d3db74cdc41a942cb830c274fb69782b55871e9a6b5dde01237adb214ae7a` |
| `dig.wav` | `e556ed21c9fcd4133ed1303e737339ea14f2de90e27c87846ee978b05bef356f` |
| `rock_fall.wav` | `108fddf2c49b175e703449dc4ec61aef9204e26eb859fad5a0af914bc4ae0370` |
| `diamond.wav` | `f57bad217d175c7b3e7905659e80549b5e893c853cfb68a3b6d8472f9afe9bb9` |
| `death.wav` | `10c3c0456541f441c86c810c2506192a54886302eebf9b5fb6b29b582bf0d24a` |
| `diamond_fall.wav` | `d4a3425c63c54dea1036cc6bd69af7e95cbd000dd7319d5ab5bbbf7212db868c` |

## Game Over Arcade

- Source : https://pixabay.com/sound-effects/film-special-effects-game-over-arcade-6435/
- Auteur indiqué par Pixabay : myfox14 (Freesound)
- Licence : Pixabay Content License
- Résumé : https://pixabay.com/service/license-summary/
- Attribution non requise par le résumé de licence Pixabay, mais la provenance
  est conservée ici pour la traçabilité.
- Ne pas redistribuer le fichier comme produit sonore autonome ; il est utilisé
  comme composant du jeu.

Le moteur charge tous ces fichiers via `cfg/audio.json`; leur remplacement ne
nécessite pas de changement de logique dans le code.
