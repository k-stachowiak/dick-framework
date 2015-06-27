// Copyright (C) 2015 Krzysztof Stachowiak
// For the license (GPL2) details see the LICENSE file

#include <map>
#include <iostream>
#include <utility>

#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_primitives.h>

#include "dick.h"

namespace dick {

class ResourcesImpl {

        // Deleters for the Allegro resources
        // ----------------------------------

        struct FontDeleter {
                void operator()(ALLEGRO_FONT *font)
                {
                        LOG_DEBUG("Deleting font (%p)", font);
                        al_destroy_font(font);
                }
        };

        struct BitmapDeleter {
                void operator()(ALLEGRO_BITMAP *bitmap)
                {
                        LOG_DEBUG("Deleting bitmap (%p)", bitmap);
                        al_destroy_bitmap(bitmap);
                }
        };

        // Object state
        // ------------

        Resources * const m_parent;
        const std::string m_path_prefix;
        std::map<std::string, std::unique_ptr<ALLEGRO_BITMAP, BitmapDeleter>> m_images;
        std::map<std::pair<std::string, int>, std::unique_ptr<ALLEGRO_FONT, FontDeleter>> m_fonts;

        ALLEGRO_BITMAP *m_load_image(const std::string &path)
        {
                std::string full_path = m_path_prefix + path;
                ALLEGRO_BITMAP *bitmap = al_load_bitmap(full_path.c_str());
                if (!bitmap) {
                        throw Error { std::string { "Failed loading image " } + full_path };
                }
                LOG_DEBUG("Loaded image (%s)", full_path.c_str());
                return bitmap;
        }

        ALLEGRO_FONT *m_load_font(const std::string &path, int size)
        {
                std::string full_path = m_path_prefix + path;
                ALLEGRO_FONT *font = al_load_font(full_path.c_str(), -size, 0);
                if (!font) {
                        throw Error { std::string { "Failed loading font " } + full_path };
                }
                LOG_DEBUG("Loaded font (%s)", full_path.c_str());
                return font;
        }

public:
        ResourcesImpl(Resources *parent, const std::string &path_prefix) :
                m_parent { parent },
                m_path_prefix { path_prefix }
        {}

        void *get_image(const std::string &path, bool can_store)
        {
                LOG_TRACE("Getting image (%s)", path.c_str());

                auto it = m_images.find(path);
                if (it != end(m_images)) {
                        LOG_TRACE("Found in this instance");
                        return static_cast<void*>(it->second.get());
                }

                if (m_parent) {
                        LOG_TRACE("Didn't find in this instance; parent exists, checking...");
                        void *parent_result = m_parent->m_impl->get_image(path, false);
                        if (parent_result) {
                                LOG_TRACE("...found in parent");
                                return parent_result;
                        } else {
                                LOG_TRACE("...didn't find in parent");
                        }
                }

                if (can_store) {
                        LOG_TRACE("Is allowed to store resources, trying to load...");
                        ALLEGRO_BITMAP *bitmap = m_load_image(path);
                        LOG_TRACE("...SUCCESS");
                        m_images[path].reset(bitmap);
                        return static_cast<void*>(bitmap);
                } else {
                        LOG_TRACE("Isn't allowed to store resources, report failure");
                        return nullptr;
                }
        }

        void *get_font(const std::string &path, int size, bool can_store)
        {
                LOG_TRACE("Getting font (%s)", path.c_str());

                auto it = m_fonts.find({path, size});
                if (it != end(m_fonts)) {
                        LOG_TRACE("Found in this instance");
                        return static_cast<void*>(it->second.get());
                }

                if (m_parent) {
                        LOG_TRACE("Didn't find in this instance; parent exists, checking...");
                        void *parent_result = m_parent->m_impl->get_font(path, size, false);
                        if (parent_result) {
                                LOG_TRACE("...found in parent");
                                return parent_result;
                        } else {
                                LOG_TRACE("...didn't find in parent");
                        }
                }

                if (can_store) {
                        LOG_TRACE("Is allowed to store resources, trying to load...");
                        ALLEGRO_FONT *font = m_load_font(path, size);
                        LOG_TRACE("...SUCCESS");
                        m_fonts[{path, size}].reset(font);
                        return static_cast<void*>(font);
                } else {
                        LOG_TRACE("Isn't allowed to store resources, report failure");
                        return nullptr;
                }
        }
};

Resources::Resources(Resources *parent, const std::string &path_prefix) : m_impl { new ResourcesImpl { parent, path_prefix } } {}
Resources::~Resources() { delete m_impl; }
void *Resources::get_image(const std::string &path) { return m_impl->get_image(path, true); }
void *Resources::get_font(const std::string &path, int size) { return m_impl->get_font(path, size, true); }

struct StateFadeBlack : public dick::StateNode {
        std::shared_ptr<dick::StateNode> m_child;
        std::shared_ptr<dick::StateNode> m_next;
        const double m_period;
        const double m_red;
        const double m_green;
        const double m_blue;
        const bool m_fade_in;
        double m_timer;
        bool m_done;
        bool m_over;

public:
        StateFadeBlack(std::shared_ptr<dick::StateNode> child,
                        std::shared_ptr<dick::StateNode> next,
                        double period,
                        double red, double green, double blue,
                        bool fade_in) :
                m_child { child },
                m_next { next },
                m_period { period },
                m_red { red }, m_green { green }, m_blue { blue },
                m_fade_in { fade_in },
                m_timer { m_period },
                m_done { false },
                m_over { false }
        {}

