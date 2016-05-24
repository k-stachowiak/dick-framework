// Copyright (C) 2015 Krzysztof Stachowiak
// For the license (GPL2) details see the LICENSE file

#ifndef DICK_H
#define DICK_H

#include <vector>
#include <memory>
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

#ifdef DICK_LOG_ENABLE
#   include <cstdio>
#   define LOG_MESSAGE(LOG_LEVEL, LOG_FORMAT, ...) printf("[" LOG_LEVEL "][%s] %s:%d : " LOG_FORMAT "\n", __func__, __FILE__, __LINE__, ##__VA_ARGS__)
#   define LOG_TRACE(LOG_FORMAT, ...) LOG_MESSAGE("TRACE", LOG_FORMAT, ##__VA_ARGS__)
#   define LOG_DEBUG(LOG_FORMAT, ...) LOG_MESSAGE("DEBUG", LOG_FORMAT, ##__VA_ARGS__)
#   define LOG_WARNING(LOG_FORMAT, ...) LOG_MESSAGE("WARNING", LOG_FORMAT, ##__VA_ARGS__)
#   define LOG_ERROR(LOG_FORMAT, ...) LOG_MESSAGE("ERROR", LOG_FORMAT, ##__VA_ARGS__)
#else
#   define LOG_TRACE(...)
#   define LOG_DEBUG(...)
#   define LOG_WARNING(...)
#   define LOG_ERROR(...)
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

// State capturing and storing input information based on input events
class InputBuffer {
    std::vector<bool> m_keys;
    std::vector<bool> m_buttons;

public:
    DimScreen cursor;

    InputBuffer() :
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

// Immediate mode GUI
// ==================

class GUI {

    const InputBuffer *m_input_buffer;
    double m_screen_width, m_screen_height;
    void *m_current_font;
    std::vector<DimScreen> m_transform_stack;
    std::vector<bool> m_buttons_prev, m_buttons;

    DimScreen m_compute_origin() const;
    bool m_clicked(Button button) const;
    bool m_cursor_in(DimScreen offset, DimScreen size) const;

public:
    typedef std::function<void ()> Callback;

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

    GUI(const InputBuffer* input_buffer, double screen_width, double screen_height) :
        m_input_buffer { input_buffer },
        m_screen_width { screen_width },
        m_screen_height { screen_height },
        m_current_font { nullptr },
        m_buttons_prev(static_cast<int>(Button::MAX), false),
        m_buttons { m_buttons_prev }
    {}

    GUI() = default;
    GUI(const GUI&) = default;
    GUI(GUI&&) = default;
    GUI& operator=(const GUI&) = default;
    GUI& operator=(GUI&&) = default;

    void tick();
    void transform_reset();
    void transform_push_shift(const DimScreen& shift);
    void transform_push_box_align(int alignment, const DimScreen& size);
    void transform_push_screen_align(int alignment);
    void transform_push_again();
    void transform_pop();

    void set_current_font(void *font) { m_current_font = font; }

    void label(
        Color text_color,
        const std::string& text);

    void button_text(
        Color border_color,
        Color bg_color,
        Color text_color,
        DimScreen padding,
        Callback callback,
        const std::string& text);
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
    virtual void on_key(Key key, bool down) override {}
    virtual void on_button(Button button, bool down) override {}
    virtual void on_cursor(DimScreen position) override {}
    virtual void tick(double dt) override {}
    virtual void draw(double weight) override {}

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
