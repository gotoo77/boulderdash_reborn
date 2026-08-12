#!/usr/bin/env python3
"""Prepare the Emscripten output for static hosting on GitHub Pages."""
from __future__ import annotations

import argparse
import shutil
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parents[1]
DEFAULT_BUILD_DIR = PROJECT_DIR / "build-web"
DEFAULT_DIST_DIR = PROJECT_DIR / "dist-web"
WEB_ARTIFACTS = (
    "boulderdash.html",
    "boulderdash.js",
    "boulderdash.wasm",
    "boulderdash.data",
)


def resolve_inside_project(path: Path) -> Path:
    resolved = path.resolve()
    try:
        resolved.relative_to(PROJECT_DIR)
    except ValueError as error:
        raise RuntimeError(f"Chemin hors du projet refusé : {resolved}") from error
    return resolved


def require_artifacts(build_dir: Path) -> None:
    missing = [
        name
        for name in WEB_ARTIFACTS
        if not (build_dir / name).is_file() or (build_dir / name).stat().st_size == 0
    ]
    if missing:
        raise RuntimeError(
            "Artefacts Web manquants ou vides : " + ", ".join(missing)
        )


def package_web(build_dir: Path, dist_dir: Path) -> None:
    build_dir = resolve_inside_project(build_dir)
    dist_dir = resolve_inside_project(dist_dir)
    if build_dir == dist_dir:
        raise RuntimeError("Le dossier source et le dossier de distribution doivent être distincts.")

    require_artifacts(build_dir)

    if dist_dir.exists():
        shutil.rmtree(dist_dir)
    dist_dir.mkdir(parents=True)

    for name in WEB_ARTIFACTS:
        shutil.copy2(build_dir / name, dist_dir / name)

    # GitHub Pages sert index.html à la racine. On conserve aussi le nom Emscripten
    # original afin que le même répertoire puisse être testé avec web_smoke.py.
    shutil.copy2(build_dir / "boulderdash.html", dist_dir / "index.html")

    index = dist_dir / "index.html"
    if not index.is_file() or index.stat().st_size == 0:
        raise RuntimeError("index.html n'a pas été créé correctement.")
    html = index.read_text(encoding="utf-8", errors="ignore")
    if "boulderdash.js" not in html:
        raise RuntimeError("index.html ne référence pas boulderdash.js.")

    expected = sorted((*WEB_ARTIFACTS, "index.html"))
    actual = sorted(path.name for path in dist_dir.iterdir() if path.is_file())
    if actual != expected:
        raise RuntimeError(
            "Contenu inattendu dans le package Web : " + ", ".join(actual)
        )

    print(f"Package Web prêt : {dist_dir}")
    for name in expected:
        size = (dist_dir / name).stat().st_size
        print(f"  {name}: {size} octets")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=DEFAULT_BUILD_DIR)
    parser.add_argument("--dist-dir", type=Path, default=DEFAULT_DIST_DIR)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        package_web(args.build_dir, args.dist_dir)
    except (OSError, RuntimeError) as error:
        print(f"Erreur : {error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
