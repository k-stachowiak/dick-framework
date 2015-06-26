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

// A resources object stores game assets. Any resource object may point to
// a parent and will perform lookup upstream before reporting an error.
// The upstream lookup is allowed to fail and return a null pointer, whereas
// the public API calls requesting unavailable resources will throw.
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
        std::unique_ptr<StateNode> m_process_events(StateNode& state_node)
        {
                ALLEGRO_EVENT event;
                std::unique_ptr<StateNode> next_state;

                while (!al_is_event_queue_empty(m_ev_queue.get())) {
                        al_get_next_event(m_ev_queue.get(), &event);
                        switch (event.type) {
                        case ALLEGRO_EVENT_DISPLAY_CLOSE:
                                LOG_DEBUG("Close event received");
                                m_kill_flag = true;
                                return {};

                        case ALLEGRO_EVENT_KEY_DOWN:
                                next_state = state_node.on_key(event.keyboard.keycode, true);
                                break;

                        case ALLEGRO_EVENT_KEY_UP:
                                next_state = state_node.on_key(event.keyboard.keycode, false);
                                break;

                        case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
                                next_state = state_node.on_button(event.mouse.button, true);
                                break;

                        case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
                                next_state = state_node.on_button(event.mouse.button, false);
                                break;

                        case ALLEGRO_EVENT_MOUSE_AXES:
                                next_state = state_node.on_cursor(DimScreen {
                                        static_cast<double>(event.mouse.x),
                                        static_cast<double>(event.mouse.y)
                                });
                                break;

                        default:
                                break;
                        }

                        if (state_node.is_over()) {
                                LOG_DEBUG("Client event handler triggered \"over\" state");
                                return {};
                        }

                        if (next_state) {
                                LOG_DEBUG("Client event handler requested state change");
                                return next_state;
                        }
                }

                return {};
        }

        // Updates client's state object and reacts to stimuli coming from it
        std::unique_ptr<StateNode> m_realtime_loop_step(double &current_time, double &accumulator, StateNode& state_node)
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
                        std::unique_ptr<StateNode> next_state = state_node.tick(spf);
                        if (next_state) {
                                LOG_DEBUG("Client tick function requested state change");
                                return next_state;
                        }
                        accumulator -= spf;
                }

                const double frame_weight = accumulator / spf;
                state_node.draw(frame_weight);

                return {};
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

                /*
                 if(!al_install_audio()) {
                         throw Error { "Failed initializing audio" };
                         exit(1);
                 }
                 LOG_TRACE("Installed audio");
                 */

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

        void real_time_loop(std::unique_ptr<StateNode> current_state)
        {
                std::unique_ptr<StateNode> next_state;
                double current_time = al_get_time();
                double accumulator = 0;
                m_kill_flag = false;

                while (true) {

                        next_state = m_process_events(*current_state.get());
                        if (current_state->is_over() || m_kill_flag) {
                                break;

                        } else if (next_state) {
                                current_state = std::move(next_state);

                        }

                        next_state = m_realtime_loop_step(current_time, accumulator, *current_state.get());
                        if (current_state->is_over()) {
                                break;

                        } else if (next_state) {
                                current_state = std::move(next_state);

                        }

                        al_rest(0.001);
                }
        }
};

Platform::Platform(const DimScreen &screen_size) : m_impl { new PlatformImpl { screen_size } } {}
Platform::~Platform() { delete m_impl; }
void Platform::real_time_loop(std::unique_ptr<StateNode> init_state) { m_impl->real_time_loop(std::move(init_state)); }

}
