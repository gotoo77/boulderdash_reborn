#!/usr/bin/env python3
"""Smoke test desktop et tactile de la version WebAssembly."""

from __future__ import annotations

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import threading

from playwright.sync_api import Error, TimeoutError, sync_playwright


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
            page.wait_for_function(
                "window.Module?.boulderdashState === 'menu'", timeout=30_000
            )
            page.wait_for_function(
                "window.Module?.boulderdashFontBackend === 'ttf'", timeout=10_000
            )

            canvas = page.locator("#canvas")
            dimensions = canvas.evaluate(
                "element => ({width: element.width, height: element.height})"
            )
            if dimensions["width"] <= 0 or dimensions["height"] <= 0:
                raise RuntimeError(f"Canvas invalide : {dimensions}")

            page.keyboard.press("ArrowDown")
            page.keyboard.press("Enter")
            page.wait_for_function(
                "window.Module?.boulderdashState === 'options'", timeout=10_000
            )
            page.keyboard.press("ArrowRight")
            page.wait_for_function(
                "window.Module?.boulderdashLanguage === 'jp'", timeout=10_000
            )
            page.keyboard.press("ArrowDown")
            page.keyboard.press("Minus")
            page.wait_for_function(
                "window.Module?.boulderdashMusicVolume === 90", timeout=10_000
            )
            page.keyboard.press("ArrowDown")
            page.keyboard.press("Minus")
            page.wait_for_function(
                "window.Module?.boulderdashEffectsVolume === 90", timeout=10_000
            )
            if args.options_screenshot:
                args.options_screenshot.parent.mkdir(parents=True, exist_ok=True)
                canvas.screenshot(path=args.options_screenshot)
            page.keyboard.press("Escape")
            page.wait_for_function(
                "window.Module?.boulderdashState === 'menu'", timeout=10_000
            )
            page.keyboard.press("Enter")
            page.wait_for_function(
                "window.Module?.boulderdashState === 'playing'", timeout=10_000
            )
            page.keyboard.press("Escape")
            page.wait_for_function(
                "window.Module?.boulderdashState === 'paused'", timeout=10_000
            )
            if args.pause_screenshot:
                args.pause_screenshot.parent.mkdir(parents=True, exist_ok=True)
                canvas.screenshot(path=args.pause_screenshot)
            page.keyboard.press("ArrowDown")
            page.keyboard.press("Enter")
            page.wait_for_function(
                "window.Module?.boulderdashState === 'options'", timeout=10_000
            )
            page.keyboard.press("Escape")
            page.wait_for_function(
                "window.Module?.boulderdashState === 'paused'", timeout=10_000
            )
            page.keyboard.press("Escape")
            page.wait_for_function(
                "window.Module?.boulderdashState === 'playing'", timeout=10_000
            )
            page.keyboard.press("p")
            page.wait_for_function(
                "window.Module?.boulderdashState === 'paused'", timeout=10_000
            )
            page.wait_for_timeout(500)
            if page.evaluate("window.Module?.boulderdashState") != "paused":
                raise RuntimeError("Le jeu a quitté la pause sans commande de reprise")
            page.keyboard.press("p")
            page.wait_for_function(
                "window.Module?.boulderdashState === 'playing'", timeout=10_000
            )
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
            touch_page.wait_for_function(
                "window.Module?.boulderdashState === 'menu'", timeout=30_000
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

            down.tap()
            confirm.tap()
            touch_page.wait_for_function(
                "window.Module?.boulderdashState === 'options'", timeout=10_000
            )
            back.tap()
            touch_page.wait_for_function(
                "window.Module?.boulderdashState === 'menu'", timeout=10_000
            )
            up.tap()
            confirm.tap()
            touch_page.wait_for_function(
                "window.Module?.boulderdashState === 'playing'", timeout=10_000
            )
            start.tap()
            touch_page.wait_for_function(
                "window.Module?.boulderdashState === 'paused'", timeout=10_000
            )
            start.tap()
            touch_page.wait_for_function(
                "window.Module?.boulderdashState === 'playing'", timeout=10_000
            )
            back.tap()
            touch_page.wait_for_function(
                "window.Module?.boulderdashState === 'paused'", timeout=10_000
            )
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
