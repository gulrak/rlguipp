# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Up to 0.3.0 rlguipp was developed inside [Cadmium](https://github.com/gulrak/cadmium) (`external/rlguipp`).
Its history was extracted from there, every commit of that time links to its Cadmium commit. The versions
before 0.3.0 are the values of `RLGUIPP_VERSION` in Cadmium, not releases.

## [Unreleased]

## [0.5.0] - 2026-11-22

### Added

- `PopupMenu::Open(position, menu, owner)` and `PopupMenu::IsOpenedBy(owner)` identify a menu by an owner id
  (e.g. from `GetId()`) instead of the address of the `Menu` object, for menus that are built anew every frame
- Test `rlguipp-menubar`

### Fixed

- A `MenuBar` that is built anew every frame (e.g. to enable items or to change labels like "Undo Paint")
  lost track of its open menu and its titles flickered. The bar now identifies its menus by its id and their
  index, menus opened without an owner are still matched by address.
- The CMake project version (0.1.0 in 0.3.0) and the README's FetchContent example now match
  `RLGUIPP_VERSION`

## [0.3.0] - 2026-11-22

The first standalone version. The version number was set on 2026-10-21 with the API refactoring, the
extraction from Cadmium followed on 2026-11-22.

### Added

- Standalone CMake project: options `RLGUIPP_BUILD_EXAMPLES`, `RLGUIPP_BUILD_TESTS`, `RLGUIPP_BUILD_TOOLS`
  (on only when built as top level project) and `RLGUIPP_GHC_FS_IMPL`, the alias target `rlguipp::rlguipp`,
  `cmake/Dependencies.cmake` fetches raylib 6.0, ghc::filesystem 1.5.14 and (examples and tests only)
  fmt 11.2.0 as release archives, a parent project can pin other versions
- MIT license file and the usage guide `README.md`
- CI on GitHub Actions: builds on Linux (GCC), macOS (Clang) and Windows (MinGW), Linux also runs all tests
  in a virtual display (`xvfb-run`) with Mesa's software OpenGL
- Ids and id scopes, similar to Dear ImGui: widget state (popups, window boxes, table views, dropdowns, the
  keyboard focus) is kept inside rlguipp by id and dropped when an id is not used for a frame; `GetId()`,
  `PushId()`/`PopId()` (string, int or pointer), labels with a hidden part (`"Text##id"`, `"Text###id"`)
- API misuse (unbalanced `Begin*`/`End*` or `PushId`/`PopId`, widgets outside of `BeginGui`/`EndGui`)
  asserts in debug builds, release builds log it once and repair the state; `RLGUIPP_ASSERT` replaces the assert
- Layout: `SetNextHeight()`, `SetNextRelativeWidth()`, `SetNextRelativeHeight()`, the sticky
  `SetDefaultWidth()`, `BeginClipping()`/`EndClipping()`
- Custom widgets: `AllocateWidget()`, `GetAlpha()`, `GetTextSize()`
- Containers: `BeginScrollView()`/`EndScrollView()` (borderless, measures its content and shows a scroll bar
  only when needed), `BeginDrawer()`/`EndDrawer()` (a popup that follows an area and closes on Escape or a
  click outside), table columns given once as `TableColumn::Fixed(px)` or `TableColumn::Stretch(weight)`
- Widgets: `ButtonMulti()` (returns the mouse button), `SimpleListView()`, `LabelWrapped()`,
  `ColorFromHsv()`/`HsvFromColor()`, `IsLastWidgetEditFinished()` for the end of an edit
