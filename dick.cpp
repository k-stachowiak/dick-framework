// Copyright (C) 2015 Krzysztof Stachowiak
// For the license (GPL2) details see the LICENSE file

#include <cassert>

#include <map>
#include <numeric>
#include <utility>
#include <iostream>
#include <algorithm>

#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_primitives.h>

#include "dick.h"

namespace dick {

// Common utility

inline ALLEGRO_COLOR dick_to_platform_color(const Color& x)
{
    return al_map_rgb_f(x.r, x.g, x.b);
}

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

Resources::Resources(Resources *parent, const std::string &path_prefix) :
    m_impl { new ResourcesImpl { parent, path_prefix } } {}
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

public:
    StateFadeBlack(
            std::shared_ptr<dick::StateNode> child,
            std::shared_ptr<dick::StateNode> next,
            double period,
            double red, double green, double blue,
            bool fade_in) :
        m_child { child },
        m_next { next },
        m_period { period },
        m_red { red }, m_green { green }, m_blue { blue },
        m_fade_in { fade_in },
        m_timer { m_period }
    {}

    void tick(double dt) override
    {
        if (t_is_over) {
            return;
        }

        m_timer -= dt;
        if (m_timer <= 0) {
            if (m_next) {
                t_transition_required = true;
            } else {
                t_is_over = true;
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
    return std::shared_ptr<StateNode> {
        new StateFadeBlack { child, next, period, red, green, blue, true }
    };
}

std::shared_ptr<StateNode> create_state_fade_out_color(
        std::shared_ptr<StateNode> child,
        std::shared_ptr<StateNode> next,
        double period,
        double red, double green, double blue)
{
    return std::shared_ptr<StateNode> {
        new StateFadeBlack { child, next, period, red, green, blue, false }
    };
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

    void on_key(Key key, bool down)
    {
        if (m_current_state) {
            m_current_state->on_key(key, down);
            m_potential_transition();
        }
    }

    void on_button(Button button, bool down)
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
            m_potential_transition();
        }
    }
};

StateMachine::StateMachine(std::shared_ptr<StateNode> init_state) :
    m_impl { new StateMachineImpl { init_state } }
{}

StateMachine::~StateMachine() { delete m_impl; }
bool StateMachine::is_over() const { return m_impl->is_over(); }
void StateMachine::on_key(Key key, bool down) { m_impl->on_key(key, down); }
void StateMachine::on_button(Button button, bool down) { m_impl->on_button(button, down); }
void StateMachine::on_cursor(DimScreen position) { m_impl->on_cursor(position); }
void StateMachine::tick(double dt) { m_impl->tick(dt); }
void StateMachine::draw(double weight) { m_impl->draw(weight); }

bool Widget::point_in(const DimScreen &point) const
{
    const DimScreen &size = get_size();
    const double x0 = t_offset.x;
    const double y0 = t_offset.y;
    const double x1 = x0 + size.x;
    const double y1 = y0 + size.y;
    return point.x >= x0 && point.x <= x1 && point.y >= y0 && point.y <= y1;
}

DimScreen Widget::align(DimScreen origin, const DimScreen& size, int alignment)
{
    if (alignment & Alignment::RIGHT) {
        origin.x -= size.x;
    }

    if (alignment & Alignment::CENTER) {
        origin.x -= size.x / 2;
    }

    if (alignment & Alignment::LEFT) {
        origin.x += 0;
    }

    if (alignment & Alignment::TOP) {
        origin.y += 0;
    }

    if (alignment & Alignment::MIDDLE) {
        origin.y -= size.y / 2;
    }

    if (alignment & Alignment::BOTTOM) {
        origin.y -= size.y;
    }

    return origin;
}

struct WidgetLabel : public Widget {

    DimScreen m_size;
    std::string m_text;

    WidgetLabel(
            const std::shared_ptr<ColorScheme>& color_scheme,
            const std::shared_ptr<LayoutScheme>& layout_scheme,
            const std::shared_ptr<InputState>& input_state,
            const std::string &text,
            const DimScreen& offset) :
        Widget { color_scheme, layout_scheme, input_state, offset },
        m_size {
            static_cast<double>(
                al_get_text_width(
                    static_cast<ALLEGRO_FONT*>(
                        layout_scheme->default_font
                    ),
                    text.c_str()
                )
            ),
            static_cast<double>(
                al_get_font_line_height(
                    static_cast<ALLEGRO_FONT*>(
                        layout_scheme->default_font
                    )
                )
            )
        },
        m_text { text }
    {
    }

