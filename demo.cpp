// Copyright (C) 2015 Krzysztof Stachowiak
// For the license (GPL2) details see the LICENSE file

#include <cmath>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>

#include "dick.h"

struct DemoState : public dick::StateNode {

        bool m_over;
        int m_last_key;
        int m_last_button;
        dick::DimScreen m_cursor;
        double m_rotation;

        dick::Resources m_resources;
        ALLEGRO_FONT *m_font;
        ALLEGRO_BITMAP *m_bitmap;

        DemoState(dick::Resources *global_resources) :
                m_over { false },
                m_last_key { -1 },
                m_last_button { -1 },
                m_cursor { -1.0, -1.0 },
                m_rotation { 0.0 },
                m_resources { global_resources },
                m_font { static_cast<ALLEGRO_FONT*>(m_resources.get_font("Roboto-Medium.ttf", 13)) },
                m_bitmap { static_cast<ALLEGRO_BITMAP*>(m_resources.get_image("db.png")) }
        {}

        bool is_over() const override
        {
                return m_over;
        }

        std::unique_ptr<StateNode> on_key(int key, bool down) override
        {
                if (down) {
                        m_last_key = key;
                } else {
                        m_last_key = -1;
                }

                if (key == ALLEGRO_KEY_Q) {
                        m_over = true;
                }

                return {};
        }

        std::unique_ptr<StateNode> on_button(int button, bool down) override
        {
                if (down) {
                        m_last_button = button;
                } else {
                        m_last_button = -1;
                }
                return {};
        }

        std::unique_ptr<StateNode> on_cursor(dick::DimScreen position) override
        {
                m_cursor = position;
                m_rotation = atan2(position.y - 240.0, position.x - 320.0);
                return {};
        }

        void draw(double weight) override
        {
                al_clear_to_color(al_map_rgb_f(0.333, 0.5, 0.667));
                al_draw_textf(m_font, al_map_rgb_f(1, 1, 1), 10.0f, 10.0f, 0, "Last key : %d", m_last_key);
                al_draw_textf(m_font, al_map_rgb_f(1, 1, 1), 10.0f, 30.0f, 0, "Last button: %d", m_last_button);
                al_draw_textf(m_font, al_map_rgb_f(1, 1, 1), 10.0f, 50.0f, 0, "Mouse at : (%.1f, %.1f)", m_cursor.x, m_cursor.y);
                al_draw_scaled_rotated_bitmap(m_bitmap, al_get_bitmap_width(m_bitmap) / 2, al_get_bitmap_height(m_bitmap) / 2, 320, 240, 1.0, 1.0, m_rotation, 0);
                al_flip_display();
        }
};

int main()
{
        dick::Platform platform { dick::DimScreen { 640, 480 } };
        dick::Resources global_resources;
        global_resources.get_font("Roboto-Medium.ttf", 14);
        platform.real_time_loop(std::make_unique<DemoState>(&global_resources));
}
