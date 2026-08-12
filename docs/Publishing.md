# Publication Web

La version Web est produite par Emscripten dans `build-web/`, puis transformée
en un petit artefact statique `dist-web/` destiné à GitHub Pages.

## Construire le package localement

```bash
uv run manage.py install-web-sdk
uv run manage.py verify-web
uv run python tools/package_web.py
```

Le package contient uniquement :

- `index.html` : point d'entrée GitHub Pages ;
- `boulderdash.html` : point d'entrée Emscripten conservé pour les tests locaux ;
- `boulderdash.js` ;
- `boulderdash.wasm` ;
- `boulderdash.data`.

`boulderdash.data` contient les ressources préchargées (`assets/` et `cfg/`).
`dist-web/` est généré et n'est pas versionné.

## GitHub Pages

Le workflow `.github/workflows/pages.yml` se déclenche sur un push vers `main`
ou manuellement depuis GitHub Actions. Il reconstruit la cible Web, vérifie les
artefacts, crée `dist-web/`, charge l'artefact Pages puis le déploie dans
l'environnement `github-pages`.

Avant le premier déploiement, configurer le dépôt dans GitHub :

1. `Settings` → `Pages` ;
2. dans `Build and deployment`, choisir `Source: GitHub Actions`.

Après déploiement, l'URL effective est fournie par le job `Deploy`. Pour ce dépôt,
l'URL projet attendue sans domaine personnalisé est
`https://gotoo77.github.io/boulderdash_reborn/` ; la valeur annoncée par GitHub
Pages après le premier déploiement reste la référence.

## Validation

La CI de pull request exécute déjà le smoke test Chromium sur le build Web puis
construit aussi `dist-web/`. Après le premier déploiement Pages, une validation
manuelle Firefox + Chromium de l'URL publique complète le gate P3.
