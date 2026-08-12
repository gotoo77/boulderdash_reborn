# Menu System Overview

The menu framework is fully data-driven: definitions are authored in JSON files, parsed at runtime, and rendered through an abstract interface that is decoupled from SDL (or any other graphics backend). The runtime is split into three layers:

1. Pure menu logic (`src/menu/`): menu definitions, parsing, focus handling, and actions – zero SDL dependencies.
2. Abstractions (`IMenuRenderer`, `IInputProvider`) that define what the core logic needs for drawing and input.
3. Concrete backends (`src/menu/sdl/`) that adapt SDL2 rendering and events to the abstractions, plus mocks for automated testing.

## JSON Definition

```
{
  "id": "main_menu",
  "layout": {
    "anchor": "center",
    "spacing": 24
  },
  "logo": {
    "texture": "assets/ui/gotoocorp_logo.png",
    "position": { "x": 0.5, "y": 0.2 },
    "size": { "width": 400, "height": 120 }
  },
  "items": [
    { "label": "NOUVELLE PARTIE", "action": "start_game" },
    { "label": "OPTIONS", "action": "open_options" },
    { "label": "QUITTER", "action": "quit_game" }
  ]
}
```

* `layout.anchor`: `"center"`, `"top_left"`, `"bottom_center"`, etc.
* `layout.spacing`: distance in pixels between each entry.
* `logo`: optional centered image in normalized coordinates (`0.0` → left/top, `1.0` → right/bottom).
* `items`: each entry has a label, an action identifier, and an optional `enabled` flag.

## Core Usage

```cpp
#include "menu/MenuLoader.h"
#include "menu/MenuManager.h"
#include "menu/MenuInterfaces.h"

menu::Menu menu = menu::MenuLoader::loadFromFile("cfg/main_menu.json");
menu::MenuManager manager;
manager.setMenu(std::move(menu));

// During your frame/update loop:
manager.update(inputProvider);  // inputProvider implements IInputProvider
manager.render(renderer, menu::MenuRenderMetrics{ viewportWidth, viewportHeight });
if (auto action = manager.consumeAction()) {
    // dispatch action-> "start_game", ...
}
```

## SDL Backend

```cpp
#include "menu/sdl/SDLMenuRenderer.h"
#include "menu/sdl/SDLInputProvider.h"

menu::SDLInputProvider input;
menu::SDLMenuRenderer renderer(sdlRenderer, bitmapFont);
renderer.registerTexture("assets/ui/gotoocorp_logo.png", preloadTexture(...));

// inside SDL event loop:
input.handleEvent(event);

// at end of frame:
manager.update(input);
manager.render(renderer, metrics);
input.endFrame();
```

## Mock Rendering

`menu::MockMenuRenderer` and `menu::MockInputProvider` allow exercising the menu logic without SDL. Captured draw calls and actions can be asserted in unit tests.
