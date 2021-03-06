// Copyright (C) 2015 Krzysztof Stachowiak
// For the license (GPL2) details see the LICENSE file

#include <cmath>
#include <iostream>
#include <sstream>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "dick.h"

const int SCREEN_W = 640;
const int SCREEN_H = 480;
const std::string IMAGE_NAME = "db.png";

struct DemoState : public dick::StateNode, std::enable_shared_from_this<dick::StateNode> {

    dick::Key m_last_key;
    dick::Button m_last_button;
    dick::DimScreen m_cursor;
    double m_rotation;

    double m_red, m_green, m_blue;

    bool m_ask_to_quit;

    dick::Resources m_resources;
    std::shared_ptr<dick::InputState> m_input_state;
    dick::GUI m_gui;
    std::unique_ptr<dick::GUI::WidgetContainer> m_status_rail;
    std::unique_ptr<dick::GUI::WidgetContainer> m_menu_rail;
    std::unique_ptr<dick::GUI::Widget> m_yes_no;
    ALLEGRO_BITMAP *m_bitmap;

    DemoState(dick::Resources *global_resources) :
        m_last_key {},
        m_last_button {},
        m_cursor { -1.0, -1.0 },
        m_rotation { 0.0 },
        m_red { 0.5 }, m_green { 0.5 }, m_blue { 0.5 },
        m_ask_to_quit { false },
        m_resources { "", global_resources },
        m_input_state { new dick::InputState },
        m_gui { m_input_state, m_resources },
        m_bitmap { static_cast<ALLEGRO_BITMAP*>(m_resources.get_image(IMAGE_NAME)) }
    {
        m_yes_no = m_gui.make_dialog_yes_no(
            "Quit?",
            [this](){ t_transition_required = true; },
            [this](){ m_ask_to_quit = false; });
        m_yes_no->align(
            { SCREEN_W / 2, 2 * SCREEN_H / 3 },
            dick::GUI::Alignment::MIDDLE | dick::GUI::Alignment::CENTER);
        m_yes_no->set_instance_name("yes-no-dialog");

        m_menu_rail = m_gui.make_container_rail(
                dick::GUI::Direction::LEFT,
                75,
                { SCREEN_W - 3, 3 });
        m_menu_rail->insert(m_gui.make_button_image(
                    m_resources.get_image("db2.png"),
                    [this](){ exit(3); }),
                    dick::GUI::Alignment::TOP | dick::GUI::Alignment::RIGHT);
        m_menu_rail->insert(m_gui.make_button(
                    m_gui.make_image(m_resources.get_image("x.png")),
                    [this](){ m_ask_to_quit = true; }),
                    dick::GUI::Alignment::TOP | dick::GUI::Alignment::RIGHT);
        m_menu_rail->insert(m_gui.make_button(
                    m_gui.make_label("Blue"),
                    [this](){ m_red = 0.333; m_green = 0.5; m_blue = 0.667; }),
                    dick::GUI::Alignment::TOP | dick::GUI::Alignment::RIGHT);
        m_menu_rail->insert(m_gui.make_button(
                    m_gui.make_label("Green"),
                    [this](){ m_red = 0.5; m_green = 0.667; m_blue = 0.333; }),
                    dick::GUI::Alignment::TOP | dick::GUI::Alignment::RIGHT);
        m_menu_rail->insert(m_gui.make_button(
                    m_gui.make_label("Red"),
                    [this](){ m_red = 0.667; m_green = 0.333; m_blue = 0.125; }),
                    dick::GUI::Alignment::TOP | dick::GUI::Alignment::RIGHT);
        m_menu_rail->set_instance_name("menu-rail");
    }

    void on_key(dick::Key key, bool down) override
    {
        m_input_state->on_key(key, down);

        if (down) {
            m_last_key = key;
        }

        if (key == dick::Key::ESCAPE) {
            t_transition_required = true;
        }
    }

    void on_button(dick::Button button, bool down) override
    {
        m_input_state->on_button(button, down);

        if (down) {
            m_last_button = button;
            if (m_status_rail) {
                m_status_rail->on_click(button);
            }
            m_menu_rail->on_click(button);
            if (m_ask_to_quit) {
                m_yes_no->on_click(button);
            }
        }
    }

    void on_cursor(dick::DimScreen position) override
    {
        m_input_state->on_cursor(position);
        m_cursor = position;
        m_rotation = atan2(position.y - SCREEN_H / 2, position.x - SCREEN_W / 2);
    }

    void tick(double) override
    {
        std::string key_string, button_string, cursor_string;

        {
            std::stringstream ss;
            ss << "Last key: " << static_cast<int>(m_last_key);
            key_string = ss.str();
        }

        {
            std::stringstream ss;
            ss << "Last button: " << static_cast<int>(m_last_button);
            button_string = ss.str();
        }

        {
            std::stringstream ss;
            ss << "Cursor at: (" << m_cursor.x << ", " << m_cursor.y << ")";
            cursor_string = ss.str();
        }

        m_status_rail = m_gui.make_container_rail(dick::GUI::Direction::DOWN, 20, { 5, 5 });
        m_status_rail->insert(m_gui.make_label(key_string));
        m_status_rail->insert(m_gui.make_label(button_string));
        m_status_rail->insert(m_gui.make_label(cursor_string));
        m_status_rail->set_instance_name("status-rail");
    }

    void draw(double) override
    {
        dick::Frame frame(dick::Color { m_red, m_green, m_blue });

        al_draw_scaled_rotated_bitmap(
                m_bitmap,
                dick::image_width(m_bitmap) / 2,
                dick::image_height(m_bitmap) / 2,
                SCREEN_W / 2, SCREEN_H / 2,
                1.0, 1.0,
                m_rotation,
                0);

        if (m_status_rail) {
            m_status_rail->on_draw();
        }
        m_menu_rail->on_draw();

        if (m_ask_to_quit) {
            m_yes_no->on_draw();
        }
    }

    std::shared_ptr<StateNode> next_state() override
    {
        return dick::create_state_fade_out_color(
                shared_from_this(),
                nullptr,
                0.5,
                0.0, 0.0, 0.0);
    }
};

int main()
{
    dick::Platform platform { dick::DimScreen { SCREEN_W, SCREEN_H } };
    dick::Resources global_resources;

    auto main_state = std::shared_ptr<dick::StateNode> { new DemoState { &global_resources } };

    dick::StateMachine state_machine {
        dick::create_state_fade_in_color(main_state, main_state, 1.0, 0.0, 0.0, 0.0)
    };

    platform.real_time_loop(state_machine);
}
