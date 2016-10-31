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

const std::string version = "0.2.0";

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
    ResourcesImpl(const std::string &path_prefix, Resources *parent) :
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

Resources::Resources(const std::string &path_prefix, Resources *parent) :
    m_impl { new ResourcesImpl { path_prefix, parent } } {}
Resources::~Resources() { delete m_impl; }
void *Resources::get_image(const std::string &path) { return m_impl->get_image(path, true); }
void *Resources::get_font(const std::string &path, int size) { return m_impl->get_font(path, size, true); }

double image_width(void *image)
{
    return static_cast<double>(
        al_get_bitmap_width(
            static_cast<ALLEGRO_BITMAP*>(
                image)));
}

double image_height(void *image)
{
    return static_cast<double>(
        al_get_bitmap_height(
            static_cast<ALLEGRO_BITMAP*>(
                image)));
}

DimScreen image_size(void *image)
{
    return { image_width(image), image_height(image) };
}

Frame::Frame(Color clear_color)
{
    al_clear_to_color(
        al_map_rgb_f(
            clear_color.r,
            clear_color.g,
            clear_color.b));
}

Frame::~Frame()
{
    al_flip_display();
}

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
                image_width(target),
                image_height(target),
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
            m_current_state = m_current_state->next_state();
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

void GUI::Widget::debug_draw() const
{
    DimScreen top_left, bottom_right;
    std::tie(top_left, bottom_right) = get_rect();
    al_draw_circle(t_offset.x, t_offset.y, 3, al_map_rgb_f(0, 1, 1), 1);
    al_draw_rectangle(
            top_left.x, top_left.y,
            bottom_right.x, bottom_right.y,
            al_map_rgb_f(1, 0, 1),
            1);
#   if DICK_GUI_DEBUG > 1
    if (point_in(t_input_state->cursor)) {
        al_draw_textf(
            static_cast<ALLEGRO_FONT*>(t_default_font),
            al_map_rgb_f(1, 1, 0),
            t_offset.x, t_offset.y,
            0,
            "%s::%s",
            get_type_name().c_str(), get_instance_name().c_str());
    }
#   endif

    const WidgetContainer *as_container = dynamic_cast<const WidgetContainer*>(this);
    if (as_container) {
        as_container->visit_children([](const Widget& child) { child.debug_draw(); });
    }
}

void GUI::Widget::debug_print(int recursion_level) const
{
    std::string prefix(2 * recursion_level, ' ');

    const DimScreen &size = get_size();
    const DimScreen &offset = get_offset();

    DimScreen top_left, bottom_right;
    std::tie(top_left, bottom_right) = get_rect();

    LOG_DEBUG("%s%s::%s o(%.1f, %.1f) s(%.1f, %.1f) r((%.1f, %.1f), (%.1f, %.1f))",
            prefix.c_str(),
            get_type_name().c_str(), get_instance_name().c_str(),
            offset.x, offset.y, size.x, size.y,
            top_left.x, top_left.y, bottom_right.x, bottom_right.y);

    const WidgetContainer *as_container = dynamic_cast<const WidgetContainer*>(this);
    if (as_container) {
        LOG_DEBUG("%sChildren:", prefix.c_str());
        as_container->visit_children(
            [recursion_level](const Widget& child)
            {
                child.debug_print(recursion_level + 1);
            });
    }
}

std::pair<DimScreen, DimScreen> GUI::Widget::get_rect() const
{
    const DimScreen& size = get_size();
    DimScreen bottom_right { t_offset.x + size.x, t_offset.y + size.y };
    return std::make_pair(t_offset, bottom_right);
}

DimScreen GUI::Widget::get_size() const
{
    DimScreen top_left, bottom_right;
    std::tie(top_left, bottom_right) = get_rect();
    return DimScreen { bottom_right.x - top_left.x, bottom_right.y - top_left.y };
}

