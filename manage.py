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
EMSDK_VERSION = "6.0.6"
EMSCRIPTEN_PROFILE = PROJECT_DIR / "profiles" / "emscripten"


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


def strict_warnings_argument() -> str:
    enabled = os.environ.get("BOULDERDASH_STRICT_WARNINGS", "0").lower()
    return "-DBOULDERDASH_STRICT_WARNINGS=" + (
        "ON" if enabled in {"1", "on", "true", "yes"} else "OFF"
    )


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
    version = subprocess.run(
        ["emcc", "--version"],
        cwd=PROJECT_DIR,
        env=env,
        check=True,
        capture_output=True,
        text=True,
    ).stdout.splitlines()[0]
    if EMSDK_VERSION not in version:
        raise RuntimeError(
            f"Emscripten {EMSDK_VERSION} est requis, mais '{version}' est actif. "
            "Lancez 'uv run manage.py install-web-sdk'."
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

    run_command([emsdk_executable, "install", EMSDK_VERSION])
    run_command([emsdk_executable, "activate", EMSDK_VERSION])
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
            strict_warnings_argument(),
        ],
        env=env,
    )
    print(Style.success("Configuration terminée."))


def clean_build_directory(directory: Path, label: str) -> None:
    resolved = directory.resolve()
    allowed = {BUILD_DIR.resolve(), WEB_BUILD_DIR.resolve()}
    if resolved not in allowed or resolved.parent != PROJECT_DIR:
        raise RuntimeError(f"Refus de supprimer un dossier non autorisé : {resolved}")
    if not resolved.exists():
        print(Style.success(f"Le build {label} est déjà propre."))
        return
    shutil.rmtree(resolved)
    print(Style.success(f"Build {label} supprimé : {resolved}"))


def clean() -> None:
    clean_build_directory(BUILD_DIR, "desktop")


def build() -> None:
    if not (BUILD_DIR / "CMakeCache.txt").exists():
        configure()
    run_command(["cmake", "--build", BUILD_DIR, "--parallel", os.cpu_count() or 2])
    print(Style.success("Compilation terminée."))


def rebuild() -> None:
    clean()
    build()


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
            f"--profile:host={EMSCRIPTEN_PROFILE}",
            "--profile:build=default",
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
            strict_warnings_argument(),
        ],
        env=env,
    )
    print(Style.success("Configuration WebAssembly terminée."))


def clean_web() -> None:
    clean_build_directory(WEB_BUILD_DIR, "WebAssembly")


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


def rebuild_web() -> None:
    clean_web()
    build_web()


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


def smoke_web(browser: str = "chromium", screenshot: Path | None = None) -> None:
    verify_web()
    command: list[object] = [
        sys.executable,
        PROJECT_DIR / "tests" / "web_smoke.py",
        "--build-dir",
        WEB_BUILD_DIR,
        "--browser",
        browser,
    ]
    if screenshot is not None:
        command.extend(["--screenshot", screenshot])
    run_command(command)


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