    void draw() override
    {
        al_draw_textf(
            static_cast<ALLEGRO_FONT*>(t_layout_scheme->default_font),
            dick_to_platform_color(t_color_scheme->text_regular),
            t_offset.x,
            t_offset.y,
            0,
            "%s",
            m_text.c_str());
    }

    DimScreen get_size() const override
    {
        return m_size;
    }
};

struct WidgetButton : public Widget {

    DimScreen m_size;
    std::unique_ptr<Widget> m_sub_widget;
    Callback m_callback;

    void m_compute_sub_offset()
    {
        DimScreen middle = t_offset;
        middle.x += m_size.x / 2;
        middle.y += m_size.y / 2;
        const DimScreen& sub_size = m_sub_widget->get_size();
        m_sub_widget->set_offset(
            align(
                middle,
                sub_size,
                Alignment::MIDDLE | Alignment::CENTER
            )
        );
    }

    WidgetButton(
            const std::shared_ptr<ColorScheme>& color_scheme,
            const std::shared_ptr<LayoutScheme>& layout_scheme,
            const std::shared_ptr<InputState>& input_state,
            std::unique_ptr<Widget> sub_widget,
            Callback callback,
            const DimScreen& size,
            const DimScreen& offset) :
        Widget { color_scheme, layout_scheme, input_state, offset },
        m_size(size),
        m_sub_widget { std::move(sub_widget) },
        m_callback { callback }
    {
        if (size.x == 0 && size.y == 0) {
            const DimScreen& sub_size = m_sub_widget->get_size();
            m_size.x = sub_size.x + 2 * layout_scheme->button_padding.x;
            m_size.y = sub_size.y + 2 * layout_scheme->button_padding.y;
        }
        m_compute_sub_offset();
    }

    void on_click(Button button) override
    {
        if (button == Button::BUTTON_1 && point_in(t_input_state->cursor)) {
            m_callback();
        }
    }

    void draw() override
    {
        double x0 = t_offset.x;
        double y0 = t_offset.y;
        double x1 = x0 + m_size.x;
        double y1 = y0 + m_size.y;

        ALLEGRO_COLOR bg_color;
        ALLEGRO_COLOR border_color;
        ALLEGRO_COLOR text_color;

        if (point_in(t_input_state->cursor)) {
            bg_color = dick_to_platform_color(t_color_scheme->bg_active);
            border_color = dick_to_platform_color(t_color_scheme->border_active);
            text_color = dick_to_platform_color(t_color_scheme->text_active);
        } else {
            bg_color = dick_to_platform_color(t_color_scheme->bg_regular);
            border_color = dick_to_platform_color(t_color_scheme->border_regular);
            text_color = dick_to_platform_color(t_color_scheme->text_regular);
        }

        al_draw_filled_rectangle(x0, y0, x1, y1, bg_color);
        al_draw_rectangle(x0, y0, x1, y1, border_color, t_layout_scheme->border_width);

        m_sub_widget->draw();
    }

    DimScreen get_size() const override
    {
        return m_size;
    }

    void set_offset(const DimScreen& offset) override
    {
        t_offset = offset;
        m_compute_sub_offset();
    }
};

struct WidgetContainerFree : public WidgetContainer {

    std::vector<std::unique_ptr<Widget>> m_children;

    WidgetContainerFree(
            const std::shared_ptr<ColorScheme>& color_scheme,
            const std::shared_ptr<LayoutScheme>& layout_scheme,
            const std::shared_ptr<InputState>& input_state,
            const DimScreen& offset) :
        WidgetContainer { color_scheme, layout_scheme, input_state, offset }
    {
    }

    void on_click(Button button) override
    {
        for (const auto& child : m_children) {
            child->on_click(button);
        }
    }

    void draw() override
    {
        for (const auto& child : m_children) {
            child->draw();
        }
    }

    void set_offset(const DimScreen& offset) override
    {
        double dx = offset.x;
        double dy = offset.y;
        for (const auto& child : m_children) {
            const DimScreen& child_offset = child->get_offset();
            child->set_offset({
                child_offset.x + dx,
                child_offset.y + dy });
        }
    }