void GUI::Widget::align(const DimScreen& point, int alignment)
{
    DimScreen top_left, bottom_right;
    std::tie(top_left, bottom_right) = get_rect();

    const DimScreen &size = get_size();

    DimScreen new_top_left = align_point(point, size, alignment);

    double dx = new_top_left.x - top_left.x;
    double dy = new_top_left.y - top_left.y;

    set_offset({ t_offset.x + dx, t_offset.y + dy });
}

bool GUI::Widget::point_in(const DimScreen &point) const
{
    DimScreen top_left, bottom_right;
    std::tie(top_left, bottom_right) = get_rect();

    double x0 = top_left.x;
    double y0 = top_left.y;
    double x1 = bottom_right.x;
    double y1 = bottom_right.y;

    return point.x >= x0 && point.x <= x1 && point.y >= y0 && point.y <= y1;
}

DimScreen GUI::Widget::align_point(DimScreen point, const DimScreen& size, int alignment)
{
    if (alignment & Alignment::RIGHT) {
        point.x -= size.x;
    }

    if (alignment & Alignment::CENTER) {
        point.x -= size.x / 2;
    }

    if (alignment & Alignment::LEFT) {
        point.x += 0;
    }

    if (alignment & Alignment::TOP) {
        point.y += 0;
    }

    if (alignment & Alignment::MIDDLE) {
        point.y -= size.y / 2;
    }

    if (alignment & Alignment::BOTTOM) {
        point.y -= size.y;
    }

    return point;
}

struct WidgetImage : public GUI::Widget {

    void *m_image;

    WidgetImage(void *default_font,
                const std::shared_ptr<GUI::ColorScheme>& color_scheme,
                const std::shared_ptr<GUI::LayoutScheme>& layout_scheme,
                const std::shared_ptr<InputState>& input_state,
                void *image,
                const DimScreen& offset,
                const std::string& instance_name) :
        Widget { default_font, color_scheme, layout_scheme, input_state, offset, instance_name },
        m_image { image }
    {}

    void on_draw() override
    {
        al_draw_bitmap(static_cast<ALLEGRO_BITMAP*>(m_image), t_offset.x, t_offset.y, 0);
    }

    DimScreen get_size() const override
    {
        return image_size(m_image);
    }

    const std::string &get_type_name() const override
    {
        static std::string name = "image";
        return name;
    }
};

struct WidgetLabel : public GUI::Widget {

    void *m_font;
    std::string m_text;

    WidgetLabel(void *default_font,
                const std::shared_ptr<GUI::ColorScheme>& color_scheme,
                const std::shared_ptr<GUI::LayoutScheme>& layout_scheme,
                const std::shared_ptr<InputState>& input_state,
                const std::string &text,
                void *font,
                const DimScreen& offset,
                const std::string& instance_name) :
        Widget { default_font, color_scheme, layout_scheme, input_state, offset, instance_name },
        m_font { font ? font : default_font },
        m_text { text }
    {}

    void on_draw() override
    {
        al_draw_textf(
            static_cast<ALLEGRO_FONT*>(m_font),
            dick_to_platform_color(t_color_scheme->text_regular),
            t_offset.x,
            t_offset.y,
            0,
            "%s",
            m_text.c_str());
    }

    DimScreen get_size() const override
    {
        return DimScreen {
            static_cast<double>(
                al_get_text_width(
                    static_cast<ALLEGRO_FONT*>(m_font),
                    m_text.c_str()
                )
            ),
            static_cast<double>(
                al_get_font_line_height(
                    static_cast<ALLEGRO_FONT*>(m_font)
                )
            )
        };
    }

    const std::string &get_type_name() const override
    {
        static std::string name = "label";
        return name;
    }
};

struct WidgetButton : public GUI::Widget {

    DimScreen m_size; // Size is used every frame therefore we cache it
    std::unique_ptr<Widget> m_sub_widget;
    GUI::Callback m_callback;