        bool is_over() const override { return m_over; }

        void tick(double dt) override
        {
                if (m_over) {
                        return;
                }

                m_timer -= dt;
                if (m_timer <= 0) {
                        if (m_next) {
                                m_done = true;
                        } else {
                                m_over = true;
                        }
                }
        }

        void draw(double weight) override
        {
                ALLEGRO_BITMAP *target = al_get_target_bitmap();
                double alpha = m_fade_in ? (m_timer / m_period) : (1.0 - m_timer / m_period);

                m_child->draw(weight);

                al_draw_filled_rectangle(0, 0,
                        al_get_bitmap_width(target),
                        al_get_bitmap_height(target),
                        al_map_rgba_f(m_red, m_green, m_blue, alpha));
        }

        bool transition_required() const override {
            return m_done;
        }

        std::shared_ptr<StateNode> next_state() override {
                return std::move(m_next);
        }
};

std::shared_ptr<StateNode> create_state_fade_in_color(
                std::shared_ptr<StateNode> child,
                std::shared_ptr<StateNode> next,
                double period,
                double red, double green, double blue)
{
        return std::shared_ptr<StateNode> { new StateFadeBlack { child, next, period, red, green, blue, true } };
}

std::shared_ptr<StateNode> create_state_fade_out_color(
                std::shared_ptr<StateNode> child,
                std::shared_ptr<StateNode> next,
                double period,
                double red, double green, double blue)
{
        return std::shared_ptr<StateNode> { new StateFadeBlack { child, next, period, red, green, blue, false } };
}

class StateMachineImpl {
        std::shared_ptr<StateNode> m_current_state;

        bool m_potential_transition()
        {
                if (m_current_state->transition_required()) {
                        LOG_DEBUG("Client requested state change");
                        m_current_state = std::move(m_current_state->next_state());
                        return true;
                } else {
                        return false;
                }
        }

        void m_unwelcome_transition()
        {
                if (m_potential_transition()) {
                        LOG_WARNING("Unwelcome state transition request e.g. after a draw handler");
                }
        }

public:
        StateMachineImpl(std::shared_ptr<StateNode> init_state) :
                m_current_state { init_state }
        {}

        bool is_over() const
        {
                if (m_current_state) {
                        return m_current_state->is_over();
                } else {
                        return true;
                }
        }

        void on_key(int key, bool down)
        {
                if (m_current_state) {
                        m_current_state->on_key(key, down);
                        m_potential_transition();
                }
        }
        void on_button(int button, bool down)
        {
                if (m_current_state) {
                        m_current_state->on_button(button, down);
                        m_potential_transition();
                }
        }

        void on_cursor(DimScreen position)
        {
                if (m_current_state) {
                        m_current_state->on_cursor(position);
                        m_potential_transition();
                }
        }

        void tick(double dt)
        {
                if (m_current_state) {
                        m_current_state->tick(dt);
                        m_potential_transition();
                }
        }

        void draw(double weight)
        {
                if (m_current_state) {
                        m_current_state->draw(weight);
                        m_unwelcome_transition();
                }
        }
};

StateMachine::StateMachine(std::shared_ptr<StateNode> init_state) :
        m_impl { new StateMachineImpl { init_state } }
{}

StateMachine::~StateMachine() { delete m_impl; }
bool StateMachine::is_over() const { return m_impl->is_over(); }
void StateMachine::on_key(int key, bool down) { m_impl->on_key(key, down); }
void StateMachine::on_button(int button, bool down) { m_impl->on_button(button, down); }
void StateMachine::on_cursor(DimScreen position) { m_impl->on_cursor(position); }
void StateMachine::tick(double dt) { m_impl->tick(dt); }
void StateMachine::draw(double weight) { m_impl->draw(weight); }

class PlatformImpl {

        // Allegro resources deleters
        // --------------------------

        struct DisplayDeleter {
                void operator()(ALLEGRO_DISPLAY *display) {
                        LOG_DEBUG("Deleting display (%p)", display);
                        al_destroy_display(display);
                }
        };

        struct EvQueueDeleter {
                void operator()(ALLEGRO_EVENT_QUEUE *queue) {
                        LOG_DEBUG("Deleting event queue (%p)", queue);
                        al_destroy_event_queue(queue);
                }
        };

        // Platform state
        // --------------

        const double m_fps;
        bool m_kill_flag;
        std::unique_ptr<ALLEGRO_DISPLAY, DisplayDeleter> m_display;
        std::unique_ptr<ALLEGRO_EVENT_QUEUE, EvQueueDeleter> m_ev_queue;

