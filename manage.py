#!/usr/bin/env python3
"""Console d'administration locale pour Boulderdash Reborn."""

from __future__ import annotations

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import os
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Sequence
import webbrowser


PROJECT_DIR = Path(__file__).resolve().parent
BUILD_DIR = PROJECT_DIR / "build"
GAME_BINARY = BUILD_DIR / "boulderdash"
WEB_BUILD_DIR = PROJECT_DIR / "build-web"
WEB_ARTIFACTS = (
    "boulderdash.html",
    "boulderdash.js",
    "boulderdash.wasm",
    "boulderdash.data",
)
LOCAL_EMSDK_DIR = PROJECT_DIR / ".tools" / "emsdk"


class Style:
    enabled = sys.stdout.isatty() and os.environ.get("NO_COLOR") is None

    @classmethod
    def apply(cls, code: str, text: str) -> str:
        return f"\033[{code}m{text}\033[0m" if cls.enabled else text

    @classmethod
    def title(cls, text: str) -> str:
        return cls.apply("1;36", text)

    @classmethod
    def success(cls, text: str) -> str:
        return cls.apply("1;32", text)

    @classmethod
    def warning(cls, text: str) -> str:
        return cls.apply("1;33", text)

    @classmethod
    def error(cls, text: str) -> str:
        return cls.apply("1;31", text)


def command_text(command: Sequence[object]) -> str:
    return " ".join(str(part) for part in command)


def run_command(
    command: Sequence[object],
    *,
    env: dict[str, str] | None = None,
    check: bool = True,
) -> subprocess.CompletedProcess[str]:
    args = [str(part) for part in command]
    print(Style.title(f"\n$ {command_text(args)}"), flush=True)
    return subprocess.run(args, cwd=PROJECT_DIR, env=env, check=check, text=True)


def compiler_environment() -> dict[str, str]:
    return os.environ.copy()