    void m_compute_sub_offset()
    {
        DimScreen middle = t_offset;
        middle.x += m_size.x / 2;
        middle.y += m_size.y / 2;
        m_sub_widget->align(middle, GUI::Alignment::MIDDLE | GUI::Alignment::CENTER);
    }

    WidgetButton(
            void *default_font,
            const std::shared_ptr<GUI::ColorScheme>& color_scheme,
            const std::shared_ptr<GUI::LayoutScheme>& layout_scheme,
            const std::shared_ptr<InputState>& input_state,
            std::unique_ptr<GUI::Widget> sub_widget,
            GUI::Callback callback,
            const DimScreen& size,
            const DimScreen& offset,
            const std::string &instance_name) :
        Widget { default_font, color_scheme, layout_scheme, input_state, offset, instance_name },
        m_size(size),
        m_sub_widget { std::move(sub_widget) },
        m_callback { callback }
    {
        if (size.x == 0 && size.y == 0) {
            const DimScreen& sub_size = m_sub_widget->get_size();
            m_size.x = sub_size.x + 2 * layout_scheme->widget_padding.x;
            m_size.y = sub_size.y + 2 * layout_scheme->widget_padding.y;
        }
        m_compute_sub_offset();
    }

    void on_click(Button button) override
    {
        if (button == Button::BUTTON_1 && point_in(t_input_state->cursor)) {
            m_callback();
        }
    }

    void on_draw() override
    {
        DimScreen top_left, bottom_right;
        std::tie(top_left, bottom_right) = get_rect();

        double x0 = top_left.x;
        double y0 = top_left.y;
        double x1 = bottom_right.x;
        double y1 = bottom_right.y;

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

        m_sub_widget->on_draw();
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

    const std::string &get_type_name() const override
    {
        static std::string name = "button";
        return name;
    }
};

struct WidgetButtonImage : public GUI::Widget {

    GUI::Callback m_callback;
    void *m_image;

    WidgetButtonImage(
            void *default_font,
            const std::shared_ptr<GUI::ColorScheme>& color_scheme,
            const std::shared_ptr<GUI::LayoutScheme>& layout_scheme,
            const std::shared_ptr<InputState>& input_state,
            void *image,
            GUI::Callback callback,
            const DimScreen& offset,
            const std::string &instance_name) :
        Widget { default_font, color_scheme, layout_scheme, input_state, offset, instance_name },
        m_callback { callback },
        m_image { image }
    {
    }

    void on_click(Button button) override
    {
        if (button == Button::BUTTON_1 && point_in(t_input_state->cursor)) {
            m_callback();
        }
    }

    void on_draw() override
    {
        al_draw_bitmap(static_cast<ALLEGRO_BITMAP*>(m_image), t_offset.x, t_offset.y, 0);
    }

    DimScreen get_size() const override
    {
        return image_size(m_image);
    }