    bool point_in(const DimScreen&) const override
    {
        return true;
    }

    void insert(std::unique_ptr<Widget> widget) override
    {
        const DimScreen& current_offset = widget->get_offset();
        widget->set_offset({
            current_offset.x + t_offset.x,
            current_offset.y + t_offset.y });
        m_children.push_back(std::move(widget));
    }

    void remove(Widget* widget) override
    {
        auto it = std::find_if(
                begin(m_children),
                end(m_children),
                [widget](const std::unique_ptr<Widget>& child)
                {
                    return child.get() == widget;
                });

        if (it != end(m_children)) {
            m_children.erase(it);
        }
    }

    bool contains(Widget* widget) override
    {
        auto it = std::find_if(
                begin(m_children),
                end(m_children),
                [widget](const std::unique_ptr<Widget>& child)
                {
                    return child.get() == widget;
                });

        return it != end(m_children);
    }

    void clear() override
    {
        m_children.clear();
    }
};

struct WidgetContainerRail : public WidgetContainer {

    bool m_vertical;
    int m_children_alignment;
    std::vector<std::unique_ptr<Widget>> m_children;

    void m_arrange_children()
    {
        DimScreen current_offset = t_offset;
        for (const std::unique_ptr<Widget>& child : m_children) {
            const DimScreen& child_size = child->get_size();
            child->set_offset(align(current_offset, child_size, m_children_alignment));
            if (m_vertical) {
                current_offset.y += child_size.y + t_layout_scheme->rail_padding.y;
            } else {
                current_offset.x += child_size.x + t_layout_scheme->rail_padding.x;
            }
        }
    }

    WidgetContainerRail(
            const std::shared_ptr<ColorScheme>& color_scheme,
            const std::shared_ptr<LayoutScheme>& layout_scheme,
            const std::shared_ptr<InputState>& input_state,
            bool vertical,
            int children_alignment,
            const DimScreen& offset) :
        WidgetContainer { color_scheme, layout_scheme, input_state, offset },
        m_vertical { vertical },
        m_children_alignment { children_alignment }
    {
    }

    void on_click(Button button) override
    {
        for (const auto& child : m_children) {
            child->on_click(button);
        }
    }

    void draw() override
    {
        for (const auto& child : m_children) {
            child->draw();
        }
    }

    void set_offset(const DimScreen& offset) override
    {
        double dx = offset.x;
        double dy = offset.y;
        for (const auto& child : m_children) {
            const DimScreen& child_offset = child->get_offset();
            child->set_offset({
                child_offset.x + dx,
                child_offset.y + dy });
        }
    }

    bool point_in(const DimScreen&) const override
    {
        return true;
    }

    void insert(std::unique_ptr<Widget> widget) override
    {
        m_children.push_back(std::move(widget));
        m_arrange_children();
    }

    void remove(Widget* widget) override
    {
        auto it = std::find_if(
                begin(m_children),
                end(m_children),
                [widget](const std::unique_ptr<Widget>& child)
                {
                    return child.get() == widget;
                });

        if (it != end(m_children)) {
            m_children.erase(it);
            m_arrange_children();
        }
    }

    bool contains(Widget* widget) override
    {
        auto it = std::find_if(
                begin(m_children),
                end(m_children),
                [widget](const std::unique_ptr<Widget>& child)
                {
                    return child.get() == widget;
                });

        return it != end(m_children);
    }

    void clear() override
    {
        m_children.clear();
    }
};

struct WidgetFactoryImpl {

    void *m_default_font;

    std::shared_ptr<ColorScheme> m_color_scheme;
    std::shared_ptr<LayoutScheme> m_layout_scheme;
    std::shared_ptr<InputState> m_input_state;