        // Handles all the stimuli from the outside world
        void m_process_events(PlatformClient &client)
        {
                ALLEGRO_EVENT event;
                while (!al_is_event_queue_empty(m_ev_queue.get())) {
                        al_get_next_event(m_ev_queue.get(), &event);
                        switch (event.type) {
                        case ALLEGRO_EVENT_DISPLAY_CLOSE:
                                LOG_DEBUG("Close event received");
                                m_kill_flag = true;
                                return;

                        case ALLEGRO_EVENT_KEY_DOWN:
                                client.on_key(event.keyboard.keycode, true);
                                break;

                        case ALLEGRO_EVENT_KEY_UP:
                                client.on_key(event.keyboard.keycode, false);
                                break;

                        case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
                                client.on_button(event.mouse.button, true);
                                break;

                        case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
                                client.on_button(event.mouse.button, false);
                                break;

                        case ALLEGRO_EVENT_MOUSE_AXES:
                                client.on_cursor(DimScreen {
                                        static_cast<double>(event.mouse.x),
                                        static_cast<double>(event.mouse.y)
                                });
                                break;

                        default:
                                break;
                        }

                        if (client.is_over()) {
                            break;
                        }
                }
        }

        // Updates client's state object and reacts to stimuli coming from it
        void m_realtime_loop_step(double &current_time, double &accumulator, PlatformClient& client)
        {
                static const double max_frame_time = 0.05;
                const double spf = 1.0 / m_fps;
                const double new_time = al_get_time();

                double frame_time = new_time - current_time;

                if (frame_time > max_frame_time) {
                        frame_time = max_frame_time;
                }

                current_time = new_time;
                accumulator += frame_time;

                while (accumulator >= spf) {
                        client.tick(spf);
                        if (client.is_over()) {
                                return;
                        }
                        accumulator -= spf;
                }

                const double frame_weight = accumulator / spf;
                client.draw(frame_weight);
                al_flip_display();
        }

public:
        PlatformImpl(const DimScreen &screen_size) :
                        m_fps { 50.0 },
                        m_kill_flag {}
        {
                if (!al_init()) {
                        throw Error { "Failed initializing core allegro" };
                }
                LOG_TRACE("Initialized core allegro");

                if (!al_init_image_addon()) {
                        throw Error { "Failed initializing image add-on" };
                        exit(1);
                }
                LOG_TRACE("Initialized image add-on");

                al_init_font_addon();
                LOG_TRACE("Initialized font add-on");

                if (!al_init_ttf_addon()) {
                        throw Error { "Failed initializing TTF add-on" };
                        exit(1);
                }
                LOG_TRACE("Initialized TTF add-on");

                if (!al_init_acodec_addon()) {
                        throw Error { "Failed initializing acodec add-on." };
                        exit(1);
                }
                LOG_TRACE("Initialized ACodec add-on");

                if (!al_init_primitives_addon()) {
                        throw Error { "Failed initializing primitives add-on" };
                        exit(1);
                }
                LOG_TRACE("Initialized primitives add-on");

                m_display.reset(al_create_display(screen_size.x, screen_size.y));
                if (!m_display) {
                        throw Error { "Failed creating display" };
                        exit(1);
                }
                LOG_TRACE("Initialized display");

                if (!al_install_keyboard()) {
                        throw Error { "Failed installing keyboard" };
                        exit(1);
                }
                LOG_TRACE("Installed keyboard");

                if (!al_install_mouse()) {
                        throw Error { "Failed installing mouse" };
                        exit(1);
                }
                LOG_TRACE("Installed mouse");

                if(!al_install_audio()) {
                        throw Error { "Failed initializing audio" };
                        exit(1);
                }
                LOG_TRACE("Installed audio");

                m_ev_queue.reset(al_create_event_queue());
                if (!m_ev_queue) {
                        throw Error { "Failed creating event queue" };
                        exit(1);
                }
                LOG_TRACE("Initialized event queue");

                al_register_event_source(m_ev_queue.get(), al_get_display_event_source(m_display.get()));
                al_register_event_source(m_ev_queue.get(), al_get_keyboard_event_source());
                al_register_event_source(m_ev_queue.get(), al_get_mouse_event_source());
                LOG_TRACE("Attached event listeners");
        }

        void real_time_loop(PlatformClient &client)
        {
                double current_time = al_get_time();
                double accumulator = 0;
                m_kill_flag = false;

                while (true) {
                        m_process_events(client);
                        if (client.is_over() || m_kill_flag) break;
                        m_realtime_loop_step(current_time, accumulator, client);
                        if (client.is_over()) break;
                        al_rest(0.001);
                }
        }
};

Platform::Platform(const DimScreen &screen_size) : m_impl { new PlatformImpl { screen_size } } {}
Platform::~Platform() { delete m_impl; }
void Platform::real_time_loop(PlatformClient &client) { m_impl->real_time_loop(client); }

}