    const std::string &get_type_name() const override
    {
        static std::string name = "button-image";
        return name;
    }
};

bool GUI::WidgetContainer::contains(Widget* widget)
{
    bool found = false;
    visit_descendants([widget, &found](Widget& x) { if (!found && widget == &x) { found = true; } });
    return found;
}

void GUI::WidgetContainer::on_click(Button button)
{
    visit_children([button](Widget& widget) { widget.on_click(button); });
}

void GUI::WidgetContainer::on_draw()
{
    visit_children([](Widget& widget) { widget.on_draw(); });
}

void GUI::WidgetContainer::set_offset(const DimScreen& offset)
{
    const DimScreen& old_offset = get_offset();
    double dx = offset.x - old_offset.x;
    double dy = offset.y - old_offset.y;
    visit_children(
        [&offset, dx, dy](Widget& widget)
        {
            const DimScreen& widget_offset = widget.get_offset();
            widget.set_offset({ widget_offset.x + dx, widget_offset.y + dy });
        });
    t_offset = offset;
}

std::pair<DimScreen, DimScreen> GUI::WidgetContainer::get_rect() const
{
    double min_x = std::numeric_limits<double>::infinity();
    double max_x = -std::numeric_limits<double>::infinity();
    double min_y = std::numeric_limits<double>::infinity();
    double max_y = -std::numeric_limits<double>::infinity();

    visit_descendants(
        [&min_x, &max_x, &min_y, &max_y](const Widget& widget)
        {
            DimScreen top_left, bottom_right;
            std::tie(top_left, bottom_right) = widget.get_rect();
            min_x = std::min(top_left.x, min_x);
            min_y = std::min(top_left.y, min_y);
            max_x = std::max(bottom_right.x, max_x);
            max_y = std::max(bottom_right.y, max_y);
        });

    return std::make_pair(
        DimScreen { min_x, min_y },
        DimScreen { max_x, max_y }
    );
}

void GUI::WidgetContainer::visit_descendants(std::function<void(Widget&)> callback)
{
    visit_children(
        [callback](Widget& widget)
        {
            callback(widget);
            WidgetContainer *child_as_container = dynamic_cast<WidgetContainer*>(&widget);
            if (child_as_container) {
                child_as_container->visit_descendants(callback);
            }
        });
}

void GUI::WidgetContainer::visit_descendants(std::function<void(const Widget&)> callback) const
{
    visit_children(
        [callback](const Widget& widget)
        {
            callback(widget);
            const WidgetContainer *child_as_container = dynamic_cast<const WidgetContainer*>(&widget);
            if (child_as_container) {
                child_as_container->visit_descendants(callback);
            }
        });
}

struct WidgetContainerFree : public GUI::WidgetContainer {

    std::vector<std::unique_ptr<Widget>> m_children;

    WidgetContainerFree(
            void *default_font,
            const std::shared_ptr<GUI::ColorScheme>& color_scheme,
            const std::shared_ptr<GUI::LayoutScheme>& layout_scheme,
            const std::shared_ptr<InputState>& input_state,
            const DimScreen& offset,
            const std::string &instance_name) :
        WidgetContainer { default_font, color_scheme, layout_scheme, input_state, offset, instance_name }
    {
    }

    bool point_in(const DimScreen&) const override
    {
        return true;
    }

    void insert(std::unique_ptr<Widget> widget, int) override
    {
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

    void clear() override
    {
        m_children.clear();
    }

    void visit_children(std::function<void(Widget&)> callback) override
    {
        for (const std::unique_ptr<Widget>& widget : m_children) {
            callback(*widget.get());
        }
    }

    void visit_children(std::function<void(const Widget&)> callback) const override
    {
        for (const std::unique_ptr<Widget>& widget : m_children) {
            callback(*widget.get());
        }
    }

    const std::string &get_type_name() const override
    {
        static std::string name = "container-free";
        return name;
    }
};

struct WidgetContainerPanel : public GUI::WidgetContainer {

    DimScreen m_size; // Cache as expensive to compute
    std::vector<std::unique_ptr<Widget>> m_children;

    void m_compute_size()
    {
        DimScreen top_left, bottom_right;
        std::tie(top_left, bottom_right) = get_rect();
        m_size = DimScreen { bottom_right.x - top_left.x, bottom_right.y - top_left.y };
    }

    WidgetContainerPanel (
            void *default_font,
            const std::shared_ptr<GUI::ColorScheme>& color_scheme,
            const std::shared_ptr<GUI::LayoutScheme>& layout_scheme,
            const std::shared_ptr<InputState>& input_state,
            const DimScreen& offset,
            const std::string& instance_name) :
        WidgetContainer { default_font, color_scheme, layout_scheme, input_state, offset, instance_name },
        m_size(layout_scheme->widget_padding)
    {
    }