    WidgetFactoryImpl(
            const std::shared_ptr<InputState>& input_state,
            Resources& resources) :
        m_default_font {
            resources.get_font("default.ttf", 24)
        },
        m_color_scheme {
            new ColorScheme {
                Color { 0.76, 0.74, 0.72 },
                Color { 0.86, 0.84, 0.82 },
                Color { 0.76, 0.74, 0.72 },
                Color { 0.66, 0.64, 0.62 },
                Color { 0.76, 0.74, 0.72 },
                Color { 0.66, 0.64, 0.62 },
                Color { 0.0, 0.0, 0.0 },
                Color { 0.1, 0.1, 0.0 },
                Color { 0.5, 0.5, 0.5 }
            }
        },
        m_layout_scheme {
            new LayoutScheme {
                m_default_font,
                2.0,
                { 5.0, 5.0 },
                { 3.0, 3.0 }
            }
        },
        m_input_state {
            input_state
        }
    {
    }

    std::unique_ptr<Widget> make_label(
            const std::string& text,
            const DimScreen& offset)
    {
        std::unique_ptr<Widget> result {
            new WidgetLabel {
                m_color_scheme,
                m_layout_scheme,
                m_input_state,
                text,
                offset
            }
        };
        return result;
    }

    std::unique_ptr<Widget> make_button(
            std::unique_ptr<Widget> sub_widget,
            Callback callback,
            const DimScreen& offset)
    {
        std::unique_ptr<Widget> result {
            new WidgetButton {
                m_color_scheme,
                m_layout_scheme,
                m_input_state,
                std::move(sub_widget),
                callback,
                { 0, 0 },
                offset
            }
        };
        return result;
    }

    std::unique_ptr<Widget> make_button_sized(
            std::unique_ptr<Widget> sub_widget,
            Callback callback,
            const DimScreen& size,
            const DimScreen& offset)
    {
        std::unique_ptr<Widget> result {
            new WidgetButton {
                m_color_scheme,
                m_layout_scheme,
                m_input_state,
                std::move(sub_widget),
                callback,
                size,
                offset
            }
        };
        return result;
    }

    std::unique_ptr<WidgetContainer> make_container_free(
            const DimScreen& offset)
    {
        std::unique_ptr<WidgetContainer> result {
            new WidgetContainerFree {
                m_color_scheme,
                m_layout_scheme,
                m_input_state,
                offset
            }
        };
        return result;
    }

    std::unique_ptr<WidgetContainer> make_container_rail(
            bool vertical,
            int children_alignment,
            const DimScreen& offset)
    {
        std::unique_ptr<WidgetContainer> result {
            new WidgetContainerRail {
                m_color_scheme,
                m_layout_scheme,
                m_input_state,
                vertical,
                children_alignment,
                offset
            }
        };
        return result;
    }
};

WidgetFactory::WidgetFactory(
        const std::shared_ptr<InputState>& input_state,
        Resources& resources) :
    m_impl { new WidgetFactoryImpl { input_state, resources } }
{
}

WidgetFactory::~WidgetFactory()
{
    delete m_impl;
}

std::unique_ptr<Widget> WidgetFactory::make_label(
        const std::string& text,
        const DimScreen& offset)
{
    return m_impl->make_label(text, offset);
}

std::unique_ptr<Widget> WidgetFactory::make_button(
        std::unique_ptr<Widget> sub_widget,
        Callback callback,
        const DimScreen& offset)
{
    return m_impl->make_button(std::move(sub_widget), callback, offset);
}

std::unique_ptr<Widget> WidgetFactory::make_button_sized(
        std::unique_ptr<Widget> sub_widget,
        Callback callback,
        const DimScreen& size,
        const DimScreen& offset)
{
    return m_impl->make_button_sized(std::move(sub_widget), callback, size, offset);
}

std::unique_ptr<WidgetContainer> WidgetFactory::make_container_free(
        const DimScreen& offset)
{
    return m_impl->make_container_free(offset);
}

std::unique_ptr<WidgetContainer> WidgetFactory::make_container_rail(
        bool vertical,
        int children_alignment,
        const DimScreen& offset)
{
    return m_impl->make_container_rail(vertical, children_alignment, offset);
}

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

    // Translation of the platform library constants
    // ---------------------------------------------

