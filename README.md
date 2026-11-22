# rlguipp — Usage Guide

`rlguipp` is an immediate-mode GUI layout wrapper built on top of [raygui](https://github.com/raysan5/raygui). It adds hierarchical layout containers, column grids, tab views, scroll panels, popups and window boxes, popup menus and a file dialog on top of raygui's flat widget API. All symbols live in the `gui::` namespace.

> NOTE: This project exists mainly to allow me to use the wrapper in multiple projects. I would not recommend it as
> a stable choice for your own project, unless you are okay with API changes and little documentation. 

---

## Table of Contents

1. [Headers and building](#headers-and-building)
2. [Frame structure](#frame-structure)
3. [API conventions](#api-conventions)
4. [Ids and widget state](#ids-and-widget-state)
5. [Containers](#containers)
6. [Layout control](#layout-control)
7. [Column layout](#column-layout)
8. [Tab views](#tab-views)
9. [Table views](#table-views)
10. [Popups and windows](#popups-and-windows)
11. [Widgets](#widgets)
12. [Custom widgets](#custom-widgets)
13. [LED display widgets](#led-display-widgets)
14. [Code editor](#code-editor)
15. [Popup menus](#popup-menus)
16. [File dialog](#file-dialog)
17. [File tree](#file-tree)
18. [Styles and the raygui DEFAULT propagation trap](#styles-and-the-raygui-default-propagation-trap)
19. [Common pitfalls](#common-pitfalls)
20. [Tests](#tests)
21. [Local raygui changes](#local-raygui-changes)

---

## Headers and building

| Header | Content |
|---|---|
| `rlguipp/rlguipp.hpp` | Core: containers, layout, widgets, popup menus, custom widget support (includes raygui) |
| `rlguipp/rlguipp_filedialog.hpp` | `ModalFileDialog` and its `FileDialogInfo` |
| `rlguipp/rlguipp_leds.hpp` | `SevenSegmentDisplay`, `Til311Display` |
| `rlguipp/rlguipp_codeeditor.hpp` | `CodeEditor`, `SyntaxHighlighter`, `CodeTextStyle`, `DrawCodeLine` |
| `rlguipp/rlguipp_filetree.hpp` | `FileTree`, `FileTreeMark` |
| `rlguipp/rlguiapp.hpp` | Optional `gui::Application` base class that handles the window, render texture and `BeginGui`/`EndGui` |
| `rlguipp/rlguipp_font.hpp` | The built-in pixel font used by `gui::Application` |

The CMake target is `rlguipp` (also `rlguipp::rlguipp`). It links against raylib (target name from the cache variable `RLGUIPP_RAYLIB_TARGET`, default `raylib_static` if it exists, else `raylib`) and `ghc_filesystem`. raygui is compiled into `rlguipp.cpp`, so do not define `RAYGUI_IMPLEMENTATION` anywhere else.

### Project layout and dependencies

| Path | Content |
|---|---|
| `rlguipp/` | The library sources, the include root is the project folder (`#include <rlguipp/rlguipp.hpp>`) |
| `external/ghc/utf8.hpp` | UTF-8 helper, only used by the implementation |
| `examples/` | The showcase, also the golden image test |
| `test/` | Input tests and the `StyleManager` of the examples |
| `tools/` | `fontgenerator`, creates `rlguipp_font.cpp`'s `fontRom` from `tools/font.txt` |
| `cmake/Dependencies.cmake` | Fetches the release archives of raylib 6.0, ghc::filesystem 1.5.14 and (examples/tests only) fmt 11.2.0 |

Built on its own (`cmake -S . -B build -G Ninja`) it also builds the showcase, the tests and the tools; as a dependency only the library (`RLGUIPP_BUILD_EXAMPLES`, `RLGUIPP_BUILD_TESTS`, `RLGUIPP_BUILD_TOOLS` default to "top level"). A parent project can pin other versions of the dependencies by declaring them first (`FetchContent_Declare(raylib ...)`, `FetchContent_Declare(ghcfilesystem ...)`) or by creating the targets `raylib` and `ghc_filesystem` before rlguipp.

The library uses the forwarding header `ghc/fs_fwd.hpp`, so exactly one source of the application has to `#include <ghc/fs_impl.hpp>`, or the application sets `RLGUIPP_GHC_FS_IMPL=ON` to have it compiled into rlguipp.

```cmake
include(FetchContent)
FetchContent_Declare(rlguipp URL "https://github.com/gulrak/rlguipp/archive/refs/tags/v0.5.0.tar.gz")
FetchContent_MakeAvailable(rlguipp)
target_link_libraries(myapp PRIVATE rlguipp::rlguipp)
```

---

## Frame structure

Every frame, the entire UI description must be wrapped in `BeginGui` / `EndGui`:

```cpp
gui::BeginGui();          // full-window mode
// ... widgets, containers ...
gui::EndGui();
```

To render into a `RenderTexture` (e.g. for scaling or off-screen passes):

```cpp
gui::BeginGui({0, 0, (float)rt.texture.width, (float)rt.texture.height},
              &rt, mouseOffset, guiScale);
// ...
gui::EndGui();
```

Dropdown boxes, popups, window boxes, popup menus and tooltips are drawn deferred in `EndGui()`, so they appear on top of everything else.

---

## API conventions

- **Value widgets** take a pointer to the value they edit and return `true` when the user changed the value in this frame:
  ```cpp
  if (gui::CheckBox("Sound", &cfg.sound)) saveConfig();
  if (gui::Slider("Volume", &cfg.volume, 0.0f, 1.0f)) applyVolume();
  ```
- **Edit widgets** (`TextBox`, `TextBoxMulti`, `Spinner`, `ValueBox`) also report the end of an edit (Enter, or a click somewhere else) via `IsLastWidgetEditFinished()`. Use it when an action should only run once the input is complete:
  ```cpp
  gui::TextBox(path, 4096);
  if (gui::IsLastWidgetEditFinished()) loadDirectory(path);
  ```
- **Value pointers are only storage**: rlguipp reads and writes the value through the pointer during the frame it was passed in (deferred widgets like dropdowns and popups until `EndGui()`), but never keeps it for a later frame. Identity is given by [ids](#ids-and-widget-state), not by addresses.
- **Parameter order**: size or area first, then the label or id, then the value pointers, then options.
- **Containers**: a `Begin*()` that returns `void` must always be closed with its `End*()`. A `Begin*()` that returns `bool` must only be closed if it returned `true`.
- **Misuse** like unbalanced `Begin*`/`End*` or `PushId`/`PopId` calls, an `End*()` of the wrong type, or a widget outside of `BeginGui`/`EndGui` asserts in debug builds. Release builds log an error with `TraceLog` (once per message) and repair the state: an `End*()` closes what was left open inside of its container, `EndGui()` closes everything still open, an `End*()` or `PopId()` without matching `Begin*()`/`PushId()` is ignored. Define `RLGUIPP_ASSERT` when compiling rlguipp (e.g. empty) to replace the assert.

---

## Ids and widget state

Some widgets and containers keep state between frames: popups and window boxes (their render texture and position), table views (the content size of the last frame), dropdown boxes (open or not) and the keyboard focus. This state is kept inside rlguipp and identified by an **id**, similar to Dear ImGui: the hash of the widget's label or id string and the current **id scope**.

```cpp
gui::DropdownBox("Red;Green;Blue", &color);            // id from the label "Red;Green;Blue"
gui::TextBox("name", name, 64);                        // TextBox has no label, so it takes an id string
gui::BeginTableView(200, "files", 3, &scroll);         // so do popups and table views
```

- **Id scopes**: `PushId(const char*)`, `PushId(int)` and `PushId(const void*)` start a scope, `PopId()` ends it. Use them when the same widgets are shown several times, e.g. once per row or per object:
  ```cpp
  for (int i = 0; i < count; ++i) {
      gui::PushId(i);
      gui::Spinner("Value", &values[i], 0, 99);   // a different id in every row
      gui::PopId();
  }
  ```
  A pointer given to `PushId(const void*)` is only hashed, never dereferenced.
- Popups, window boxes, table views and the active tab start an id scope for their content (with their own id), the other containers don't, so wrapping widgets in `Begin()`/`End()` doesn't change their ids.
- **Labels with a hidden part**: labels that are also ids (`Spinner`, `ValueBox`, `DropdownBox`, `BeginTab`, `BeginWindowBox`) can have a part that is not shown. `"Text##id"` shows `Text` and uses the whole label as id, so two widgets can show the same (or no) text. `"Text###id"` uses only `###id`, so the shown text can change without changing the id, e.g. for a window title with a file name in it.
  ```cpp
  gui::Spinner("##width", &width, 1, 100);     // no visible label
  gui::BeginWindowBox(area, (fileName + "###editor").c_str(), &open, flags);
  ```
- `GetId(label)` returns the id a widget with that label or id string gets in the current scope, e.g. for `SetKeyboardFocus()`. Outside of `BeginGui`/`EndGui` it uses the root scope.
- **Pruning**: the state of an id that was not used in a frame is dropped in the next `BeginGui()`, the render texture of a popup is unloaded with it. A popup that is closed (or not described for a frame) opens again at its initial position, a focused text box that is not shown loses the focus. A table view that was not shown keeps its scroll position (the scroll vector is the caller's), only its first frame after that doesn't know the content size yet.
- Pruning by frame means one `BeginGui`/`EndGui` per frame, a second GUI in the same frame would drop the state of the first one.

---

## Containers

| Container | Returns | Notes |
|---|---|---|
| `Begin()` / `End()` | `void` | Group, children stacked vertically |
| `BeginColumns()` / `EndColumns()` | `void` | Children placed left to right |
| `BeginPanel(title)` / `EndPanel()` | `void` | Bordered panel, title bar if a title is given |
| `BeginGroupBox(text)` / `EndGroupBox()` | `void` | Border with the text inset into the top edge |
| `BeginScrollPanel(height, content, &scroll)` / `EndScrollPanel()` | `void` | `content` is the virtual size, `scroll` the offset |
| `BeginScrollView(height, id)` / `EndScrollView()` | `void` | No border, measures its content and shows a vertical scroll bar only when it is needed, keeps the scroll position by `id`, the content height of the last frame decides about the scroll bar |
| `BeginTabView(&activeTab)` / `EndTabView()` | `void` | See [Tab views](#tab-views) |
| `BeginTab(text)` / `EndTab()` | `bool` | `true` for the active tab |
| `BeginTableView(height, id, columns, &scroll)` / `EndTableView()` | `void` | See [Table views](#table-views) |
| `BeginPopup(area, id, &isOpen)` / `EndPopup()` | `bool` | Returns `isOpen` |
| `BeginWindowBox(area, title, &isOpen, flags)` / `EndWindowBox()` | `bool` | `false` if not open or just closed, the title is also the id |
| `BeginClipping(rect)` / `EndClipping()` | `void` | Clips rlguipp/raygui drawing to `rect` (intersected with the outer clip area) |

---

## Layout control

| Function | Effect |
|---|---|
| `SetNextWidth(float px)` | Width in pixels for the **next widget only**. |
| `SetNextRelativeWidth(float f)` | Width of the next widget as fraction of the parent's content width. |
| `SetDefaultWidth(float px)` | Width for **all following widgets** of this container, until changed again; `<= 0` goes back to the remaining width. |
| `SetNextHeight(float px)` / `SetNextRelativeHeight(float f)` | Height override for the next widget only. |
| `SetRowHeight(float h)` | Default row height for all following widgets in this container. Default is `DEFAULT_ROW_HEIGHT` (26 px). |
| `SetSpacing(float s)` | Vertical spacing between rows (or horizontal spacing between columns inside `BeginColumns`). |
| `SetIndent(float w)` | Indent the following widgets by `w` pixels. |
| `SetReserve(float w)` | Reserve `w` pixels on the right side for the right labels of bar widgets (sliders etc.). |
| `Space()` / `Space(float s)` | Insert blank space of the current spacing, or of `s` (the spacing follows as after any element), a negative `s` inserts nothing, so filling up to a position that was already passed doesn't push on. |
| `Separator(float s = -1)` | Horizontal rule. |
| `GetCurrentPos()` | Top-left position the next widget will use. |
| `GetContentAvailable()` | Rectangle from the current position to the content edges. |
| `GetLastWidgetRect()` | Position and size of the widget just drawn. |
| `SetTooltip(str)` | Attach a tooltip to the last widget's rectangle. A `\n` starts a new line, such a tooltip stays visible longer. |

Without an explicit width, a widget takes the **remaining** width of its container.

---

## Column layout

`BeginColumns` / `EndColumns` places its direct children left to right. As every widget takes the remaining width by default, the first child would take all of it, so **give every column but the last a width**:

```cpp
gui::BeginColumns();
    gui::SetNextRelativeWidth(0.25f);  gui::Begin(); /* left quarter  */ gui::End();
    gui::SetNextWidth(80);             gui::Begin(); /* 80 px         */ gui::End();
    /* no width */                     gui::Begin(); /* fills the rest */ gui::End();
gui::EndColumns();
```

For a row of equally sized widgets, `SetDefaultWidth` avoids repeating `SetNextWidth`:

```cpp
gui::BeginColumns();
gui::SetDefaultWidth(20);
if (gui::Button("+")) add();
if (gui::Button("-")) remove();
if (gui::Button(GuiIconText(ICON_PENCIL, ""))) edit();
gui::EndColumns();
```

Don't give all N columns the same fraction: rounding and the spacing between columns push the total past 100 %. Leave the last column without a width instead.

---

## Tab views

```cpp
int activeTab = 0;

gui::BeginTabView(&activeTab);
if (gui::BeginTab("General")) {
    // widgets for this tab
    gui::EndTab();
}
if (gui::BeginTab("Advanced")) {
    // widgets for this tab
    gui::EndTab();
}
gui::EndTabView();
```

`BeginTab` draws the tab label and returns `true` only for the active tab; only then call `EndTab()`. The label is the id scope of the tab's content. Inactive labels are drawn at ~45 % opacity of the label text colour, so they don't depend on a per-control style (see the [DEFAULT propagation trap](#styles-and-the-raygui-default-propagation-trap)).

---

## Table views

A virtualised list of rows. The columns are given once in `BeginTableView`, only visible rows are drawn.

```cpp
using gui::TableColumn;
Vector2 scroll = {0, 0};
gui::BeginTableView(300.0f, "files", {TableColumn::Stretch(), TableColumn::Fixed(80), TableColumn::Fixed(120)}, &scroll);
for (const auto& row : data) {
    if (gui::TableNextRow(20.0f)) {        // false = row is not visible
        if (gui::TableNextColumn()) { gui::Label(row.name.c_str()); }
        if (gui::TableNextColumn()) { gui::Label(row.size.c_str()); }
        if (gui::TableNextColumn()) { gui::Label(row.date.c_str()); }
    }
}
gui::EndTableView();
```

- `TableColumn::Fixed(px)` is a column of that width in pixels.
- `TableColumn::Stretch(weight = 1)` columns share the width the fixed columns leave (the table width minus the vertical scrollbar), split by their weights. Two `Stretch()` columns get half each, `Stretch(2)` and `Stretch(1)` two thirds and one third.
- If the fixed columns are wider than the table, the stretch columns get no width and the table scrolls horizontally.
- A row may use fewer columns than defined, calling `TableNextColumn()` more often is an error.
- Row heights can differ between rows, but must not change between frames while the table is shown.
- `TableNextRow(height, handler)` and `TableNextColumn(handler)` call the handler with the cell rectangle when it is visible; inside a column handler drawing is clipped to the cell. Use the `*Clipped` draw functions (`DrawTextClipped`, `DrawRectangleClipped`) for custom drawing.
- `SimpleListView(height, id, items, &scroll, &active)` is a ready-made one-column list on top of this.

---

## Popups and windows

### BeginWindowBox / EndWindowBox

A floating window with a title bar, identified by its title (see [labels with a hidden part](#ids-and-widget-state) for titles that change).

```cpp
Rectangle area = {-1, -1, 400, 300};  // negative x/y: centered
if (gui::BeginWindowBox(area, "My Window", &winOpen, gui::WindowBoxFlags(gui::WBF_CLOSABLE | gui::WBF_MOVABLE))) {
    // widgets inside the window
    gui::EndWindowBox();
}
```

| Flag | Effect |
|---|---|
| `WBF_CLOSABLE` | Shows a close button, clicking it sets `winOpen = false` and `BeginWindowBox` returns `false` |
| `WBF_MOVABLE` | The window can be dragged by its title bar |
| `WBF_MODAL` | Darkens the rest of the GUI and takes all input |

### BeginPopup / EndPopup

A frameless popup identified by `id` that is shown while `isOpen` is `true`:

```cpp
auto anchor = gui::GetLastWidgetRect();
if (gui::BeginPopup({anchor.x, anchor.y + anchor.height + 2, 120, 56}, "actions", &popupOpen)) {
    if (gui::Button("Action")) popupOpen = false;
    gui::EndPopup();
}
```

`isOpen` is only read in the frame it is given, so it may move or be destroyed while the popup is open, the popup is just not shown anymore. The position of a moved window is part of its state, it is reset when the window was closed or not described for a frame. A popup that is described with another size gets a texture of the new size.

### BeginDrawer / EndDrawer

A popup that sits at its area and closes itself: Escape or a click outside of it (pressed and released outside) set `isOpen` to `false`. It follows the area when it changes, e.g. after a resize of the window. Clicks on a popup or modal window above it, and the click that closes a popup menu, are not outside. It is meant for a panel that slides over the content, like a file tree over an editor:

```cpp
if (gui::Button("Files"))
    filesOpen = !filesOpen;  // handled before BeginDrawer(), so a click on the button while open closes it
if (gui::BeginDrawer({editorArea.x, editorArea.y, 220, editorArea.height}, "files", &filesOpen)) {
    if (auto file = fileTree.render())
        open(*file), filesOpen = false;
    gui::EndDrawer();
}
```

A drawer that stays open (pinned) is just the same content in the normal layout. A toggle button described after `BeginDrawer()` would open it again with the click that closed it.

---

## Widgets

| Widget | Returns | Notes |
|---|---|---|
| `Label(text)` | — | Non-interactive text |
| `LabelWrapped(text)` | — | Like `Label`, keeps line breaks and wraps at spaces to its width, one row height per line |
| `Button(text)` | `bool` clicked | |
| `ButtonMulti(text)` | `optional<MouseButton>` | Tells which mouse button clicked it |
| `LabelButton(text)` | `bool` clicked | Looks like a label, acts like a button |
| `Toggle(text, &active)` | `bool` changed | |
| `ToggleGroup(text, &active)` | `bool` changed | Items separated by `;`, rows by `\n` |
| `CheckBox(text, &checked)` | `bool` changed | |
| `ComboBox(text, &active)` | `bool` changed | Items separated by `;` |
| `DropdownBox(label, &active, up)` | `bool` changed | Items separated by `;`, the label is also the id. Drawn in `EndGui()`, so a selection is reported in the following frame |
| `Spinner(label, &value, min, max)` | `bool` changed | Edit widget, the label is also the id |
| `ValueBox(label, &value, min, max)` | `bool` changed | Edit widget, the label is also the id |
| `TextBox(id, buffer, bufferSize)` / `TextBox(id, std::string&, maxLength)` | `bool` changed | Edit widget, see [Text editing](#text-editing) |
| `TextBoxMulti(height, id, std::string&, maxLength)` | `bool` changed | Edit widget, wraps at spaces and scrolls, Enter adds a line break, `height <= 0` uses the remaining height |
| `Slider(left, right, &value, min, max)` / `Slider(left, &value, min, max)` | `bool` changed | The second form shows the value right |
| `SliderBar(...)` | `bool` changed | Same forms as `Slider` |
| `ProgressBar(left, right, value, min, max)` | — | |
| `StatusBar(text)` / `StatusBar({{width, text}, ...})` | — | Bottom bar of the current container |
| `ListView(height, text, &scrollIndex, &active)` | `bool` changed | Items separated by `;` |
| `ListViewEx(height, items, count, &scrollIndex, &active, &focus)` | `bool` changed | |
| `ColorPicker(&color)` / `ColorPanel(text, &color)` | `bool` changed | |
| `ColorBarAlpha(text, &alpha)` / `ColorBarHue(text, &hue)` | `bool` changed | |
| `Grid(height, spacing, subdivs)` | `Vector2` cell under the mouse | |

### Text editing

`TextBox` and `TextBoxMulti` are implemented by rlguipp, not by raygui's `GuiTextBox` (it has no selection, no clipboard and no undo, and can't wrap or scroll). They use the `TEXTBOX` style, so they look like raygui widgets.

| Input | Action |
|---|---|
| Click, drag, Shift+click | Place the cursor, select, extend the selection |
| Double click / triple click | Select a word / all (`TextBox`) or the paragraph (`TextBoxMulti`) |
| Cursor keys, Home/End, Page Up/Down | Move, with Shift extend the selection. Up/Down in a `TextBox` go to the start/end |
| Alt (macOS) or Ctrl + Left/Right/Backspace/Delete | Move or delete by words |
| Cmd+Left/Right/Up/Down, Cmd+Backspace (macOS), Ctrl+Home/End (others) | Line start/end, text start/end, delete to the line start |
| Sys+A/C/X/V | Select all, copy, cut, paste |
| Sys+Z, Sys+Shift+Z, Ctrl+Y (not macOS) | Undo, redo, consecutive typing is undone at once |
| Enter | Ends the editing of a `TextBox`, adds a line break in a `TextBoxMulti` |
| Escape, a click elsewhere | Ends the editing |
| Mouse wheel | Scrolls a `TextBoxMulti` |

- Sys is Cmd on macOS (also in the web build on a Mac) and Ctrl elsewhere, see `IsSysKeyDown()`. The letter shortcuts use the key showing the letter in the current keyboard layout, so Undo is on the key labeled Z also with a German or French layout. `IsShortcutPressed(letter)` does that check for custom widgets.
- The text is UTF-8, the cursor moves by code points. Pasted text loses control characters, a `TextBox` gets line breaks and tabs as spaces.
- The maximum length (in bytes) only limits what the user enters or pastes. Text set by the application is shown and kept as it is, a change of it from outside drops the undo history.
- `GuiSetStyle(TEXTBOX, TEXT_READONLY, 1)` around the call allows selecting and copying, but no changes.
- The clipboard is raylib's `GetClipboardText()`/`SetClipboardText()`, `SetClipboardHandler(get, set, pasteRequested)` replaces it. The optional `pasteRequested` replaces the paste shortcut check, for the web build, where a browser hands out the clipboard text only with its paste event. `CopyToClipboard(text)` writes to the same clipboard, `PasteFromClipboard()` reads it, and `IsPasteRequested()` tells a custom widget that the user asked to paste (the shortcut or the `pasteRequested` of the handler).

The keyboard focus is an [id](#ids-and-widget-state): `SetKeyboardFocus(gui::GetId("name"))` focuses the edit widget with that label or id in the current scope, `HasKeyboardFocus(id)` checks it, `SetKeyboardFocus(0)` removes the focus. The focus is dropped after a frame in which the focused widget was not shown. A custom widget can use the focus too: it takes an id, calls `SetKeyboardFocus(id)` when clicked and `HasKeyboardFocus(id)` every frame, which also keeps the focus alive, also when called outside of `BeginGui`/`EndGui` (e.g. in an update function before the gui).

---

## Custom widgets

A custom widget reserves its place in the layout with `AllocateWidget()` and draws into the returned rectangle:

```cpp
void MyWidget(int64_t value)
{
    // natural size 120x20, SetNextWidth()/SetNextHeight() override it,
    // a component <= 0 uses the standard size (remaining width, row height)
    Rectangle r = gui::AllocateWidget({120, 20});

    Color fg = Fade(GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL)), gui::GetAlpha());
    DrawRectangleLinesEx(r, 1, fg);
    auto text = TextFormat("%lld", (long long)value);
    auto size = gui::GetTextSize(text);
    DrawTextEx(GuiGetFont(), text, {r.x + (r.width - size.x) / 2, r.y + (r.height - size.y) / 2},
               (float)GuiGetStyle(DEFAULT, TEXT_SIZE), (float)GuiGetStyle(DEFAULT, TEXT_SPACING), fg);
}
```

- `AllocateWidget()` advances the layout, so call it once per widget, also when the widget ends up not drawing anything.
- `GetAlpha()` is the alpha set with `GuiSetAlpha()`, fade custom colours with it like raygui does.
- Use `DrawRectangle` (integer arguments) for pixel aligned grids of small elements, float positions accumulate rounding differences.

---

## LED display widgets

`#include <rlguipp/rlguipp_leds.hpp>`

```cpp
gui::SevenSegmentDisplay(20.0f, value, 4, 2, false);  // height, value, digits, decimal point (0 = none, 1 = rightmost), hex mode
gui::Til311Display(3, value, 4, 0, true);             // dot size (integer), value, digits, decimal point, hex mode
```

- Every digit always has a decimal point slot to its right, lit or dim, so the width never changes with `decimalPos`.
- Lit segments use `DEFAULT, TEXT_COLOR_NORMAL`, dark ones `DEFAULT, TEXT_COLOR_DISABLED`. Set both temporarily for LED colours, e.g. with the `StyleManager::Scope` of the examples:

```cpp
{
    StyleManager::Scope led;
    led.setStyle(Style::TEXT_COLOR_NORMAL,   Color{0xff, 0x22, 0x00, 0xff});  // on
    led.setStyle(Style::TEXT_COLOR_DISABLED, Color{0x28, 0x04, 0x00, 0xff});  // off: same hue, 5-15 % brightness
    gui::Til311Display(3, value, 4, 0, true);
}
```

- For the TIL311, the dot pitch is `2 * dotSize` and the decimal point sits one `dotSize` below the character, like on the real part (which was only ever made in red).

---

## Code editor

`#include <rlguipp/rlguipp_codeeditor.hpp>`

`CodeEditor` is a source code editor with line numbers, syntax highlighting, a find and replace bar, undo/redo and the clipboard of the text widgets. Unlike the other widgets it is an object: it keeps the text, the cursor, the undo history and the line index, `render()` shows it and handles the input.

```cpp
MyHighlighter highlighter;               // a gui::SyntaxHighlighter, see below
gui::CodeEditor editor{"source"};        // the id is the one of its keyboard focus
editor.setHighlighter(&highlighter);     // not owned, nullptr shows plain text
editor.setText(source);
editor.clearHistory();                   // setText() can be undone, unless the history is cleared
// every frame:
editor.render(300);                      // height, the width is the remaining one, <= 0 uses the remaining height
if (editor.contentRevision() != savedRevision)
    ...                                  // the text changed
```

The editing works like in a `TextBoxMulti` (see [Text editing](#text-editing)): the same keys and shortcuts, word movement, double and triple click, and the same clipboard. The differences:

| Input | Action |
|---|---|
| Up/Down, Page Up/Down | Move by text lines, lines are not wrapped |
| Triple click | Select the line |
| Tab | Insert spaces up to the next multiple of four columns |
| Sys+F, Sys+R | Toggle the find bar, the find and replace bar (`Aa` case sensitive, `.*` regular expression, the arrows go to the next and previous match) |
| Replace, Rep.all | Replace the selected match and select the next one, replace all matches as one undo step |
| Escape | Close the bar and clear the selection, the editor keeps the focus |

- Typing is undone word by word: a word typed after a space starts a new undo step, so does typing after a pause of more than a second (the text widgets undo all characters typed one after another at once).
- The undo history keeps the edits instead of copies of the text, so it has no limit; `clearHistory()` empties it.
- The font must be monospaced: `CodeTextStyle` gives the font (the gui font if none), its size, the width of a character cell and the line height. Every code point takes one cell, a tab too, lines are not wrapped. The defaults fit the 8 pixel font of `gui::Application`.
- While the gui is locked (a popup menu or a modal window is open), the editor takes no input, but keeps its focus.
- `contentRevision()` changes with every change of the text, `canUndo()`, `line()`, `column()` and `selection()` (byte offsets) give the editing state.
- For actions of the application (a context menu, a tool) the editor edits like the user: `insert(text)` replaces the selection, `replaceRange(start, end, text)` any range, each as one undo step. `select(start, end)` and `goTo(line, column)` move the cursor, which the next `render()` scrolls into view. Offsets are byte offsets into the UTF-8 text.
- `contextAt(offset)` describes a position as a `CodeContext`: line and column (in code points), the selection, the word there (by the double click rule), the line text and the style id the highlighter gives the character.

### Context menu

A right-click into the text opens a context menu with Undo, Redo, Cut, Copy, Paste, Select All, Find... and Replace... (disabled where they can't do anything, with the shortcuts as hints). A right-click outside the selection first moves the cursor there, one into the selection keeps it. The menu key or Shift+F10 open it at the cursor, as does `openContextMenu()`. In the web build Paste is not in the menu, as the browser only hands out the clipboard with its paste event.

The application can change the menu before it opens, the handler gets the menu (a `PopupMenu::Menu`) and the `CodeContext` of the click:

```cpp
editor.setContextMenuHandler([&](gui::PopupMenu::Menu& menu, const gui::CodeContext& context) {
    auto toUpper = gui::PopupMenu::Action("To Uppercase", [&]() {
        const auto [start, end] = editor.selection();
        editor.replaceRange(start, end, upperCase(editor.selectedText()));
    });
    menu.push_back(gui::PopupMenu::Separator());
    menu.push_back(context.selection.first != context.selection.second ? toUpper : gui::PopupMenu::Disabled(toUpper));
});
```

- The actions run in a later frame, while the menu is open the gui is locked, so the text doesn't change in between. Don't keep the `CodeContext`, its `lineText` points into the text.
- The menu is copied when it opens, its actions usually capture the editor, which must outlive the open menu.

### Links

A link handler makes parts of the text lead somewhere, e.g. the file name of an include to that file:

```cpp
editor.setLinkHandler([&](const gui::CodeContext& context) -> std::optional<gui::CodeLink> {
    // find out if the character at context.offset belongs to a link, if so return its byte range, a label for the
    // context menu (empty for none) and what opening it does
    return gui::CodeLink{start, end, "Open sprites.8o", [&]() { openFile("sprites.8o"); }};
});
```

- While Sys is held, the handler is asked for the character under the mouse, a link there is underlined (in `DEFAULT, TEXT_COLOR_FOCUSED`) and the mouse cursor is a pointing hand. Sys+click opens it, a Sys+click beside a link is a normal click.
- A right-click on a link (or the menu key at it) puts the label of the link at the top of the context menu.
- `open()` of a Sys+click is called at the end of `render()`, it may switch the application to another editor, but must not destroy the one that is calling it.

### Shortcuts of the application

Keys the application uses for itself, but the editor would also act on, can be taken from the editor. While it has the keyboard focus, the handler is asked before the keys of a frame are handled, if it returns `true` the editor ignores them in that frame:

```cpp
editor.setShortcutHandler([&]() {
    // e.g. Back with Cmd+Option+Left, that would move the cursor to the line start otherwise
    return backRequested;
});
```

Do the action after `render()`, not in the handler, when it may switch to another editor.

### Syntax highlighting

A `SyntaxHighlighter` gives each byte of a line a style id, `CodeTextStyle::palette` turns it into a color (id 0 is plain text, an id without a palette entry uses the first color). Lines are passed without their line break.

```cpp
class MyHighlighter : public gui::SyntaxHighlighter
{
public:
    // the state at the start of the next line, here: inside a block comment, called for every line behind an edit
    LineState nextLineState(std::string_view line, LineState state) const override;
    // set the style of each byte of the line, styles has line.size() entries, all 0 at the call
    void highlightLine(std::string_view line, LineState state, std::span<uint8_t> styles) const override;
};
```

- `LineState` is a `uint32_t` the highlighter defines, 0 is the state at the start of the text. The editor keeps the state of every line and updates them from an edited line on, so `nextLineState()` should be cheap; a highlighter without state across lines doesn't need to override it.
- After the rules of a highlighter changed (e.g. another language), `updateHighlighting()` scans the text again.
- `DrawCodeLine(line, position, width, style, highlighter, state)` draws a single highlighted line without an editor, e.g. for a disassembly view.

The showcase has a C-like example highlighter (`DemoHighlighter`) on the Widgets tab.

---

## Popup menus

```cpp
gui::PopupMenu::Menu contextMenu = {
    gui::PopupMenu::Action("Cut",   [&]{ cut(); }, "Cmd+X"),
    gui::PopupMenu::Action("Copy",  [&]{ copy(); }),
    gui::PopupMenu::Separator(),
    gui::PopupMenu::Toggle("Word wrap", &wordWrap),
    gui::PopupMenu::Submenu("Recent", {
        gui::PopupMenu::Filename("/some/path/file.ch8", [&]{ open("/some/path/file.ch8"); }),
    }),
    gui::PopupMenu::Disabled(gui::PopupMenu::Action("Paste", [&]{ paste(); })),
};

if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !gui::PopupMenu::IsOpen())
    gui::PopupMenu::Open(GetMousePosition(), contextMenu);
```

- `Open()` copies the menu, so it may be changed or destroyed while it is open, even from inside one of its actions. `IsMenuOpen(menu)` tells if the open menu was opened from `menu`, it compares the address of `menu`, so it only works for a menu object that lives on.
- For menus built anew in every frame, give them an id: `Open(position, menu, owner)` and `IsOpenedBy(owner)` identify the menu by the owner id (e.g. `GetId("context")`) instead of its address.
- The position is in the coordinates of the current container, like `GetMousePosition()`; inside a popup or window it is moved to where the menu is drawn.
- The menu is updated in `BeginGui()` and drawn in `EndGui()`; while it is open, the rest of the GUI is locked.
- `SetMenuStyle()`/`GetMenuStyle()` change sizes and the shadow; font size and item height follow the raygui `TEXT_SIZE` unless set in the style.

### Menu bar

A menu bar is a row of titles, each with a popup menu:

```cpp
gui::PopupMenu::Bar menuBar = {
    {"File", { gui::PopupMenu::Action("Open...", [&]{ open(); }), gui::PopupMenu::Separator(),
               gui::PopupMenu::Action("Quit", [&]{ quit(); }) }},
    {"View", { gui::PopupMenu::Toggle("Status Bar", &showStatusBar) }},
};
// every frame, e.g. as first row of the gui:
gui::MenuBar("menubar", menuBar);
```

- The bar takes the remaining width and the row height at the current layout position, so it can also share a row with other widgets in `BeginColumns()`.
- The bar may be built anew in every frame (e.g. to enable items or to change labels like "Undo Paint"), it identifies its menus by its id and their index. The items of an open menu are the ones of the frame it was opened in.
- Clicking a title opens its menu below it, clicking it again closes it. While one of its menus is open, hovering another title switches to that menu.
- The bar is drawn inside the window, there is no native (e.g. macOS) menu bar.
- The shortcut texts of items are only shown, the application has to handle the keys itself. `ShortcutText(key, modifiers)` writes them like the platform does: `ShortcutText("G", SHORTCUT_SYS | SHORTCUT_SHIFT)` is "⇧⌘G" on macOS (and in the web build on a Mac, see `IsMacLike()`) and "Ctrl+Shift+G" elsewhere. The default font has the symbols ⌃ ⌥ ⇧ ⌘ and the arrows. `SetMacLike(true/false)` overrides the platform detection, for the shortcut texts and the system key (Cmd or Ctrl).

---

## File dialog

`#include <rlguipp/rlguipp_filedialog.hpp>`

```cpp
gui::FileDialogInfo fdi;
fdi.type = gui::FileDialogInfo::SelectFolder;
fdi.title = "Select a folder";
fdi.path = startPath;
// ...
if (dialogOpen && gui::ModalFileDialog(fdi, &dialogOpen)) {
    if (fdi.action == gui::FileDialogInfo::Okay)
        use(fdi.path);   // or fdi.selection / fdi.filename, depending on the type
}
```

`SetDefaultFileIcons()` sets the extension to icon map used for new dialogs. The dialog's window is identified by `fdi.title`.

---

## File tree

`#include <rlguipp/rlguipp_filetree.hpp>`

`FileTree` shows the folders and files below a root folder, folders first, then sorted by name without case, like the file dialog lists them. Hidden files (a name starting with a dot) are not shown.

```cpp
gui::FileTree tree{"project-files"};
tree.setRoot(projectFolder);
tree.setFilter({".8o", ".txt"});  // folders are always shown, an empty list shows all files
tree.setMarkHandler([&](const std::string& path, bool isDirectory) {
    return path == mainFile ? gui::FileTreeMark{-1, GOLD, "(main)"} : gui::FileTreeMark{};
});
tree.setContextMenuHandler([&](gui::PopupMenu::Menu& menu, const std::string& path, bool isDirectory) {
    menu.push_back(gui::PopupMenu::Action("Refresh", [&]() { tree.refresh(); }));
});
// ...
if (auto file = tree.render(height))   // the file the user picked in this frame
    open(*file);
```

- A click on a folder opens or closes it, a click on a file selects it and `render()` returns it. A click gives the tree the keyboard focus: Up/Down/Home/End/Page Up/Page Down move the selection, Right opens a folder (or goes to its first entry), Left closes it (or goes to the folder of the entry), Enter picks a file.
- Folders are read when they are opened the first time and kept, `refresh()` reads the opened folders again (folders that are gone disappear). The tree doesn't watch the disk itself, the application calls `refresh()`, e.g. when its window gets the focus.
- `reveal(path)` opens the folders down to a path, selects it and scrolls it into view. `select()`, `isExpanded()` and `setExpanded()` change the state without that.
- The mark handler is asked in every frame for each shown entry: another icon, a text color (for an unselected entry) and a dimmer suffix after the name. The context menu handler gets the entry of a right-click (it is selected first) or the root folder for a click below the entries; the menu starts empty and is only shown with items.
- Paths are kept as given (relative or absolute), normalized and without a separator at the end, `selected()` and `render()` return them that way.
- In a [drawer](#begindrawer--enddrawer) the tree is a pop-over file list, `render()` with a height of 0 fills it.

---

## Styles and the raygui DEFAULT propagation trap

rlguipp uses raygui's style system directly (`GuiSetStyle`/`GuiGetStyle`, also as `gui::SetStyle`/`gui::GetStyle`). The `StyleManager` used by the examples and by Cadmium is not part of the library.

> **This is the most important raygui gotcha.** Ignoring it causes styles to reset unexpectedly.

raygui's `GuiSetStyle` copies every write to a `DEFAULT` base property to all controls:

```c
if ((control == DEFAULT) && (property < NUM_PROPS_BASE)) {
    for (int i = 1; i < MAX_CONTROLS; i++)
        guiStyle[i * stride + property] = value;
}
```

**So any write to a DEFAULT base property overwrites the same property of every control (BUTTON, TEXTBOX, STATUSBAR, …).** Per-control values set earlier are silently lost — also when a temporary style change (like `StyleManager::Scope`) restores a DEFAULT property.

If a widget needs a value that differs from the DEFAULT one, derive it in the widget's drawing code (like `BeginTab` does for inactive labels) instead of relying on a persistent per-control style.

---

## Common pitfalls

1. **All columns but the last are invisible**: a `BeginColumns` child without a width takes all the remaining width. Give each column but the last one a width.
2. **Columns overflow**: the same fraction for all N columns plus spacing is more than 100 %. Leave the last one without a width.
3. **`SetNextWidth` only applies to the next widget**: use `SetDefaultWidth` for several widgets of the same width.
4. **A dropdown reports its selection one frame later**: it is drawn in `EndGui()`.
5. **Widgets with the same label share their state**: two `Spinner`s, `ValueBox`es or `DropdownBox`es with the same label (also an empty one), or two `TextBox`es with the same id, in the same id scope have the same id, so focusing one puts both into edit mode. Use `"##"` labels or `PushId()` per row or object, see [Ids and widget state](#ids-and-widget-state).
6. **A changing window title or tab label changes the id**: the window is then a new one at its initial position, the widgets inside get new ids. Use `"Title###id"` for titles that change.

---

## Tests

The showcase (`examples/showcase.cpp`) doubles as a golden image test. In this mode it renders a fixed list of tabs, themes, menus and dialogs in a hidden window and compares each frame pixel by pixel with the approved PNGs in `examples/golden/`:

```
rlguipp_showcase --golden <approved-dir> --out <output-dir> [--tolerance <n>] [--update]
```

- The CTest test `rlguipp-golden-images` (label `gui`) runs it, e.g. `ctest --test-dir build/debug -R rlguipp-golden`. It needs a display, as raylib opens a (hidden) window.
- `--tolerance <n>` lets every channel of a pixel differ by up to n (default 0, exact). The CTest test uses 2, so the images also pass with software OpenGL (Mesa llvmpipe), which renders gradients and blending one or two steps off.
- Golden mode calls `SetMacLike(true)`, so the menus show the macOS shortcut texts on every platform.
- CI (`.github/workflows/ci.yml`) builds on Linux, macOS and Windows; only Linux runs the tests, in a virtual display (`xvfb-run`) with Mesa's software OpenGL.
- On a mismatch the actual image and a `<name>-diff.png` (differing pixels red) are written to the output directory, which is `<build>/examples/golden-out` for the CTest test.
- After an intended visual change, look at the new images and run with `--update` to replace the approved ones:
  ```
  build/debug/examples/rlguipp_showcase --golden examples/golden --out /tmp/golden-out --update
  ```
- To keep the images stable, golden mode fixes the time (no animation, a fixed FPS text), moves the mouse out of the window and shows the file dialog and the file tree on a generated `fixture` directory with fixed file sizes and times.
- The images are pixel exact for one renderer. A different GPU driver or raylib version can change single pixels beyond the tolerance, then check the diff images and update.
- New cases go into the list in `ShowcaseApp::runGoldenTests()`.

The text widgets have an input test, `test/textedit_test.cpp` (CTest `rlguipp-textedit`, label `gui`). It draws a `TextBox`, a `TextBox` with a char buffer and a `TextBoxMulti` in a hidden window and plays key, mouse button, mouse position and wheel input with raylib's `PlayAutomationEvent()`, so modifier keys work like real ones. Automation events can't type characters, text is entered by pasting from a test clipboard handler, the system clipboard is never touched.
The code editor has an input test of the same kind, `test/codeeditor_test.cpp` (CTest `rlguipp-codeeditor`, label `gui`), with a test highlighter that records the line states the editor passes. On the desktop it also types characters, by calling the GLFW char callback raylib registered.
The file tree and the drawer are tested the same way in `test/filetree_test.cpp` (CTest `rlguipp-filetree`, label `gui`), on a folder it creates in the temp directory.
- To test the error recovery of release builds in a debug build, compile rlguipp with `RLGUIPP_ASSERT` defined as something that doesn't abort (see [API conventions](#api-conventions)).

---

## Local raygui changes

`raygui4.5.h` (raygui v4.5-dev) is vendored **with local changes**, kept as small as possible so raygui can be upgraded from upstream. Every change is marked with a `NOTE: rlguipp change` comment, so `grep -n "rlguipp change" raygui4.5.h` lists them. When upgrading raygui, re-apply them (unless upstream fixed the issue) and run the checks below.

### 1. Clipped drawing

**Problem:** scroll panels, table cells and `BeginClipping()` need raygui's own drawing to be clipped, also inside render textures and nested clip areas.

**Local change:** `GuiDrawText()` draws its glyphs with `DrawTextCodepointClipped()` and `GuiDrawRectangle()` draws with `DrawRectangleClipped()`. These functions (and `DrawTextClipped()`, `DrawPanelClipped()`) are declared in the raygui header and implemented in `rlguipp.cpp`, where they clip against the top of rlguipp's clipping stack.

### 2. `GuiTextBox` reads all typed characters of a frame

**Problem:** raylib keeps typed characters only until the next frame (the character queue is reset in `PollInputEvents`, at most `MAX_CHAR_PRESSED_QUEUE` = 16 per frame). Upstream raygui calls `GetCharPressed()` once per frame, so every further character typed in the same frame is lost. Fast typing, or a slow frame, drops letters.

**Local change:** the insert part of `GuiTextBox` runs in a loop until `GetCharPressed()` returns 0.

**Keep in mind:** in v4.5 the delete/backspace code further down uses `codepointSize`, the size of the character *typed* in this frame. Without a typed character the original code still set it to 1 via `CodepointToUTF8(0, ...)`, so the local change computes it once before the loop as well. Otherwise backspace would stop deleting.

**Upstream status** (checked 2026-10-18 against raygui v5.1-dev, raygui commit `ca17a68d`, the copy in raylib's `examples/text/raygui.h` is identical): not fixed, `GuiTextBox` still reads a single codepoint per frame (`int codepoint = GUI_INPUT_KEY;`). The change has to be re-applied after upgrading. In v5.x the input is read through the `GUI_INPUT_KEY` macro and the insert is the `else if` branch after the new paste handling, so the loop has to wrap that part.

**Not changed:** `GuiValueBox` and `GuiValueBoxFloat` read one character per frame the same way. They are only used for short numeric input.

**Since rlguipp has its own `TextBox`** (see [Text editing](#text-editing)), this change only matters for code calling `GuiTextBox` directly, nothing in rlguipp or Cadmium does.

### 3. rlguipp relies on raygui internals

`rlguipp.cpp` compiles the raygui implementation and uses some of its internal state (`guiAlpha`, `guiLocked`, `guiState`, `guiControlExclusiveMode`) and static helpers (`GetTextWidth`, `GetTextBounds`, `GuiTooltip`, `GuiDrawRectangle`, `GuiScrollBar`). Check these still exist after an upgrade.

### 4. Upstream changes to look at when upgrading (v4.5-dev → v5.1-dev)

- **Delete/backspace with UTF-8 fixed upstream:** v4.5 subtracts `codepointSize` (the typed character's size) instead of the size of the deleted character, which is wrong for multi-byte UTF-8 characters. v5.1 uses the size of the deleted codepoint and adds word deletion with Ctrl+Delete / Ctrl+Backspace. After upgrading, the "keep `codepointSize` at 1" part of change 2 is not needed anymore.
- **New paste in `GuiTextBox`:** v5.1 pastes with Ctrl+V (Ctrl only, not Cmd on macOS). The loop that moves the existing text to make room uses the condition `(i > textBoxCursorIndex) && (i < pasteLength)`, which looks wrong: pasting in the middle of a text seems to overwrite the text after the cursor. Test it before relying on it.
- **Input macros:** v5.1 reads all input through macros (`GUI_INPUT_KEY`, `GUI_KEY_PRESSED`, `GUI_KEY_DOWN`, ...) that can be redefined before including raygui.

### 5. Checks after an upgrade

1. `grep -n "rlguipp change"` in the old and the new `raygui.h`, re-apply what upstream did not fix.
2. `ctest -L gui` (golden images and the text widget test), the text widgets use raygui internals (`GetTextBounds`, `GuiDrawRectangle`, `GuiScrollBar`, `guiLocked`, `guiControlExclusiveMode`).
3. Scroll a scroll panel and a table view: text and frames of raygui widgets must be clipped at the panel border.
