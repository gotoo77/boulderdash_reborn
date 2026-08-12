#!/usr/bin/env python3
"""Smoke test desktop et tactile de la version WebAssembly."""

from __future__ import annotations

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import threading

from playwright.sync_api import Error, Page, TimeoutError, sync_playwright


PROJECT_DIR = Path(__file__).resolve().parents[1]


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, format: str, *args: object) -> None:
        pass


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=PROJECT_DIR / "build-web")
    parser.add_argument("--browser", choices=("chromium", "firefox"), default="chromium")
    parser.add_argument("--screenshot", type=Path)
    parser.add_argument("--options-screenshot", type=Path)
    parser.add_argument("--pause-screenshot", type=Path)
    return parser.parse_args()


def wait_for(page: Page, expression: str, label: str, timeout: int = 10_000) -> None:
    try:
        page.wait_for_function(expression, timeout=timeout)
    except TimeoutError as error:
        state = page.evaluate("window.Module?.boulderdashState ?? 'unknown'")
        raise RuntimeError(f"Timeout pendant {label}; état courant={state}") from error


def wait_state(page: Page, state: str, label: str, timeout: int = 10_000) -> None:
    wait_for(page, f"window.Module?.boulderdashState === '{state}'", label, timeout)


def main() -> int:
    args = parse_args()
    build_dir = args.build_dir.resolve()
    entrypoint = build_dir / "boulderdash.html"
    if not entrypoint.is_file():
        raise SystemExit(f"Artefact Web absent : {entrypoint}")

    handler = partial(QuietHandler, directory=str(build_dir))
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    url = f"http://127.0.0.1:{server.server_port}/boulderdash.html"
    page_errors: list[str] = []

    try:
        with sync_playwright() as playwright:
            browser_type = getattr(playwright, args.browser)
            launch_args = ["--disable-gpu"] if args.browser == "chromium" else []
            browser = browser_type.launch(headless=True, args=launch_args)

            page = browser.new_page(viewport={"width": 1280, "height": 900})
            page.on("pageerror", lambda error: page_errors.append(str(error)))
            page.goto(url, wait_until="domcontentloaded", timeout=30_000)
            wait_state(page, "menu", "chargement du menu desktop", 30_000)
            wait_for(
                page,
                "window.Module?.boulderdashFontBackend === 'ttf'",
                "chargement de la police TTF",
            )

            canvas = page.locator("#canvas")
            dimensions = canvas.evaluate(
                "element => ({width: element.width, height: element.height})"
            )
            if dimensions["width"] <= 0 or dimensions["height"] <= 0:
                raise RuntimeError(f"Canvas invalide : {dimensions}")

            page.keyboard.press("ArrowDown")
            page.keyboard.press("Enter")
            wait_state(page, "options", "ouverture Options au clavier")
            page.keyboard.press("ArrowRight")
            wait_for(
                page,
                "window.Module?.boulderdashLanguage === 'jp'",
                "changement de langue au clavier",
            )
            page.keyboard.press("ArrowDown")
            page.keyboard.press("Minus")
            wait_for(
                page,
                "window.Module?.boulderdashMusicVolume === 90",
                "réglage du volume musique",
            )
            page.keyboard.press("ArrowDown")
            page.keyboard.press("Minus")
            wait_for(
                page,
                "window.Module?.boulderdashEffectsVolume === 90",
                "réglage du volume effets",
            )
            if args.options_screenshot:
                args.options_screenshot.parent.mkdir(parents=True, exist_ok=True)
                canvas.screenshot(path=args.options_screenshot)
            page.keyboard.press("Escape")
            wait_state(page, "menu", "retour au menu depuis Options")
            page.keyboard.press("Enter")
            wait_state(page, "playing", "démarrage de la partie au clavier")
            page.keyboard.press("Escape")
            wait_state(page, "paused", "pause au clavier")
            if args.pause_screenshot:
                args.pause_screenshot.parent.mkdir(parents=True, exist_ok=True)
                canvas.screenshot(path=args.pause_screenshot)
            page.keyboard.press("ArrowDown")
            page.keyboard.press("Enter")
            wait_state(page, "options", "Options depuis le menu pause")
            page.keyboard.press("Escape")
            wait_state(page, "paused", "retour au menu pause")
            page.keyboard.press("Escape")
            wait_state(page, "playing", "reprise avec Échap")
            page.keyboard.press("p")
            wait_state(page, "paused", "pause avec P")
            page.wait_for_timeout(500)
            if page.evaluate("window.Module?.boulderdashState") != "paused":
                raise RuntimeError("Le jeu a quitté la pause sans commande de reprise")
            page.keyboard.press("p")
            wait_state(page, "playing", "reprise avec P")
            if args.screenshot:
                args.screenshot.parent.mkdir(parents=True, exist_ok=True)
                canvas.screenshot(path=args.screenshot)
            page.close()

            touch_context = browser.new_context(
                viewport={"width": 390, "height": 844},
                has_touch=True,
            )
            touch_page = touch_context.new_page()
            touch_page.on("pageerror", lambda error: page_errors.append(str(error)))
            touch_page.goto(url, wait_until="domcontentloaded", timeout=30_000)
            wait_state(touch_page, "menu", "chargement du menu mobile", 30_000)
            wait_for(
                touch_page,
                "typeof window.Module?._boulderdash_web_input === 'function'",
                "disponibilité du pont tactile C++",
            )

            touch_controls = touch_page.locator("#touch-controls")
            if not touch_controls.is_visible():
                raise RuntimeError("Les contrôles tactiles ne sont pas visibles sur un navigateur tactile")

            touch_canvas_box = touch_page.locator("#canvas").bounding_box()
            if touch_canvas_box is None or touch_canvas_box["width"] > 390:
                raise RuntimeError(f"Canvas mobile hors viewport : {touch_canvas_box}")

            down = touch_page.locator("[data-action='1']")
            up = touch_page.locator("[data-action='0']")
            confirm = touch_page.locator("[data-action='4']")
            back = touch_page.locator("[data-action='5']")
            start = touch_page.locator("[data-action='6']")

            # click() déclenche explicitement pointerdown/pointerup dans Chromium,
            # c'est le contrat réellement consommé par le shell tactile.
            down.click()
            confirm.click()
            wait_state(touch_page, "options", "ouverture Options au tactile")
            back.click()
            wait_state(touch_page, "menu", "retour au menu au tactile")
            up.click()
            confirm.click()
            wait_state(touch_page, "playing", "démarrage de la partie au tactile")
            start.click()
            wait_state(touch_page, "paused", "pause avec START tactile")
            start.click()
            wait_state(touch_page, "playing", "reprise avec START tactile")
            back.click()
            wait_state(touch_page, "paused", "pause avec B tactile")
            touch_context.close()

            if page_errors:
                raise RuntimeError("Erreurs JavaScript : " + " | ".join(page_errors))
            browser.close()
    except (Error, TimeoutError, RuntimeError) as error:
        raise SystemExit(f"Smoke test Web échoué : {error}") from error
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)

    print(
        f"Smoke test Web réussi avec {args.browser} : clavier, tactile mobile, police TTF, japonais, "
        f"volumes, pause/options et reprise ({dimensions['width']}x{dimensions['height']})."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