    void on_draw() override
    {
        DimScreen top_left, bottom_right;
        std::tie(top_left, bottom_right) = get_rect();

        double x0 = top_left.x;
        double y0 = top_left.y;
        double x1 = bottom_right.x;
        double y1 = bottom_right.y;

        ALLEGRO_COLOR bg_color = dick_to_platform_color(t_color_scheme->bg_regular);
        ALLEGRO_COLOR border_color = dick_to_platform_color(t_color_scheme->border_regular);

        al_draw_filled_rectangle(x0, y0, x1, y1, bg_color);
        al_draw_rectangle(x0, y0, x1, y1, border_color, t_layout_scheme->border_width);

        visit_children([](Widget& child) { child.on_draw(); });
    }

    void insert(std::unique_ptr<Widget> widget, int alignment) override
    {
        widget->align(t_offset, alignment);
        m_children.push_back(std::move(widget));
        m_compute_size();
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
            m_compute_size();
        }
    }

    void clear() override
    {
        m_children.clear();
    }

    std::pair<DimScreen, DimScreen> get_rect() const override
    {
        DimScreen top_left, bottom_right;
        std::tie(top_left, bottom_right) = WidgetContainer::get_rect();

        return std::make_pair(
            DimScreen {
                top_left.x - t_layout_scheme->widget_padding.x,
                top_left.y - t_layout_scheme->widget_padding.y
            },
            DimScreen {
                bottom_right.x + t_layout_scheme->widget_padding.x,
                bottom_right.y + t_layout_scheme->widget_padding.y
            }
        );
    }

    void visit_children(std::function<void(Widget&)> callback) override
    {
        for (const std::unique_ptr<Widget>& widget : m_children) {
            callback(*widget.get());
        }
    }

    void visit_children(std::function<void(const Widget&)> callback) const override
    {
        for (const std::unique_ptr<Widget>& widget : m_children) {
            callback(*widget.get());
        }
    }

    const std::string &get_type_name() const override
    {
        static std::string name = "container-panel";
        return name;
    }
};

struct WidgetContainerRail : public GUI::WidgetContainer {

    DimScreen m_current_offset;
    GUI::Direction::Enum m_direction;
    double m_stride;
    std::vector<std::unique_ptr<Widget>> m_children;

    void m_advance_offset()
    {
        switch (m_direction) {
        case GUI::Direction::UP:
            m_current_offset.y -= m_stride;
            break;
        case GUI::Direction::RIGHT:
            m_current_offset.x += m_stride;
            break;
        case GUI::Direction::DOWN:
            m_current_offset.y += m_stride;
            break;
        case GUI::Direction::LEFT:
            m_current_offset.x -= m_stride;
            break;
        }
    }

    WidgetContainerRail(
            void *default_font,
            const std::shared_ptr<GUI::ColorScheme>& color_scheme,
            const std::shared_ptr<GUI::LayoutScheme>& layout_scheme,
            const std::shared_ptr<InputState>& input_state,
            GUI::Direction::Enum direction,
            double stride,
            const DimScreen& offset,
            const std::string& instance_name) :
        WidgetContainer { default_font, color_scheme, layout_scheme, input_state, offset, instance_name },
        m_current_offset(offset),
        m_direction { direction },
        m_stride { stride }
    {
    }

    bool point_in(const DimScreen&) const override
    {
        return true;
    }

    void insert(std::unique_ptr<Widget> widget, int alignment) override
    {
        widget->align(m_current_offset, alignment);
        m_children.push_back(std::move(widget));
        m_advance_offset();
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

    void clear() override
    {
        m_children.clear();
        m_current_offset = t_offset;
    }

    void visit_children(std::function<void(Widget&)> callback) override
    {
        for (const std::unique_ptr<Widget>& widget : m_children) {
            callback(*widget.get());
        }
    }

    void visit_children(std::function<void(const Widget&)> callback) const override
    {
        for (const std::unique_ptr<Widget>& widget : m_children) {
            callback(*widget.get());
        }
    }

    const std::string &get_type_name() const override
    {
        static std::string name = "container-rail";
        return name;
    }
};

struct WidgetContainerBox: public GUI::WidgetContainer {

    DimScreen m_current_offset;
    GUI::Direction::Enum m_direction;
    double m_spacing;
    std::vector<std::unique_ptr<Widget>> m_children;

