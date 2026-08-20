---
layout: default
title: GUI Runtime
document_id: gui-runtime-guide
locale: en
permalink: /Docs/en/how-to/runtime/gui.html
---

# GUI Runtime

> Engine-owned documentation. This page describes the reusable client
> AngelScript GUI runtime in `Source/Scripting/AngelScript/CoreScripts/Gui.fos`
> and `Input.fos`. It does not define a declarative GUI file format.

## What Engine provides

FOnline provides a script-side object tree, screen stack, layout and drawing
behavior, mouse and keyboard dispatch, focus, dragging, repeated input, grids,
item views, text input, message boxes, and consoles.

The checked machine-readable companion is the
[generated GUI runtime reference](../../reference/gui-runtime/index.md). It includes
all 12 runtime types, their 160 documented API members, 39 callback signatures,
31 top-level screen API overloads, built-in metadata annotations, and the
integration rules on this page.

Engine does **not** parse `.fogui`, `.foguischeme`, XML, JSON, or another GUI
layout format. An embedding project may write AngelScript screen factories by
hand, generate them from a project-owned format, or use a separately versioned
companion tool. In every case, the runtime consumes AngelScript classes and
factories, not the authoring files.

## Source ownership

| Owner | Surface |
|---|---|
| `CoreScripts/Gui.fos` | Object model, screens, layout, drawing, lifecycle, focus, hit testing, drag/drop, grids, item views |
| `CoreScripts/Input.fos` | Key/mouse state and native event forwarding into `Gui` |
| `ClientEngine` | Native input events and the script-enabled `OnRenderIface` draw scope |
| Embedding project | `GuiScreen` and `CursorType` extensions, `GuiScreens::InitializeScreens`, factories, lifecycle/render hook calls |
| Embedding project or companion tool | Declarative layout format, generator, visual editor, styles, screen catalog, project widgets |

Dear ImGui is a different subsystem. The native/server/tool ImGui surfaces and
`Game.ImGui` script bindings do not create or drive the `Gui.fos` screen tree.

## Required project contract

`Gui::EngineCallback_Start()` calls this external symbol:

```angelscript
namespace GuiScreens
{
void InitializeScreens()
{
    Gui::RegisterScreen(
        GuiScreen::Login,
        GuiScreens::LoginScreen::CreateScreen);
}
}
```

The project must also extend `GuiScreen` with every registered id:

```angelscript
///@ Enum GuiScreen Login
```

Engine declares only `GuiScreen::None = 0`. It likewise declares only
`CursorType::Default = 0`; project cursor modes are project metadata.

Importing the complete CoreScripts directory without the matching project
metadata and `GuiScreens` implementation is expected to fail compilation. This
is why the minimal server-only starter does not claim to be a client GUI
example.

## Client event bridge

The runtime implements lifecycle and drawing hooks but does not subscribe all
of them automatically. A client module normally bridges them as follows:

```angelscript
[[Event]] [[Async]]
void OnStart()
{
    // Bind fonts and perform other presentation setup first.
    Gui::EngineCallback_Start();
}

[[Event]]
void OnLoop()
{
    Gui::EngineCallback_Loop();
}

[[Event]]
void OnRenderIface()
{
    Gui::EngineCallback_Draw();
    Gui::EngineCallback_DrawCursor();
}

[[Event]]
void OnScreenSizeChanged()
{
    Gui::Callback_OnResolutionChanged();
}

void ChangeProjectLanguage(string language)
{
    Game.ChangeLanguage(language);
    Gui::Callback_OnLanguageChanged();
}
```

`EngineCallback_DrawCursor()` is separate so a project can place the cursor
above its other interface layers or suppress it for another control mode.

`Input.fos` is different: its `[[ModuleInit]]` subscribes `Game.OnMouseDown`,
`OnMouseUp`, `OnMouseMove`, `OnKeyDown`, `OnKeyUp`, and `OnInputLost` and
forwards them to `Gui`. Include it once; do not add a second copy of the same
forwarding bridge.

## Object model

The runtime hierarchy is:

