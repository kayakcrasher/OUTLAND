#include "outland/game/HomeScreen.hpp"
#include "outland/input/PointerEvents.hpp"

#include <raylib.h>

namespace outland::game {

namespace {

#ifdef OUTLAND_DEV_TOOLS
constexpr const char* dev_label="CREATIVE BUILDER (DEV)";
#else
constexpr const char* dev_label="DEV LAB";
#endif
struct MenuButton {
    Rectangle bounds;
    const char* label;
    GameMode mode;
};

MenuButton make_button(
    const int screen_width,
    const float y,
    const char* label,
    const GameMode mode
) {
    constexpr float width = 360.0F;
    constexpr float height = 58.0F;

    return {
        {
            static_cast<float>(screen_width) *
                    0.5F -
                width * 0.5F,
            y,
            width,
            height
        },
        label,
        mode
    };
}

Rectangle bot_toggle_bounds(
    const int screen_width,
    const float y
) {
    constexpr float width = 220.0F;
    return {static_cast<float>(screen_width) * 0.5F + 190.0F, y + 9.0F, width, 40.0F};
}

const char* bot_toggle_label(
    const ai::BotLevel level
) {
    switch (level) {
        case ai::BotLevel::Easy: return "BOTS: EASY";
        case ai::BotLevel::Hard: return "BOTS: HARD";
        default: return "BOTS: MEDIUM";
    }
}

void draw_button(
    const MenuButton& button
) {
    const Vector2 pointer =
        GetMousePosition();

    const bool hovered =
        CheckCollisionPointRec(
            pointer,
            button.bounds
        );

    const Color fill =
        hovered
            ? Color{80, 100, 72, 240}
            : Color{38, 45, 38, 230};

    DrawRectangleRounded(
        button.bounds,
        0.18F,
        8,
        fill
    );

    DrawRectangleRoundedLines(
        button.bounds,
        0.18F,
        8,
        WHITE
    );

    const int font_size = 24;

    const int text_width =
        MeasureText(
            button.label,
            font_size
        );

    DrawText(
        button.label,
        static_cast<int>(
            button.bounds.x +
            button.bounds.width * 0.5F -
            static_cast<float>(text_width) * 0.5F
        ),
        static_cast<int>(
            button.bounds.y + 16.0F
        ),
        font_size,
        RAYWHITE
    );
}

}

GameMode HomeScreen::update(
    const int screen_width,
    const int screen_height
) {
    (void)screen_height;

    const float start_y = 270.0F;
    const float spacing = 70.0F;

    const MenuButton buttons[] = {
        make_button(
            screen_width,
            start_y,
            "BATTLE ROYALE",
            GameMode::BattleRoyale
        ),
        make_button(
            screen_width,
            start_y + spacing,
            "ZOMBIE SURVIVAL",
            GameMode::ZombieSurvival
        ),
        make_button(
            screen_width,
            start_y + spacing * 2.0F,
            "EXPLORE VERDA",
            GameMode::Explore
        ),
        make_button(
            screen_width,
            start_y + spacing * 3.0F,
            dev_label,
            GameMode::DevLab
        )
    };

    if (
        !input::PointerEvents::presses().empty() || IsMouseButtonPressed(
            MOUSE_BUTTON_LEFT
        )
    ) {
        const auto buffered=input::PointerEvents::presses();
        const Vector2 pointer =buffered.empty()?GetMousePosition():buffered.front();

        if (CheckCollisionPointRec(pointer, bot_toggle_bounds(screen_width, start_y))) {
            bot_level_ = bot_level_ == ai::BotLevel::Easy ? ai::BotLevel::Medium :
                bot_level_ == ai::BotLevel::Medium ? ai::BotLevel::Hard : ai::BotLevel::Easy;
            return GameMode::Home;
        }

        for (
            const MenuButton& button :
            buttons
        ) {
            if (
                CheckCollisionPointRec(
                    pointer,
                    button.bounds
                )
            ) {
                selected_ =
                    button.mode;

                return selected_;
            }
        }
    }

    return GameMode::Home;
}

void HomeScreen::draw(
    const int screen_width,
    const int screen_height
) const {
    ClearBackground(
        Color{
            24,
            31,
            25,
            255
        }
    );

    DrawRectangleGradientV(
        0,
        0,
        screen_width,
        screen_height,
        Color{
            52,
            73,
            56,
            255
        },
        Color{
            17,
            20,
            17,
            255
        }
    );

    const char* title =
        "OUTLAND";

    const int title_size =
        72;

    const int title_width =
        MeasureText(
            title,
            title_size
        );

    DrawText(
        title,
        screen_width / 2 -
            title_width / 2,
        90,
        title_size,
        RAYWHITE
    );

    const char* subtitle =
        "WELCOME TO VERDA";

    const int subtitle_size =
        22;

    const int subtitle_width =
        MeasureText(
            subtitle,
            subtitle_size
        );

    DrawText(
        subtitle,
        screen_width / 2 -
            subtitle_width / 2,
        180,
        subtitle_size,
        Color{
            190,
            205,
            175,
            255
        }
    );

    constexpr float start_y =
        270.0F;

    constexpr float spacing =
        70.0F;

    const MenuButton buttons[] = {
        make_button(
            screen_width,
            start_y,
            "BATTLE ROYALE",
            GameMode::BattleRoyale
        ),
        make_button(
            screen_width,
            start_y + spacing,
            "ZOMBIE SURVIVAL",
            GameMode::ZombieSurvival
        ),
        make_button(
            screen_width,
            start_y + spacing * 2.0F,
            "EXPLORE VERDA",
            GameMode::Explore
        ),
        make_button(
            screen_width,
            start_y + spacing * 3.0F,
            dev_label,
            GameMode::DevLab
        )
    };

    for (
        const MenuButton& button :
        buttons
    ) {
        draw_button(button);
    }

    // Battle Royale difficulty: changes how bots think and shoot, never their health or senses.
    const Rectangle toggle = bot_toggle_bounds(screen_width, start_y);
    const bool toggle_hovered = CheckCollisionPointRec(GetMousePosition(), toggle);
    DrawRectangleRounded(toggle, 0.25F, 8, toggle_hovered ? Color{110, 84, 52, 240} : Color{58, 46, 34, 230});
    DrawRectangleRoundedLines(toggle, 0.25F, 8, Color{230, 200, 150, 255});
    const char* toggle_label = bot_toggle_label(bot_level_);
    DrawText(toggle_label,
        static_cast<int>(toggle.x + toggle.width * 0.5F - static_cast<float>(MeasureText(toggle_label, 20)) * 0.5F),
        static_cast<int>(toggle.y + 10.0F), 20, Color{245, 225, 190, 255});

    DrawText(
        "VERDA DEVELOPMENT BUILD",
        18,
        screen_height - 34,
        16,
        GRAY
    );
}

} // namespace outland::game
