//---------------------------------------------------------------------------------------
// Automated test of the rlguipp menu bar
//
// The bar is drawn into a render texture of a hidden window, the input is played with
// raylib's automation events. The application builds its bar anew in every frame (the
// menus are at another address each frame), which the bar has to handle.
//---------------------------------------------------------------------------------------

#include <rlguipp/rlguipp.hpp>

#include <cstdio>
#include <string>

namespace {

// raylib's AutomationEventType values, the enum is only declared in rcore.c
constexpr unsigned int AUTOMATION_MOUSE_BUTTON_UP = 5;
constexpr unsigned int AUTOMATION_MOUSE_BUTTON_DOWN = 6;
constexpr unsigned int AUTOMATION_MOUSE_POSITION = 7;
constexpr float ROW_HEIGHT = 20;

int g_failures = 0;
int g_checks = 0;

#define CHECK_EQ(actual, expected)                                                                                         \
    do {                                                                                                                   \
        ++g_checks;                                                                                                        \
        const auto actualValue = (actual);                                                                                 \
        const auto expectedValue = (expected);                                                                             \
        if (!(actualValue == expectedValue)) {                                                                             \
            ++g_failures;                                                                                                  \
            std::printf("FAILED line %d: %s is %d, expected %d\n", __LINE__, #actual, int(actualValue), int(expectedValue)); \
        }                                                                                                                  \
    } while (false)

void play(unsigned int type, int param0 = 0, int param1 = 0)
{
    AutomationEvent event{};
    event.type = type;
    event.params[0] = param0;
    event.params[1] = param1;
    PlayAutomationEvent(event);
}

class Harness
{
public:
    Harness()
    {
        _target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    }
    ~Harness() { UnloadRenderTexture(_target); }

    void frame()
    {
        // a new bar every frame, alternating between two buffers so the menus never keep their address
        auto& bar = _bars[_frame++ & 1];
        bar = {{"File", {gui::PopupMenu::Action("Open", [this]() { ++_opened; })}},
               {"Edit", {gui::PopupMenu::Action("Undo", []() {}), gui::PopupMenu::Action("Redo", []() {})}},
               {"View", {gui::PopupMenu::Action("Zoom", []() {})}}};
        BeginTextureMode(_target);
        ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
        gui::BeginGui({}, &_target);
        gui::SetRowHeight(ROW_HEIGHT);
        gui::MenuBar("menu", bar);
        gui::EndGui();
        EndTextureMode();
        Image image = LoadImageFromTexture(_target.texture);
        ImageFlipVertical(&image);
        for (int i = 0; i < 3; ++i) {
            auto r = titleRect(i);
            _highlighted[i] = ColorToInt(GetImageColor(image, int(r.x) + 2, int(r.y + r.height / 2))) == GuiGetStyle(DEFAULT, BASE_COLOR_FOCUSED);
        }
        UnloadImage(image);
        BeginDrawing();  // polls the input
        EndDrawing();
    }

    Rectangle titleRect(int index) const
    {
        static const char* titles[] = {"File", "Edit", "View"};
        float x = 0;
        for (int i = 0; i < index; ++i)
            x += gui::GetTextSize(titles[i]).x + 12;
        return {x, 1, gui::GetTextSize(titles[index]).x + 12, ROW_HEIGHT - 2};
    }

    Vector2 titleCenter(int index) const
    {
        auto r = titleRect(index);
        return {r.x + r.width / 2, r.y + r.height / 2};
    }

    // the first item of the menu of a title
    Vector2 itemCenter(int index) const
    {
        auto r = titleRect(index);
        return {r.x + 20, r.y + r.height + 8};
    }

    void moveTo(Vector2 p)
    {
        play(AUTOMATION_MOUSE_POSITION, int(p.x), int(p.y));
        frame();
    }

    void click(Vector2 p)
    {
        play(AUTOMATION_MOUSE_POSITION, int(p.x), int(p.y));
        frame();
        play(AUTOMATION_MOUSE_BUTTON_DOWN, MOUSE_BUTTON_LEFT);
        frame();
        play(AUTOMATION_MOUSE_BUTTON_UP, MOUSE_BUTTON_LEFT);
        frame();
    }

    // the number of the following frames in which the title was highlighted
    int highlightedFrames(int index, int frames)
    {
        int count = 0;
        for (int i = 0; i < frames; ++i) {
            frame();
            count += _highlighted[index] ? 1 : 0;
        }
        return count;
    }

    int opened() const { return _opened; }

private:
    RenderTexture2D _target{};
    gui::PopupMenu::Bar _bars[2];
    int _frame{0};
    bool _highlighted[3]{};
    int _opened{0};
};

void testOpenMenuStaysHighlighted(Harness& h)
{
    h.moveTo({300, 200});
    CHECK_EQ(h.highlightedFrames(0, 4), 0);
    h.click(h.titleCenter(0));
    CHECK_EQ(gui::PopupMenu::IsOpen(), true);
    // the open menu's title is highlighted in every frame, also while the mouse is in the menu, the others never
    CHECK_EQ(h.highlightedFrames(0, 8), 8);
    h.moveTo(h.itemCenter(0));
    CHECK_EQ(h.highlightedFrames(0, 8), 8);
    CHECK_EQ(h.highlightedFrames(1, 8), 0);
    // hovering another title switches to its menu at once
    h.moveTo(h.titleCenter(1));
    h.moveTo(h.itemCenter(1));
    CHECK_EQ(h.highlightedFrames(1, 8), 8);
    CHECK_EQ(h.highlightedFrames(0, 8), 0);
    CHECK_EQ(gui::PopupMenu::IsOpen(), true);
    h.moveTo(h.titleCenter(1));
    // a click on the title of the open menu closes it
    h.click(h.titleCenter(1));
    CHECK_EQ(gui::PopupMenu::IsOpen(), false);
    h.moveTo({300, 200});
    CHECK_EQ(h.highlightedFrames(1, 4), 0);
}

void testActionOfARebuiltMenu(Harness& h)
{
    h.click(h.titleCenter(0));
    CHECK_EQ(gui::PopupMenu::IsOpen(), true);
    // the first item is right below the title
    h.click(h.itemCenter(0));
    CHECK_EQ(h.opened(), 1);
    CHECK_EQ(gui::PopupMenu::IsOpen(), false);
}

}  // namespace

int main()
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(400, 300, "rlguipp menu bar test");
    SetTargetFPS(0);
    {
        Harness harness;
        testOpenMenuStaysHighlighted(harness);
        testActionOfARebuiltMenu(harness);
    }
    gui::UnloadGui();
    CloseWindow();
    std::printf("%d of %d checks failed\n", g_failures, g_checks);
    return g_failures ? 1 : 0;
}