```text
Object
|- Panel
|  |- Screen
|  |- Grid
|  |  `- ItemView
|  `- Button
|     `- CheckBox
|        `- RadioButton
`- Text
   |- TextInput
   |  `- Console
   `- MessageBox
```

Use the [generated type reference](../../reference/gui-runtime/types.md) for every
member and callback signature.

### `Object`

`Object` owns:

- parent/children linkage and recursive initialization/removal;
- authored base position/size and current absolute geometry;
- active, focused, hovered, pressed, dragged, and highlighted state;
- anchor and dock values;
- hit/catch/transparent-hit/focus-group policy;
- recursive lookup, count, draw, move, focus, mouse, and input callbacks.

Children are drawn in array order and hit-tested in reverse order. A later
child is therefore both visually above and interactively ahead of an earlier
sibling.

`SetHasOnDraw(true)` is required when a generated or hand-written type has an
`OnDraw` or `OnPostDraw` path that the base runtime cannot discover itself.
Without a self draw reason or drawable descendant, `_NeedDraw` may keep the
subtree out of the draw traversal.

### `Panel`

`Panel` adds:

- background sprites and layouts;
- `SetFrameImage` plus source-pixel cap insets for 9-slice frames;
- crop/scissor behavior;
- vertical and horizontal scroll state;
- a 120 ms scroll tween.

`SetBackgroundImage` may adopt the sprite's native size when layout is `None`
or the object has no authored base size. `SetFrameImage` intentionally keeps
the panel's current size so a frame can be resliced into a docked or authored
rectangle.

When crop is enabled, the runtime applies the same rectangle to drawing and hit
testing. Descendants outside the rectangle are neither visible nor clickable.

### Text and input

`Text` draws with a bound `FontType`, `FontFlag` set, normal/focused colors, and
skip-line value. `SetTextWithResize` measures the string and changes height; it
does not solve project translation policy or font coverage.

`TextInput` adds length limits, password masking, cursor/carriage handling,
clipboard behavior, focus-driven on-screen keyboard activation, and a 1000 ms
last-character reveal window for passwords.

`Console` adds activation, history, submission, and
`Game.OnConsoleMessage`. `MessageBox` maintains parallel text/type/time arrays,
filters by `MessageBoxType`, and reads `Gui.MsgboxInvert` as its default
inversion policy.

### Buttons and selection

`Button` adds pressed, hover, disabled, switched, and condition images/state.
`CheckBox` exposes `OnCheckedChanged`. A checked `RadioButton` clears switched
radio siblings under the same parent before firing its own change callback.

### Grids and item views

`Grid` clones a named object prototype into indexed cells. The prototype is
hidden; `ResizeGrid` creates clones, attaches grid/cell metadata recursively,
fires resize callbacks, initializes clones, and positions them by columns and
padding.

`ItemView` obtains client `Item` handles through `OnGetItems`, rejects or sorts
them through `OnCheckItem`, binds live items to cells, and invokes
`OnDrawItem`. Its handle matching deliberately rejects destroyed handles and
can replace a stale handle with a live instance carrying the same entity id.

## Layout contract

`Object::_RefreshPosition()` starts from authored `_BasePos` and `_BaseSize`.
For roots, the live parent size is `Settings.View.ScreenWidth` by
`Settings.View.ScreenHeight`. For descendants it is the current parent size.

### Dock

Any non-`None` dock value takes precedence over anchors:

| Dock | Position and size |
|---|---|
| `Left` | Parent top-left; keep width, fill parent height |
| `Right` | Parent right edge; keep width, fill parent height |
| `Top` | Parent top-left; fill parent width, keep height |
| `Bottom` | Parent bottom edge; fill parent width, keep height |
| `Fill` | Parent rectangle |

### Anchor

When Dock is `None`, each axis is resolved separately:

- `Left` / `Top` keeps the authored offset from that parent edge;
- `Right` / `Bottom` adds the live-size minus authored-parent-base-size delta;
- an axis with no matching edge bit is centered by half that delta.

Call `Gui::Callback_OnResolutionChanged()` after a viewport change. It walks
every registered screen and recursively recomputes positions.

## Screen registration and lifetime

`RegisterScreen` first unregisters the previous id, ignores `GuiScreen::None`,
stores the factory, and constructs one screen immediately. That first
construction initializes the object tree and precaches referenced resources.

For a regular screen, the instance remains registered and is reused. For a
multiinstance screen, the precache instance is removed and each `ShowScreen`
creates a new instance.

`GuiScreen::Cursor` is special: its instance is held in `CursorScreen`, kept
active, and excluded from the ordinary `Screens` stack.

### Showing

`ShowScreen` disables map scroll directions, then:

1. reuses an existing non-multiinstance screen or constructs a new instance;
2. makes the previous top active screen disappear;
3. focuses the first text input and establishes hover;
4. moves the shown screen to the top;
5. activates it;
6. runs `OnShow()` and `OnShow(params)` recursively;
7. runs `OnAppear()` recursively;
8. fires `Game.OnScreenChange(true, id, params)`.

`OnShow` is suitable for data refresh. `OnAppear` means the screen has become
the visible top layer.

### Hiding

`HideScreen` hides only one matching active instance per call:

1. run `OnDisappear`;
2. run `OnHide`;
3. deactivate the screen;
4. fire `Game.OnScreenChange(false, id, params)`;
5. remove a multiinstance tree;
6. move the next active screen to the top and run its `OnAppear`.

`HideAllScreens` repeats this top-down until no active screen remains.

## Drawing order

Native `ClientEngine` enables script drawing around `Game.OnRenderIface`. A
project must call `Gui::EngineCallback_Draw()` inside that event.

The screen pass walks `Screens` in stack order and draws every active screen.
Within a drawable object:

1. item and `OnDraw` callbacks run;
2. the object's background or text is drawn by its concrete type;
3. active non-dragged children draw in order;
4. `OnPostDraw` runs.

A dragged object is skipped in its normal subtree and drawn by the separate
cursor pass. The optional drop menu then draws, followed by the cursor screen.

Drawing outside `OnRenderIface` is not supported merely because a script method
is callable. `ClientEngine::CanDrawInScripts` scopes legal script rendering.

## Mouse input

Mouse down starts at the top active screen:

- clear ordinary focus on left down;
- cancel any previous press as lost;
- run the screen-global callback;
- notify the optional drop menu;
- hit-test the screen tree;
- store and press the hit object;
- focus it on left down.

If no object is hit, a left click closes an `IsCloseOnMiss` screen. A nonmodal
screen may then search lower active screens and bring the first modal or hit
screen forward. A modal screen never falls through.

Mouse up runs global release, releases the matching pressed object, emits click
only when the pointer still hits it, and dispatches drag/drop handlers in
registration order until one handles the target.

`CheckTransparentOnHit` uses sprite alpha for panel self-hit testing. It does
not make arbitrary child geometry transparent.

## Keyboard and focus

Key down dispatch order is:

1. global input on active screens from top to bottom;
2. the current console;
3. focused objects when no active console owns input.

`FocusGroup` focuses an entire parent subtree rather than one node.
`NextTextInput` performs a depth-first traversal of the active screen.

`Input.fos` tracks 256 key states and mouse buttons through
`MouseButton::Ext4`. It suppresses repeated key-down events except `Text`,
`Space`, `Back`, `Delete`, `Left`, and `Right`.

## Press, drag, and input loss

After mouse down, `OnMousePressed` repeat starts after 500 ms and repeats every
40 ms while the pointer still hits the pressed object.

A draggable object enters drag state after its own threshold logic. During
release, project handlers receive the dragged object and current hit target.
The first handler returning `true` owns the drop.

On input loss, `Input.fos` releases all tracked keys/buttons. `Gui` clears
hover, focus, and press state, reports the release as lost, and notifies the
drop menu. This is the contract that prevents sticky input after alt-tab,
minimize, or focus loss.

Mouse down is ignored while `Game.IsConnecting()`. The loop also releases
mouse state while connecting.

## Touch boundary

`ClientEngine` emits separate `OnTouchDown`, `OnTouchMove`, `OnTouchUp`, tap,
scroll, and zoom events. The stock `Input.fos` GUI bridge subscribes mouse,
keyboard, and input-loss events only.

Do not claim touch support from a passing mouse test. A touch-capable project
must provide and visibly validate its own touch-to-action or touch-to-mouse
adaptation.

## Authoring approaches

An embedding project may:

1. write `Gui::Screen` subclasses and factories by hand;
2. generate those classes from a project-owned declarative source;
3. consume a separately versioned companion GUI package.

Whichever approach is used, generated output should:

- derive classes from the exact Engine runtime types;
- use the callback signatures in the generated reference;
- emit project enum metadata and `Gui::RegisterScreen` calls;
- never require Engine to read the authoring source at runtime;
- remain reproducible and checked into the project when the project build
  consumes checked-in generated scripts.

## Diagnostics

| Symptom | First check |
|---|---|
| Missing `GuiScreen` / `GuiScreens` at compile time | Project metadata and `GuiScreens::InitializeScreens` |
| Screen exists but never appears | `RegisterScreen`, project start hook, active id, factory |
| Screen shows behind another screen | Active stack order, modal state, `BringToFront` |
| Click reaches the wrong object | Child order, active state, crop rectangle, transparent hit, modal fallthrough |
| UI does not resize | Project `OnScreenSizeChanged` bridge and anchor/dock ownership |
| Language changes but text stays stale | `Callback_OnLanguageChanged`, `OnRefreshText`, font glyph coverage |
| `OnPostDraw` never runs | `SetHasOnDraw(true)` or another self/descendant draw reason |
| Drag image duplicates | Normal subtree still drawing the dragged object or cursor pass called twice |
| Key/button remains held after focus loss | Duplicate/custom input bridge bypassing `Input::InputLost` |
| Touch does nothing | Missing project touch adaptation |

## Validation workflow

Run the Engine-owned structural checks:

```powershell
python BuildTools\docs_gui_runtime.py --check
python -m unittest BuildTools.tests.test_docs_gui_runtime
python BuildTools\docs_validate.py
```

Then validate the embedding project:

1. regenerate its screen code from the owning source, if any;
2. compile/bake the client with the exact Engine revision;
3. show every changed screen at minimum, maximum, and one nonstandard
   supported resolution;
4. exercise mouse, keyboard, focus, modal, close-on-miss, repeat, drag/drop,
   and input-loss paths;
5. switch every shipped language and inspect glyphs, wrapping, clipping, and
   refreshed geometry;
6. compare visible output on every claimed renderer/platform;
7. inspect logs for script exceptions, missing sprites/fonts, and failed
   screen factories.

Headless success is not visual or interaction proof. There is no self-contained
native fixture in Engine that constructs this CoreScripts runtime,
renders screen pixels, drives input, changes resolution/language, and proves
teardown. Keep the complete surface experimental until that gap is closed.

## Project boundary

Keep these concerns out of reusable Engine documentation:

- concrete screen ids, layouts, coordinates, styles, and assets;
- project `.fogui`-like grammar and editor behavior;
- generated project namespace and file paths;
- project font slots, text keys, accessibility targets, and localization
  policy;
- gameplay actions, inventory semantics, HUD rules, and drag/drop effects;
- project screenshot baselines and platform acceptance reports.

Engine documentation may define the runtime contract those systems consume.
The embedding project owns how it produces, styles, and validates its actual
interface.

## Maintenance

Update this guide, `BuildTools/GuiRuntimeInterface.json`, and the generated
reference when any of these change:

- `Gui.fos` classes, documented members, callbacks, enums, settings, events, or
  top-level API;
- screen registration, stack, show/hide, focus, cursor, drag/drop, grid, item,
  layout, crop, frame, text, message, or console behavior;
- `Input.fos` subscriptions, key/mouse state, repeat, dispatch, or input loss;
- native client input dispatch or script render scope;
- required project hooks or the `GuiScreens` external contract;
- the decision about whether a GUI generator is Engine-owned, companion-owned,
  or project-owned.

Regenerate in dependency order:

```powershell
python BuildTools\docs_gui_runtime.py --write
python BuildTools\docs_contract_diff.py --baseline-git-ref <base> --write --enforce
python BuildTools\docs_site.py --write
python BuildTools\docs_ai_delivery.py --write
python BuildTools\docs_validate.py
```