- Text editing in `TextBox` and `TextBoxMulti` (implemented by rlguipp instead of raygui's `GuiTextBox`)
  with selection, clipboard, undo, word and line movement, double and triple click, read-only mode, and
  wrapping and scrolling in `TextBoxMulti`; `SetClipboardHandler()` (e.g. for the web), `CopyToClipboard()`,
  `PasteFromClipboard()`, `IsPasteRequested()`
- Keyboard shortcuts: `IsShortcutPressed()` uses the letter the key shows in the current keyboard layout,
  `ShortcutText()` writes "⇧⌘G" on macOS and "Ctrl+Shift+G" elsewhere, `IsMacLike()` (also true in the web
  build on a Mac) and `SetMacLike()` to override it
- Popup menus: `PopupMenu::Open()`, `Close()`, `IsOpen()`, `IsMenuOpen()`, `SetMenuStyle()`/`GetMenuStyle()`,
  submenus and display-only shortcut texts; `MenuBar(id, PopupMenu::Bar{...})` built on them
- Code editor (`rlguipp_codeeditor.hpp`): `CodeEditor` with undo, find and replace, a context menu hook, a
  link hook (e.g. to open included files) and an application shortcut handler; `SyntaxHighlighter` with
  per-byte styles and a state carried from line to line, `CodeTextStyle`, `DrawCodeLine()`
- File tree (`rlguipp_filetree.hpp`): `FileTree` with lazily read folders, marks (icon, color, suffix), a
  context menu hook and keyboard navigation
- File dialog (`rlguipp_filedialog.hpp`): `ModalFileDialog()` with `FileDialogInfo` for open, save and
  folder selection, `SetDefaultFileIcons()`/`GetDefaultFileIcons()`
- LED displays (`rlguipp_leds.hpp`): `SevenSegmentDisplay()` and `Til311Display()`
- Optional application base class `gui::Application` (`rlguiapp.hpp`): window, render texture with an
  integer pixel scale and the `BeginGui`/`EndGui` frame around `drawGui()`
- Own bitmap font (`rlguipp_font.hpp`) with 309 glyphs: ASCII, Latin-1, arrows, ⌃ ⌥ ⇧ ⌘ and some more
  symbols; `tools/fontgenerator` creates it from `tools/font.txt` and rejects malformed glyphs with errors
- `getUserDirectories()` (`syshelper.hpp`): the home, documents and downloads folders of the platform
- The showcase (`examples/showcase.cpp`) demonstrates all features and doubles as golden image test
  (`--golden`, `--update`, `--tolerance`, 22 approved images); input tests `rlguipp-textedit`,
  `rlguipp-codeeditor` and `rlguipp-filetree` play raylib automation events in a hidden window

### Changed

- raygui 3.2 was replaced by raygui 4.5-dev (upstream commit 1f81371) with four small local changes marked
  `NOTE: rlguipp change`, see "Local raygui changes" in the README
- rlguipp is a compiled C++20 library now instead of a single header, with separate headers for the file
  dialog, the LED displays, the code editor, the file tree, the font and the application class; it is
  C++ only and needs ghc::filesystem
- Value widgets take a pointer to their value and return `true` when the user changed it, e.g.
  `CheckBox(text, &checked)` and `Slider(label, &value, min, max)` instead of returning the new value
- Widgets without a label (`TextBox`, `TextBoxMulti`, table views, popups) take an id string, popups, window
  boxes and table views keep their state by id instead of by the address of the value pointer
- The keyboard focus is an id: `SetKeyboardFocus(GetId("name"))` and `HasKeyboardFocus(id)` instead of a
  `void*` key
- A `Begin*()` that returns `void` must always be closed, one that returns `bool` only when it returned
  `true`; `WBF_CLOSABLE` now really controls the close button of `BeginWindowBox()`
- `SetNextWidth()` takes pixels only, fractions moved to `SetNextRelativeWidth()`
- `BeginTableView()` gets its columns once (`TableColumn`), rows call `TableNextColumn()` without a width
- `DropdownBox()` reports a changed selection one frame late (the open list is drawn at the end of the frame)
- `Space()` with a negative value inserts nothing instead of the default spacing
- Popups are drawn ordered by their level

### Removed

- `BeginMenuBar()`/`EndMenuBar()`, `BeginMenu()`/`EndMenu()`, `MenuItem()`, `BeginPopupMenu()`/
  `EndPopupMenu()`, replaced by `PopupMenu` and `MenuBar()`
- `MessageBox()` and `TextInputBox()`
- `rlunicode.h` (UTF-8 handling uses `ghc/utf8.hpp`) and the raygui 3 header

## [0.2.0] - 2023-03-19

### Added

- Tab views: `BeginTabView(&activeTab)`/`EndTabView()` with `BeginTab(text)`/`EndTab()`
- Keyboard focus: `SetKeyboardFocus()` and `HasKeyboardFocus()`
- `IsSysKeyDown()`: Cmd on macOS, Ctrl elsewhere
- `BeginGui()` takes a mouse offset and a GUI scale, for a GUI drawn into a render texture that is shown
  scaled or moved

## [0.1.0] - 2022-09-30

The first version, part of Cadmium's first public commit: a single header `rlguipp.hpp` on top of raygui 3.2.

### Added

- Frame structure with `BeginGui()`/`EndGui()`, also into a render texture
- Layout containers: `Begin()`/`End()`, `BeginColumns()`/`EndColumns()`, `BeginPanel()`, `BeginGroupBox()`,
  `BeginScrollPanel()`, table views, popups and window boxes
- Layout control: `SetIndent()`, `SetReserve()`, `SetNextWidth()`, `SetRowHeight()`, `SetSpacing()`,
  `Space()`, `Separator()`, `GetCurrentPos()`, `GetContentAvailable()`, `GetLastWidgetRect()`
- raygui controls as layout-aware widgets: labels, buttons, toggles, check boxes, combo and dropdown boxes,
  spinners, value and text boxes, sliders, progress bars, list views, color picker and bars, grid, status bar
- Menu bar and popup menus, message and text input boxes, tooltips, `IconText()` with the 256 icons of
  `icons.h`

[Unreleased]: https://github.com/gulrak/rlguipp/compare/v0.5.0...main
[0.5.0]: https://github.com/gulrak/rlguipp/compare/b3bf34c...v0.5.0
[0.3.0]: https://github.com/gulrak/rlguipp/compare/316097f...b3bf34c
[0.2.0]: https://github.com/gulrak/rlguipp/compare/5586011...316097f
[0.1.0]: https://github.com/gulrak/rlguipp/commit/5586011