def select_action_with_fzf(actions: dict[str, tuple[str, object]]) -> str | None:
    entries = [f"{key}\t{label}" for key, (label, _) in actions.items()]
    entries.append("0\tQuitter")
    result = subprocess.run(
        [
            "fzf",
            "--height=~22",
            "--layout=reverse",
            "--border=rounded",
            "--info=inline",
            "--prompt=Action > ",
            "--header=↑/↓ naviguer · saisir pour filtrer · Entrée valider · Échap quitter",
            "--delimiter=\t",
            "--with-nth=2..",
            "--no-multi",
        ],
        cwd=PROJECT_DIR,
        input="\n".join(entries) + "\n",
        stdout=subprocess.PIPE,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        return None
    selected = result.stdout.strip()
    return selected.split("\t", 1)[0] if selected else None


def select_action_with_numbers(actions: dict[str, tuple[str, object]]) -> str | None:
    print(Style.title("\n╔══════════════════════════════════╗"))
    print(Style.title("║   Boulderdash Reborn — Admin     ║"))
    print(Style.title("╚══════════════════════════════════╝"))
    for key, (label, _) in actions.items():
        print(f"  {key}. {label}")
    print("  0. Quitter")
    try:
        return input("\nVotre choix : ").strip()
    except (EOFError, KeyboardInterrupt):
        return None


def interactive_menu() -> int:
    actions = {
        "1": ("Lancer le jeu", run_game),
        "2": ("Compiler", build),
        "3": ("Nettoyer le build desktop", clean),
        "4": ("Reconstruire le build desktop", rebuild),
        "5": ("Lancer les tests", test),
        "6": ("Vérification complète", verify),
        "7": ("Configurer avec Conan", configure),
        "8": ("Installer Emscripten SDK", install_web_sdk),
        "9": ("Configurer le build Web", configure_web),
        "10": ("Compiler le build Web", build_web),
        "11": ("Nettoyer le build Web", clean_web),
        "12": ("Reconstruire le build Web", rebuild_web),
        "13": ("Vérifier le build Web", verify_web),
        "14": ("Smoke test Web avec Chromium", smoke_web),
        "15": ("Compiler et servir le build Web", serve_web),
        "16": ("Afficher l'état Git", git_status),
        "17": ("Afficher la roadmap", show_roadmap),
    }
    use_fzf = bool(shutil.which("fzf") and sys.stdin.isatty() and sys.stderr.isatty())
    while True:
        if use_fzf:
            choice = select_action_with_fzf(actions)
        else:
            choice = select_action_with_numbers(actions)
        if choice is None:
            print("\nArrêt de la console d’administration.")
            return 0
        if choice == "0":
            return 0
        action = actions.get(choice)
        if not action:
            print(Style.warning("Choix inconnu."))
            continue
        try:
            action[1]()
        except KeyboardInterrupt:
            print(Style.warning("\nAction interrompue."))
        except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
            print(Style.error(f"\nErreur : {error}"), file=sys.stderr)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command")
    subparsers.add_parser("configure", help="Installer les dépendances et configurer CMake")
    subparsers.add_parser("clean", help="Supprimer le build desktop")
    subparsers.add_parser("clear", help="Alias de clean")
    subparsers.add_parser("build", help="Compiler le projet")
    subparsers.add_parser("rebuild", help="Reconstruire entièrement le projet desktop")
    subparsers.add_parser("test", help="Compiler et exécuter les tests")
    subparsers.add_parser("verify", help="Compiler, tester et effectuer un smoke test SDL")
    subparsers.add_parser("install-web-sdk", help="Installer Emscripten SDK localement")
    subparsers.add_parser("configure-web", help="Configurer CMake avec Emscripten")
    subparsers.add_parser("clean-web", help="Supprimer le build WebAssembly")
    subparsers.add_parser("clear-web", help="Alias de clean-web")
    subparsers.add_parser("build-web", help="Compiler la version WebAssembly")
    subparsers.add_parser("rebuild-web", help="Reconstruire entièrement la version WebAssembly")
    subparsers.add_parser("verify-web", help="Vérifier les artefacts et données Web")
    smoke_parser = subparsers.add_parser(
        "smoke-web", help="Tester le menu et le premier niveau dans un navigateur"
    )
    smoke_parser.add_argument("--browser", choices=("chromium", "firefox"), default="chromium")
    smoke_parser.add_argument("--screenshot", type=Path)
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
        "clean": clean,
        "clear": clean,
        "build": build,
        "rebuild": rebuild,
        "test": test,
        "verify": verify,
        "install-web-sdk": install_web_sdk,
        "configure-web": configure_web,
        "clean-web": clean_web,
        "clear-web": clean_web,
        "build-web": build_web,
        "rebuild-web": rebuild_web,
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
        elif args.command == "smoke-web":
            smoke_web(args.browser, args.screenshot)
        else:
            actions[args.command]()
        return 0
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(Style.error(f"Erreur : {error}"), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
