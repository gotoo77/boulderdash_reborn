#!/usr/bin/env python3
"""Console d'administration locale pour Boulderdash Reborn."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Sequence


PROJECT_DIR = Path(__file__).resolve().parent
BUILD_DIR = PROJECT_DIR / "build"
GAME_BINARY = BUILD_DIR / "boulderdash"


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
        "6": ("Afficher l'état Git", git_status),
        "7": ("Afficher la roadmap", show_roadmap),
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
    subparsers.add_parser("status", help="Afficher l'état Git et les remotes")
    subparsers.add_parser("roadmap", help="Afficher ROADMAP.md")
    run_parser = subparsers.add_parser("run", help="Compiler si nécessaire et lancer le jeu")
    run_parser.add_argument("game_args", nargs=argparse.REMAINDER)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    actions = {
        "configure": configure,
        "build": build,
        "test": test,
        "verify": verify,
        "status": git_status,
        "roadmap": show_roadmap,
    }
    try:
        if args.command is None:
            return interactive_menu()
        if args.command == "run":
            run_game(args.game_args)
        else:
            actions[args.command]()
        return 0
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(Style.error(f"Erreur : {error}"), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