    void m_advance_offset(const std::unique_ptr<Widget>& child)
    {
        DimScreen size = child->get_size();
        switch (m_direction) {
        case GUI::Direction::UP:
            m_current_offset.y -= size.y + m_spacing;
            break;
        case GUI::Direction::RIGHT:
            m_current_offset.x += size.x + m_spacing;
            break;
        case GUI::Direction::DOWN:
            m_current_offset.y += size.y + m_spacing;
            break;
        case GUI::Direction::LEFT:
            m_current_offset.x -= size.x + m_spacing;
            break;
        }
    }

    WidgetContainerBox(
            void *default_font,
            const std::shared_ptr<GUI::ColorScheme>& color_scheme,
            const std::shared_ptr<GUI::LayoutScheme>& layout_scheme,
            const std::shared_ptr<InputState>& input_state,
            GUI::Direction::Enum direction,
            double spacing,
            const DimScreen& offset,
            const std::string& instance_name) :
        WidgetContainer { default_font, color_scheme, layout_scheme, input_state, offset, instance_name },
        m_current_offset(offset),
        m_direction { direction },
        m_spacing { spacing }
    {
    }

    void insert(std::unique_ptr<Widget> widget, int) override
    {
        widget->align(m_current_offset, GUI::Alignment::TOP | GUI::Alignment::LEFT);
        m_advance_offset(widget);
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

    void clear() override
    {
        m_children.clear();
        m_current_offset = t_offset;
    }

    void visit_children(std::function<void(Widget&)> callback) override
    {
        for (const std::unique_ptr<Widget>& widget : m_children) {
            callback(*widget.get());
        }
    }

    void visit_children(std::function<void(const Widget&)> callback) const override
    {
        for (const std::unique_ptr<Widget>& widget : m_children) {
            callback(*widget.get());
        }
    }

    const std::string &get_type_name() const override
    {
        static std::string name = "container-box";
        return name;
    }
};

struct GUIImpl {

    void *m_default_font;
    std::shared_ptr<GUI::ColorScheme> m_color_scheme;
    std::shared_ptr<GUI::LayoutScheme> m_layout_scheme;
    std::shared_ptr<InputState> m_input_state;
    std::string m_default_instance_name;