def require_working_conan() -> None:
    executable = shutil.which("conan")
    if not executable:
        raise RuntimeError(
            "Conan est absent. Exécutez 'uv sync' puis relancez avec "
            "'uv run manage.py'."
        )
    try:
        subprocess.run(
            [executable, "--version"],
            cwd=PROJECT_DIR,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except (OSError, subprocess.CalledProcessError) as error:
        raise RuntimeError(
            "Conan est inutilisable dans cet environnement. Exécutez 'uv sync' "
            "puis relancez avec 'uv run manage.py'."
        ) from error


def emscripten_environment() -> dict[str, str]:
    env = os.environ.copy()
    if shutil.which("emcmake", path=env.get("PATH")):
        return env
    environment_script = LOCAL_EMSDK_DIR / "emsdk_env.sh"
    if not environment_script.is_file():
        return env
    result = subprocess.run(
        [
            "bash",
            "-c",
            'source "$1" >/dev/null && env -0',
            "bash",
            str(environment_script),
        ],
        cwd=PROJECT_DIR,
        check=True,
        capture_output=True,
    )
    for entry in result.stdout.split(b"\0"):
        if not entry or b"=" not in entry:
            continue
        key, value = entry.split(b"=", 1)
        env[key.decode()] = value.decode()
    return env


def require_emscripten() -> dict[str, str]:
    env = emscripten_environment()
    missing = [
        command
        for command in ("emcmake", "emcc", "em++")
        if not shutil.which(command, path=env.get("PATH"))
    ]
    if missing:
        raise RuntimeError(
            "Emscripten est absent (commandes manquantes : "
            + ", ".join(missing)
            + "). Lancez 'uv run manage.py install-web-sdk' ou utilisez "
            "l'option d'installation du menu."
        )
    return env


def install_web_sdk() -> None:
    emsdk_executable = LOCAL_EMSDK_DIR / "emsdk"
    if not emsdk_executable.exists():
        LOCAL_EMSDK_DIR.parent.mkdir(parents=True, exist_ok=True)
        run_command(
            [
                "git",
                "clone",
                "--depth",
                "1",
                "https://github.com/emscripten-core/emsdk.git",
                LOCAL_EMSDK_DIR,
            ]
        )
    elif not (LOCAL_EMSDK_DIR / ".git").is_dir():
        raise RuntimeError(f"Le dossier {LOCAL_EMSDK_DIR} existe mais n'est pas un dépôt emsdk.")

    run_command([emsdk_executable, "install", "latest"])
    run_command([emsdk_executable, "activate", "latest"])
    env = require_emscripten()
    run_command(["emcc", "--version"], env=env)
    print(Style.success("Emscripten SDK est installé et activé localement."))


def configure() -> None:
    require_working_conan()
    env = compiler_environment()
    BUILD_DIR.mkdir(exist_ok=True)
    run_command(
        [
            "conan",
            "install",
            ".",
            f"--output-folder={BUILD_DIR}",
            "--build=missing",
            "-s",
            "build_type=Release",
            "-s",
            "compiler.cppstd=17",
        ],
        env=env,
    )
    run_command(
        [
            "cmake",
            "-S",
            ".",
            "-B",
            BUILD_DIR,
            f"-DCMAKE_TOOLCHAIN_FILE={BUILD_DIR / 'conan_toolchain.cmake'}",
            "-DCMAKE_BUILD_TYPE=Release",
        ],
        env=env,
    )
    print(Style.success("Configuration terminée."))


def build() -> None:
    if not (BUILD_DIR / "CMakeCache.txt").exists():
        configure()
    run_command(["cmake", "--build", BUILD_DIR, "--parallel", os.cpu_count() or 2])
    print(Style.success("Compilation terminée."))


def configure_web() -> None:
    require_working_conan()
    env = require_emscripten()
    WEB_BUILD_DIR.mkdir(exist_ok=True)
    run_command(
        [
            "conan",
            "install",
            ".",
            f"--output-folder={WEB_BUILD_DIR}",
            "--build=missing",
            "-s",
            "build_type=Release",
            "-s",
            "compiler.cppstd=17",
        ],
        env=env,
    )
    run_command(
        [
            "emcmake",
            "cmake",
            "-S",
            ".",
            "-B",
            WEB_BUILD_DIR,
            "--fresh",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DBUILD_TESTING=OFF",
            f"-Dnlohmann_json_DIR={WEB_BUILD_DIR}",
        ],
        env=env,
    )
    print(Style.success("Configuration WebAssembly terminée."))


def web_configuration_is_current(env: dict[str, str]) -> bool:
    cache = WEB_BUILD_DIR / "CMakeCache.txt"
    conan_config = WEB_BUILD_DIR / "nlohmann_json-config.cmake"
    emsdk_dir = env.get("EMSDK")
    if not cache.is_file() or not conan_config.is_file() or not emsdk_dir:
        return False

    expected_toolchain = (
        Path(emsdk_dir) / "upstream" / "emscripten" / "cmake" / "Modules"
        / "Platform" / "Emscripten.cmake"
    )
    cache_text = cache.read_text(encoding="utf-8", errors="ignore")
    toolchain_is_current = (
        f"CMAKE_TOOLCHAIN_FILE:FILEPATH={expected_toolchain}" in cache_text
    )
    dependency_is_configured = any(
        f"nlohmann_json_DIR:{cache_type}={WEB_BUILD_DIR}" in cache_text
        for cache_type in ("PATH", "UNINITIALIZED")
    )
    return toolchain_is_current and dependency_is_configured


def build_web() -> None:
    env = require_emscripten()
    if not web_configuration_is_current(env):
        print(Style.warning("Configuration Web absente ou obsolète : reconfiguration."))
        configure_web()
    run_command(
        [
            "cmake",
            "--build",
            WEB_BUILD_DIR,
            "--target",
            "boulderdash",
            "--parallel",
            os.cpu_count() or 2,
        ],
        env=env,
    )
    print(Style.success("Compilation WebAssembly terminée."))


def verify_web() -> None:
    build_web()
    missing = [
        name
        for name in WEB_ARTIFACTS
        if not (WEB_BUILD_DIR / name).is_file() or (WEB_BUILD_DIR / name).stat().st_size == 0
    ]
    if missing:
        raise RuntimeError("Artefacts Web manquants ou vides : " + ", ".join(missing))

    loader = (WEB_BUILD_DIR / "boulderdash.js").read_text(encoding="utf-8", errors="ignore")
    missing_packages = [
        path for path in ("assets/tiles.png", "cfg/config.json") if path not in loader
    ]
    if missing_packages:
        raise RuntimeError(
            "Fichiers absents du package Emscripten : " + ", ".join(missing_packages)
        )
    if "Asyncify" not in loader:
        raise RuntimeError(
            "Le runtime Web ne contient pas Asyncify : la boucle de jeu ne pourra "
            "pas rendre la main au navigateur."
        )
    print(Style.success("Artefacts Web et données préchargées vérifiés."))


def serve_web(port: int = 8000, *, open_browser: bool = True) -> None:
    verify_web()
    handler = partial(SimpleHTTPRequestHandler, directory=str(WEB_BUILD_DIR))
    server = ThreadingHTTPServer(("127.0.0.1", port), handler)
    url = f"http://127.0.0.1:{port}/boulderdash.html"
    print(Style.success(f"Serveur Web disponible sur {url}"))
    print("Appuyez sur Ctrl+C pour l'arrêter.")
    if open_browser:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nArrêt du serveur Web.")
    finally:
        server.server_close()


def test() -> None:
    build()
    run_command(["ctest", "--test-dir", BUILD_DIR, "--output-on-failure"])
    print(Style.success("Tous les tests sont passés."))


def run_game(arguments: Sequence[str] = ()) -> None:
    build()
    run_command([GAME_BINARY, *arguments])


def smoke_test(duration: float = 3.0, *, ensure_build: bool = True) -> None:
    if ensure_build:
        build()
    env = os.environ.copy()
    env.update(
        {
            "SDL_VIDEODRIVER": "dummy",
            "SDL_AUDIODRIVER": "dummy",
            "SDL_RENDER_DRIVER": "software",
        }
    )
    print(Style.title(f"\n$ smoke test SDL ({duration:.1f} s)"))
    process = subprocess.Popen(
        [str(GAME_BINARY)],
        cwd=PROJECT_DIR,
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
    )
    try:
        return_code = process.wait(timeout=duration)
    except subprocess.TimeoutExpired:
        process.terminate()
        process.wait(timeout=2)
        print(Style.success("Le jeu est resté actif et a passé le smoke test."))
        return
    raise RuntimeError(f"Le jeu s'est arrêté prématurément avec le code {return_code}.")


def verify() -> None:
    test()
    smoke_test(ensure_build=False)
    print(Style.success("\nVérification complète réussie."))


def git_status() -> None:
    run_command(["git", "status", "--short", "--branch"])
    run_command(["git", "remote", "-v"])


def show_roadmap() -> None:
    roadmap = PROJECT_DIR / "ROADMAP.md"
    print(Style.title(f"\n{roadmap}"))
    print(roadmap.read_text(encoding="utf-8"))


def interactive_menu() -> int:
    actions = {
        "1": ("Lancer le jeu", run_game),
        "2": ("Compiler", build),
        "3": ("Lancer les tests", test),
        "4": ("Vérification complète", verify),
        "5": ("Configurer avec Conan", configure),
        "6": ("Installer Emscripten SDK", install_web_sdk),
        "7": ("Configurer le build Web", configure_web),
        "8": ("Compiler le build Web", build_web),
        "9": ("Vérifier le build Web", verify_web),
        "10": ("Compiler et servir le build Web", serve_web),
        "11": ("Afficher l'état Git", git_status),
        "12": ("Afficher la roadmap", show_roadmap),
    }
    while True:
        print(Style.title("\n╔══════════════════════════════════╗"))
        print(Style.title("║   Boulderdash Reborn — Admin     ║"))
        print(Style.title("╚══════════════════════════════════╝"))
        for key, (label, _) in actions.items():
            print(f"  {key}. {label}")
        print("  0. Quitter")
        choice = input("\nVotre choix : ").strip()
        if choice == "0":
            return 0
        action = actions.get(choice)
        if not action:
            print(Style.warning("Choix inconnu."))
            continue
        try:
            action[1]()
        except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
            print(Style.error(f"\nErreur : {error}"), file=sys.stderr)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command")
    subparsers.add_parser("configure", help="Installer les dépendances et configurer CMake")
    subparsers.add_parser("build", help="Compiler le projet")
    subparsers.add_parser("test", help="Compiler et exécuter les tests")
    subparsers.add_parser("verify", help="Compiler, tester et effectuer un smoke test SDL")
    subparsers.add_parser("install-web-sdk", help="Installer Emscripten SDK localement")
    subparsers.add_parser("configure-web", help="Configurer CMake avec Emscripten")
    subparsers.add_parser("build-web", help="Compiler la version WebAssembly")
    subparsers.add_parser("verify-web", help="Vérifier les artefacts et données Web")
    subparsers.add_parser("status", help="Afficher l'état Git et les remotes")
    subparsers.add_parser("roadmap", help="Afficher ROADMAP.md")
    run_parser = subparsers.add_parser("run", help="Compiler si nécessaire et lancer le jeu")
    run_parser.add_argument("game_args", nargs=argparse.REMAINDER)
    serve_parser = subparsers.add_parser("serve-web", help="Compiler et servir la version Web")
    serve_parser.add_argument("--port", type=int, default=8000)
    serve_parser.add_argument("--no-browser", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    actions = {
        "configure": configure,
        "build": build,
        "test": test,
        "verify": verify,
        "install-web-sdk": install_web_sdk,
        "configure-web": configure_web,
        "build-web": build_web,
        "verify-web": verify_web,
        "status": git_status,
        "roadmap": show_roadmap,
    }
    try:
        if args.command is None:
            return interactive_menu()
        if args.command == "run":
            run_game(args.game_args)
        elif args.command == "serve-web":
            serve_web(args.port, open_browser=not args.no_browser)
        else:
            actions[args.command]()
        return 0
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(Style.error(f"Erreur : {error}"), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
