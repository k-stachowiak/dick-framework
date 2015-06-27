// Copyright (C) 2015 Krzysztof Stachowiak
// For the license (GPL2) details see the LICENSE file

#include <cmath>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "dick.h"

const int SCREEN_W = 640;
const int SCREEN_H = 480;
const std::string FONT_NAME = "Roboto-Medium.ttf";
const int FONT_SIZE = 20;
const std::string IMAGE_NAME = "db.png";

struct DemoState : public dick::StateNode, std::enable_shared_from_this<dick::StateNode> {

        int m_last_key;
        int m_last_button;
        dick::DimScreen m_cursor;
        double m_rotation;

        dick::Resources m_resources;
        ALLEGRO_FONT *m_font;
        ALLEGRO_BITMAP *m_bitmap;

        DemoState(dick::Resources *global_resources) :
                m_last_key { -1 },
                m_last_button { -1 },
                m_cursor { -1.0, -1.0 },
                m_rotation { 0.0 },
                m_resources { global_resources },
                m_font { static_cast<ALLEGRO_FONT*>(m_resources.get_font(FONT_NAME, FONT_SIZE)) },
                m_bitmap { static_cast<ALLEGRO_BITMAP*>(m_resources.get_image(IMAGE_NAME)) }
        {}

        std::shared_ptr<StateNode> on_key(int key, bool down) override
        {
                if (down) {
                        m_last_key = key;
                } else {
                        m_last_key = -1;
                }


                if (key == ALLEGRO_KEY_ESCAPE) {
                        return dick::create_state_fade_out_color(shared_from_this(), nullptr, 0.5, 0.0, 0.0, 0.0);
                }

                return {};
        }

        std::shared_ptr<StateNode> on_button(int button, bool down) override
        {
                if (down) {
                        m_last_button = button;
                } else {
                        m_last_button = -1;
                }
                return {};
        }

        std::shared_ptr<StateNode> on_cursor(dick::DimScreen position) override
        {
                m_cursor = position;
                m_rotation = atan2(position.y - SCREEN_H / 2, position.x - SCREEN_W / 2);
                return {};
        }

        void draw(double weight) override
        {
                al_clear_to_color(al_map_rgb_f(0.333, 0.55, 0.7));
                al_draw_textf(m_font, al_map_rgb_f(1, 1, 1), 10.0f, 10.0f, 0, "Last key : %d", m_last_key);
                al_draw_textf(m_font, al_map_rgb_f(1, 1, 1), 10.0f, 30.0f, 0, "Last button: %d", m_last_button);
                al_draw_textf(m_font, al_map_rgb_f(1, 1, 1), 10.0f, 50.0f, 0, "Mouse at : (%.1f, %.1f)", m_cursor.x, m_cursor.y);
                al_draw_scaled_rotated_bitmap(
                        m_bitmap,
                        al_get_bitmap_width(m_bitmap) / 2,
                        al_get_bitmap_height(m_bitmap) / 2,
                        SCREEN_W / 2, SCREEN_H / 2,
                        1.0, 1.0,
                        m_rotation,
                        0);
        }
};

int main()
{
        dick::Platform platform { dick::DimScreen { SCREEN_W, SCREEN_H } };
        dick::Resources global_resources;
        global_resources.get_font(FONT_NAME, FONT_SIZE);
        auto main_state = std::shared_ptr<dick::StateNode> { new DemoState { &global_resources } };
        platform.real_time_loop(dick::create_state_fade_in_color(main_state, main_state, 1.0, 0.0, 0.0, 0.0));
}

