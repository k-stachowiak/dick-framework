// Copyright (C) 2015 Krzysztof Stachowiak
// For the license (GPL2) details see the LICENSE file

#ifndef DICK_H
#define DICK_H

#include <vector>
#include <memory>
#include <limits>
#include <stdexcept>
#include <functional>

namespace dick {

// Helper types
// ============

struct Error : public std::runtime_error {
    Error(const std::string &message) :
        std::runtime_error { message }
    {}
};

struct DimScreen {
    double x, y;
};

struct Color {
    double r, g, b;
};

// Logging facilities
// ==================

#if DICK_LOG > 0
#   include <cstdio>
#   define LOG_MESSAGE(LOG_LEVEL, LOG_FORMAT, ...) \
        printf( \
            "[" LOG_LEVEL "][%s] %s:%d : " LOG_FORMAT "\n", \
            __func__, __FILE__, __LINE__, ##__VA_ARGS__)
#   define LOG_ERROR(LOG_FORMAT, ...) LOG_MESSAGE("ERROR", LOG_FORMAT, ##__VA_ARGS__)
#   if DICK_LOG > 1
#       define LOG_WARNING(LOG_FORMAT, ...) LOG_MESSAGE("WARNING", LOG_FORMAT, ##__VA_ARGS__)
#       if DICK_LOG > 2
#           define LOG_DEBUG(LOG_FORMAT, ...) LOG_MESSAGE("DEBUG", LOG_FORMAT, ##__VA_ARGS__)
#           if DICK_LOG > 3
#               define LOG_TRACE(LOG_FORMAT, ...) LOG_MESSAGE("TRACE", LOG_FORMAT, ##__VA_ARGS__)
#           else
#               define LOG_TRACE(...)
#           endif
#       else
#           define LOG_DEBUG(...)
#       endif
#   else
#       define LOG_WARNING(...)
#   endif
#else
#   define LOG_TRACE(...)
#endif

// Resources management
// ====================

class ResourcesImpl;

struct Resources {
    // This API is designed to enable lazy loading of assets. The Resources
    // obects may form a tree so that the short living resources can be
    // allocated deeper in the hierarchy and be easily disposed when no
    // longer needed by simply deleting the particular node. It is also
    // possible to only have a single instance of a resource object in a
    // program.
    //
    // Upon resource request a given instance will look up its own resources
    // and only reach to the parent instance if not found. If not found in
    // the up-stream path then the given instance will attempt loading the
    // resource from the file system.
    //
    // Note that no caching is performed in the parent instances. It may
    // be only done in the instance which handles the resource request.
    //
    // The returned values are type-erased pointer to a framework speciffic
    // resource pointers.

    ResourcesImpl *m_impl;
    Resources(Resources *parent = nullptr, const std::string &path_prefix = {});
    ~Resources();
    void *get_image(const std::string &path);
    void *get_font(const std::string &path, int size);
};

// Input management
// ================

enum class Key {
    UP,
    DOWN,
    LEFT,
    RIGHT,

    ESCAPE,
    SPACE,
    ENTER,
    BACKSPACE,
    TAB,

    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,

    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,

    UNHANDLED,

    MAX
};

enum class Button {
    BUTTON_1 = 1,
    BUTTON_2,
    BUTTON_3,

    UNHANDLED,

    MAX
};

class InputState {
    std::vector<bool> m_keys;
    std::vector<bool> m_buttons;

public:
    DimScreen cursor;

    InputState() :
        m_keys(static_cast<int>(Key::MAX), false),
        m_buttons(static_cast<int>(Button::MAX), false),
        cursor { 0, 0 }
    {}

    void on_key(Key key, bool down) { m_keys[static_cast<int>(key)] = down; }
    void on_button(Button button, bool down) { m_buttons[static_cast<int>(button)] = down; }
    void on_cursor(DimScreen position) { cursor = position; }
    bool keys(Key key) const { return m_keys[static_cast<int>(key)]; }
    bool buttons(Button button) const { return m_buttons[static_cast<int>(button)]; }
};

// State interface definition
// ==========================

// Platform client is an object that may be plugged into the platform and
// respond to its stimuli while performing internal operation as time
// passes. There are two implementations of the platform client below.
struct PlatformClient {
    virtual ~PlatformClient() {}
    virtual bool is_over() const = 0;
    virtual void on_key(Key key, bool down) = 0;
    virtual void on_button(Button button, bool down) = 0;
    virtual void on_cursor(DimScreen position) = 0;
    virtual void tick(double dt) = 0;
    virtual void draw(double weight) = 0;
};

// State node is an object that can be plugged in directly to the platform
// object as it implements the PlatformClient interface, but it can also be
// managed by the state machine which is realized by the additional transition
// API.
struct StateNode : public PlatformClient {

    virtual ~StateNode() {}

    // Make all client methods optional
    bool is_over() const override { return t_is_over; }
    virtual void on_key(Key, bool) override {}
    virtual void on_button(Button, bool) override {}
    virtual void on_cursor(DimScreen) override {}
    virtual void tick(double) override {}
    virtual void draw(double) override {}

    // Transition mechanics
    bool transition_required() const { return t_transition_required; }
    virtual std::shared_ptr<StateNode> next_state() { return {}; }

protected:
    bool t_is_over = false;
    bool t_transition_required = false;
};

// An example proxy state which will display the child state wigh an overlay
// fading in or out. The period is the time of the fade. Once the period has
// passed this object will either request switch to the next state if one
// provided. If the next pointer is null, the proxy object will request program
// termination.

std::shared_ptr<StateNode> create_state_fade_in_color(
        std::shared_ptr<StateNode> child,
        std::shared_ptr<StateNode> next,
        double period,
        double red = 0, double green = 0, double blue = 0);

std::shared_ptr<StateNode> create_state_fade_out_color(
        std::shared_ptr<StateNode> child,
        std::shared_ptr<StateNode> next,
        double period,
        double red = 0, double green = 0, double blue = 0);

// State machine is another variant of a PlatformClient object that acts as a
// proxy for the system of underlying states. It is a default client for the
// platform as the StateNode will rarely be useful directly, however it can
// also be used elsewhere as a mechanism for implementing state subspaces
// within a single, more general state.

class StateMachineImpl;

struct StateMachine : public PlatformClient {
    StateMachineImpl *m_impl;
    StateMachine(std::shared_ptr<StateNode> init_state);
    ~StateMachine();

    bool is_over() const override;
    void on_key(Key key, bool down) override;
    void on_button(Button button, bool down) override;
    void on_cursor(DimScreen position) override;
    void tick(double dt) override;
    void draw(double weight) override;
};

// OOP GUI
// =======

struct GUIImpl;

// A type aggregating the GUI state and construction operations. Functions
// as the namespace for the GUI related types and constants as well as the
// type erased GUI elements factory, storing some common configuration shared
// between all the widget types that is silently passed to the concrete
// widget constructors maintaining clear API on the surface.

struct GUI {

    // This is a type of the most generic callback possible. Using the capturing
    // mechanism can render this type very useful. I hope there will be no need
    // for any more fancy callback types.
    typedef std::function<void()> Callback;

    // Bit distinct constants for the directions.
    struct Direction {
        enum Enum {
            UP = 0x1,
            RIGHT = 0x2,
            DOWN = 0x4,
            LEFT = 0x8
        };
    };

    // Bit distinct constants for the basic vertical and horizontal alignment.
    struct Alignment {
        enum Enum {
            RIGHT = 0x1,
            CENTER = 0x2,
            LEFT = 0x4,
            TOP = 0x8,
            MIDDLE = 0x10,
            BOTTOM = 0x20
        };
    };

    // A set of color definitions to be shared between the UI widgets so that they
    // share a common color scheme.
    struct ColorScheme {
        Color bg_regular;
        Color bg_active;
        Color bg_inactive;
        Color border_regular;
        Color border_active;
        Color border_inactive;
        Color text_regular;
        Color text_active;
        Color text_inactive;
    };

    // Constants that define the common GUI scheme that aren't colors.
    struct LayoutScheme {
        double border_width;
        DimScreen widget_padding;
        DimScreen dialog_spacing;
    };

    struct Widget {
    protected:

        // Below are the references to the global resources shared among all the
        // widgets. They define the look and feel as well as provide the
        // information from the outside world:

        void *t_default_font;
        std::shared_ptr<ColorScheme> t_color_scheme;
        std::shared_ptr<LayoutScheme> t_layout_scheme;
        std::shared_ptr<InputState> t_input_state;

        // Common property of all the widgets.

        DimScreen t_offset { 0, 0 };
        std::string t_instance_name;

    public:
        Widget(void *default_font,
               const std::shared_ptr<ColorScheme>& color_scheme,
               const std::shared_ptr<LayoutScheme>& layout_scheme,
               const std::shared_ptr<InputState>& input_state,
               const DimScreen& offset,
               const std::string& instance_name) :
            t_default_font { default_font },
            t_color_scheme { color_scheme },
            t_layout_scheme { layout_scheme },
            t_input_state { input_state },
            t_offset(offset),
            t_instance_name { instance_name }
        {}

        virtual ~Widget() {}

        // Inversion of control for the regular GUI stuff:

        virtual void on_click(Button) {}
        virtual void on_draw() {}
        void debug_draw() const;
        void debug_print(int recursion_level = 0) const;

        // Layout and alignment control via the sizes and offsets.

        const DimScreen& get_offset() const { return t_offset; }
        virtual void set_offset(const DimScreen& offset) { t_offset = offset; }
        virtual std::pair<DimScreen, DimScreen> get_rect() const;
        virtual DimScreen get_size() const;

        // Helper algorithms:

        virtual bool point_in(const DimScreen& point) const;
        static DimScreen align(DimScreen origin, const DimScreen& size, int alignment);

        // Compile-time type inference

        virtual const std::string &get_type_name() const = 0;
        const std::string &get_instance_name() const { return t_instance_name; }
        void set_instance_name(const std::string& name) { t_instance_name = name; }
    };

    // An extension to the Widget concept in the way that it allows for storing
    // other widgets and perform aggregated operations on all of its children.
    struct WidgetContainer : public Widget {

        WidgetContainer(
                void *default_font,
                const std::shared_ptr<ColorScheme>& color_scheme,
                const std::shared_ptr<LayoutScheme>& layout_scheme,
                const std::shared_ptr<InputState>& input_state,
                const DimScreen& offset,
                const std::string& instance_name) :
            Widget { default_font, color_scheme, layout_scheme, input_state, offset, instance_name }
        {}

        virtual ~WidgetContainer() {}

        // The methods added by the WidgetContainer class to the Widget concept:

        virtual void insert(
            std::unique_ptr<Widget> widget,
            int alignment = Alignment::TOP | Alignment::LEFT) = 0;
        virtual void remove(Widget* widget) = 0;
        virtual void clear() = 0;
        virtual bool contains(Widget* widget);

        // Commonly overridden Widget API

        virtual void on_click(Button) override;
        virtual void on_draw() override;
        virtual void set_offset(const DimScreen &offset) override;
        virtual std::pair<DimScreen, DimScreen> get_rect() const override;

        // Helpers assuming the container has the children stored in an iterable
        // range.

        virtual void visit_children(std::function<void(Widget&)> callback) = 0;
        virtual void visit_children(std::function<void(const Widget&)> callback) const = 0;

        void visit_descendants(std::function<void(Widget&)> callback);
        void visit_descendants(std::function<void(const Widget&)> callback) const;
    };

    // The actual GUI implementation

    GUIImpl *m_impl;

    GUI(const std::shared_ptr<InputState>& input_state, Resources& resources);
    ~GUI();

    // Widget constructors

    std::unique_ptr<Widget> make_image(
            void *image,
            const DimScreen& offset = { 0, 0 });

    std::unique_ptr<Widget> make_label(
            const std::string& text,
            const DimScreen& offset = { 0, 0 });

    std::unique_ptr<Widget> make_label_ex(
            const std::string& text,
            void *font,
            const DimScreen& offset = { 0, 0 });

    std::unique_ptr<Widget> make_button(
            std::unique_ptr<Widget> sub_widget,
            Callback callback,
            const DimScreen& offset = { 0, 0 });

    std::unique_ptr<Widget> make_button_sized(
            std::unique_ptr<Widget> sub_widget,
            Callback callback,
            const DimScreen& size,
            const DimScreen& offset = { 0, 0 });

    std::unique_ptr<Widget> make_dialog_yes_no(
            const std::string& question,
            Callback on_yes,
            Callback on_no,
            const DimScreen& offset = { 0, 0 });

    std::unique_ptr<WidgetContainer> make_container_free(
            const DimScreen& offset = { 0, 0 });

    std::unique_ptr<WidgetContainer> make_container_panel(
            const DimScreen& offset = { 0, 0 });

    std::unique_ptr<WidgetContainer> make_container_rail(
            Direction::Enum direction,
            double stride,
            const DimScreen& offset = { 0, 0 });
};

// Core object
// ===========

class PlatformImpl;

struct Platform {
    // It's harder to implement this any simpler way. Provide your client
    // state to the real_time_loop and handle events.

    PlatformImpl *m_impl;
    Platform(const DimScreen &screen_size);
    ~Platform();
    void real_time_loop(PlatformClient &client);
};

}

#endif