    GUIImpl(
            const std::shared_ptr<InputState>& input_state,
            Resources& resources) :
        m_default_font {
            resources.get_font("gui_default.ttf", 20)
        },
        m_color_scheme {
            new GUI::ColorScheme {
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
            new GUI::LayoutScheme {
                1.0,
                { 10.0, 8.0 },
                { 13.0, 10.0 }
            }
        },
        m_input_state { input_state },
        m_default_instance_name { "unnamed" }
    {
    }

    std::unique_ptr<GUI::Widget> make_image(
            void *image,
            const DimScreen& offset)
    {
        std::unique_ptr<GUI::Widget> result {
            new WidgetImage {
                m_default_font,
                m_color_scheme,
                m_layout_scheme,
                m_input_state,
                image,
                offset,
                m_default_instance_name
            }
        };
        return result;
    }

    std::unique_ptr<GUI::Widget> make_label(
            const std::string& text,
            const DimScreen& offset)
    {
        std::unique_ptr<GUI::Widget> result {
            new WidgetLabel {
                m_default_font,
                m_color_scheme,
                m_layout_scheme,
                m_input_state,
                text,
                nullptr,
                offset,
                m_default_instance_name
            }
        };
        return result;
    }

    std::unique_ptr<GUI::Widget> make_label_ex(
            const std::string& text,
            void *font,
            const DimScreen& offset = { 0, 0 })
    {
        std::unique_ptr<GUI::Widget> result {
            new WidgetLabel {
                m_default_font,
                m_color_scheme,
                m_layout_scheme,
                m_input_state,
                text,
                font,
                offset,
                m_default_instance_name
            }
        };
        return result;
    }

    std::unique_ptr<GUI::Widget> make_button(
            std::unique_ptr<GUI::Widget> sub_widget,
            GUI::Callback callback,
            const DimScreen& offset)
    {
        std::unique_ptr<GUI::Widget> result {
            new WidgetButton {
                m_default_font,
                m_color_scheme,
                m_layout_scheme,
                m_input_state,
                std::move(sub_widget),
                callback,
                { 0, 0 },
                offset,
                m_default_instance_name
            }
        };
        return result;
    }

    std::unique_ptr<GUI::Widget> make_button_sized(
            std::unique_ptr<GUI::Widget> sub_widget,
            GUI::Callback callback,
            const DimScreen& size,
            const DimScreen& offset)
    {
        std::unique_ptr<GUI::Widget> result {
            new WidgetButton {
                m_default_font,
                m_color_scheme,
                m_layout_scheme,
                m_input_state,
                std::move(sub_widget),
                callback,
                size,
                offset,
                m_default_instance_name
            }
        };
        return result;
    }

    std::unique_ptr<GUI::Widget> make_button_image(
            void* image,
            GUI::Callback callback,
            const DimScreen& offset)
    {
        std::unique_ptr<GUI::Widget> result {
            new WidgetButtonImage {
                m_default_font,
                m_color_scheme,
                m_layout_scheme,
                m_input_state,
                image,
                callback,
                offset,
                m_default_instance_name
            }
        };
        return result;
    }

    std::unique_ptr<GUI::WidgetContainer> make_container_free(
            const DimScreen& offset)
    {
        std::unique_ptr<GUI::WidgetContainer> result {
            new WidgetContainerFree {
                m_default_font,
                m_color_scheme,
                m_layout_scheme,
                m_input_state,
                offset,
                m_default_instance_name
            }
        };
        return result;
    }

    std::unique_ptr<GUI::WidgetContainer> make_container_panel(
            const DimScreen& offset)
    {
        std::unique_ptr<GUI::WidgetContainer> result {
            new WidgetContainerPanel {
                m_default_font,
                m_color_scheme,
                m_layout_scheme,
                m_input_state,
                offset,
                m_default_instance_name
            }
        };
        return result;
    }

    std::unique_ptr<GUI::WidgetContainer> make_container_rail(
            GUI::Direction::Enum direction,
            double stride,
            const DimScreen& offset)
    {
        std::unique_ptr<GUI::WidgetContainer> result {
            new WidgetContainerRail {
                m_default_font,
                m_color_scheme,
                m_layout_scheme,
                m_input_state,
                direction,
                stride,
                offset,
                m_default_instance_name
            }
        };
        return result;
    }

    std::unique_ptr<GUI::WidgetContainer> make_container_box(
            GUI::Direction::Enum direction,
            double spacing,
            const DimScreen& offset)
    {
        std::unique_ptr<GUI::WidgetContainer> result {
            new WidgetContainerBox {
                m_default_font,
                m_color_scheme,
                m_layout_scheme,
                m_input_state,
                direction,
                spacing,
                offset,
                m_default_instance_name
            }
        };
        return result;
    }
};

GUI::GUI(
        const std::shared_ptr<InputState>& input_state,
        Resources& resources) :
    m_impl { new GUIImpl { input_state, resources } }
{
}

GUI::~GUI()
{
    delete m_impl;
}

std::unique_ptr<GUI::Widget> GUI::make_image(
        void *image,
        const DimScreen& offset)
{
    return m_impl->make_image(image, offset);
}

std::unique_ptr<GUI::Widget> GUI::make_label(
        const std::string& text,
        const DimScreen& offset)
{
    return m_impl->make_label(text, offset);
}

std::unique_ptr<GUI::Widget> GUI::make_label_ex(
        const std::string& text,
        void *font,
        const DimScreen& offset)
{
    return m_impl->make_label_ex(text, font, offset);
}

std::unique_ptr<GUI::Widget> GUI::make_button(
        std::unique_ptr<GUI::Widget> sub_widget,
        Callback callback,
        const DimScreen& offset)
{
    return m_impl->make_button(std::move(sub_widget), callback, offset);
}

std::unique_ptr<GUI::Widget> GUI::make_button_sized(
        std::unique_ptr<GUI::Widget> sub_widget,
        Callback callback,
        const DimScreen& size,
        const DimScreen& offset)
{
    return m_impl->make_button_sized(std::move(sub_widget), callback, size, offset);
}

std::unique_ptr<GUI::Widget> GUI::make_button_image(
        void* image,
        Callback callback,
        const DimScreen& offset)
{
    return m_impl->make_button_image(image, callback, offset);
}

std::unique_ptr<GUI::Widget> GUI::make_dialog_yes_no(
        const std::string& question,
        Callback on_yes,
        Callback on_no,
        const DimScreen& offset)
{

    auto yes_button = make_button(make_label("Yes"), on_yes);
    yes_button->set_instance_name("btn-yes");

    auto no_button = make_button(make_label("No"), on_no);
    no_button->set_instance_name("btn-no");

    auto question_label = make_label(question);
    question_label->set_instance_name("lbl-question");

    double central_stride = question_label->get_size().y + m_impl->m_layout_scheme->dialog_spacing.y;

    auto yes_no_box = make_container_box(
            GUI::Direction::RIGHT,
            m_impl->m_layout_scheme->dialog_spacing.x);
    yes_no_box->insert(std::move(yes_button));
    yes_no_box->insert(std::move(no_button));
    yes_no_box->set_instance_name("rail-yes-no");

    auto central_rail = make_container_rail(GUI::Direction::DOWN, central_stride);
    central_rail->insert(
            std::move(question_label),
            GUI::Alignment::TOP | GUI::Alignment::CENTER);
    central_rail->insert(
            std::move(yes_no_box),
            GUI::Alignment::TOP | GUI::Alignment::CENTER);
    central_rail->set_instance_name("rail-central");

    auto panel = make_container_panel(offset);
    panel->insert(std::move(central_rail));

    std::unique_ptr<GUI::Widget> result = std::move(panel);

    return result;
}

std::unique_ptr<GUI::Widget> GUI::make_dialog_ok(
        const std::string& message,
        Callback on_ok,
        const DimScreen& offset)
{

    auto button = make_button(make_label("OK"), on_ok);
    button->set_instance_name("btn-ok");

    auto message_label = make_label(message);
    message_label->set_instance_name("lbl-message");

    double central_stride = message_label->get_size().y + m_impl->m_layout_scheme->dialog_spacing.y;
    auto central_rail = make_container_rail(GUI::Direction::DOWN, central_stride);
    central_rail->insert(
            std::move(message_label),
            GUI::Alignment::TOP | GUI::Alignment::CENTER);
    central_rail->insert(
            std::move(button),
            GUI::Alignment::TOP | GUI::Alignment::CENTER);
    central_rail->set_instance_name("rail-central");

    auto panel = make_container_panel(offset);
    panel->insert(std::move(central_rail));

    std::unique_ptr<GUI::Widget> result = std::move(panel);

    return result;
}

std::unique_ptr<GUI::WidgetContainer> GUI::make_container_free(
        const DimScreen& offset)
{
    return m_impl->make_container_free(offset);
}

std::unique_ptr<GUI::WidgetContainer> GUI::make_container_panel(
        const DimScreen& offset)
{
    return m_impl->make_container_panel(offset);
}

std::unique_ptr<GUI::WidgetContainer> GUI::make_container_rail(
        Direction::Enum direction,
        double stride,
        const DimScreen& offset)
{
    return m_impl->make_container_rail(direction, stride, offset);
}

std::unique_ptr<GUI::WidgetContainer> GUI::make_container_box(
        Direction::Enum direction,
        double spacing,
        const DimScreen& offset)
{
    return m_impl->make_container_box(direction, spacing, offset);
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