    static Key m_platform_to_dick_key(int key)
    {
        switch (key) {
        case ALLEGRO_KEY_UP: return Key::UP;
        case ALLEGRO_KEY_DOWN: return Key::DOWN;
        case ALLEGRO_KEY_LEFT: return Key::LEFT;
        case ALLEGRO_KEY_RIGHT: return Key::RIGHT;
        case ALLEGRO_KEY_ESCAPE: return Key::ESCAPE;
        case ALLEGRO_KEY_SPACE: return Key::SPACE;
        case ALLEGRO_KEY_ENTER: return Key::ENTER;
        case ALLEGRO_KEY_BACKSPACE: return Key::BACKSPACE;
        case ALLEGRO_KEY_TAB: return Key::TAB;
        case ALLEGRO_KEY_A: return Key::A;
        case ALLEGRO_KEY_B: return Key::B;
        case ALLEGRO_KEY_C: return Key::C;
        case ALLEGRO_KEY_D: return Key::D;
        case ALLEGRO_KEY_E: return Key::E;
        case ALLEGRO_KEY_F: return Key::F;
        case ALLEGRO_KEY_G: return Key::G;
        case ALLEGRO_KEY_H: return Key::H;
        case ALLEGRO_KEY_I: return Key::I;
        case ALLEGRO_KEY_J: return Key::J;
        case ALLEGRO_KEY_K: return Key::K;
        case ALLEGRO_KEY_L: return Key::L;
        case ALLEGRO_KEY_M: return Key::M;
        case ALLEGRO_KEY_N: return Key::N;
        case ALLEGRO_KEY_O: return Key::O;
        case ALLEGRO_KEY_P: return Key::P;
        case ALLEGRO_KEY_Q: return Key::Q;
        case ALLEGRO_KEY_R: return Key::R;
        case ALLEGRO_KEY_S: return Key::S;
        case ALLEGRO_KEY_T: return Key::T;
        case ALLEGRO_KEY_U: return Key::U;
        case ALLEGRO_KEY_V: return Key::V;
        case ALLEGRO_KEY_W: return Key::W;
        case ALLEGRO_KEY_X: return Key::X;
        case ALLEGRO_KEY_Y: return Key::Y;
        case ALLEGRO_KEY_Z: return Key::Z;
        case ALLEGRO_KEY_0: return Key::KEY_0;
        case ALLEGRO_KEY_1: return Key::KEY_1;
        case ALLEGRO_KEY_2: return Key::KEY_2;
        case ALLEGRO_KEY_3: return Key::KEY_3;
        case ALLEGRO_KEY_4: return Key::KEY_4;
        case ALLEGRO_KEY_5: return Key::KEY_5;
        case ALLEGRO_KEY_6: return Key::KEY_6;
        case ALLEGRO_KEY_7: return Key::KEY_7;
        case ALLEGRO_KEY_8: return Key::KEY_8;
        case ALLEGRO_KEY_9: return Key::KEY_9;
        default: return Key::UNHANDLED;
        }
    }

    static Button m_platform_to_dick_button(int button)
    {
        switch (button) {
        case 1: return Button::BUTTON_1;
        case 2: return Button::BUTTON_2;
        case 3: return Button::BUTTON_3;
        default: return Button::UNHANDLED;
        }
    }

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
                client.on_key(m_platform_to_dick_key(event.keyboard.keycode), true);
                break;

            case ALLEGRO_EVENT_KEY_UP:
                client.on_key(m_platform_to_dick_key(event.keyboard.keycode), false);
                break;

            case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
                client.on_button(m_platform_to_dick_button(event.mouse.button), true);
                break;

            case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
                client.on_button(m_platform_to_dick_button(event.mouse.button), false);
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
    ~PlatformImpl()
    {
        m_ev_queue.reset();
        al_uninstall_audio();
        al_uninstall_mouse();
        al_uninstall_keyboard();
        m_display.reset();
        al_shutdown_primitives_addon();
        al_shutdown_ttf_addon();
        al_shutdown_font_addon();
        al_shutdown_image_addon();
        al_uninstall_system();
    }

    PlatformImpl(const DimScreen &screen_size) :
        m_fps { 50.0 },
        m_kill_flag {}
    {
        if (!al_install_system(ALLEGRO_VERSION_INT, atexit)) {
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
            if (client.is_over() || m_kill_flag) break;
            al_rest(0.001);
        }
    }
};

Platform::Platform(const DimScreen &screen_size) : m_impl { new PlatformImpl { screen_size } } {}
Platform::~Platform() { delete m_impl; }
void Platform::real_time_loop(PlatformClient &client) { m_impl->real_time_loop(client); }

}
