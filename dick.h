// Copyright (C) 2015 Krzysztof Stachowiak
// For the license (GPL2) details see the LICENSE file

#ifndef DICK_H
#define DICK_H

#include <memory>
#include <stdexcept>

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

// Logging facilities
// ==================

#ifdef DICK_LOG_ENABLE
#       include <cstdio>
#       define LOG_MESSAGE(LOG_LEVEL, LOG_FORMAT, ...) printf("[" LOG_LEVEL "][%s] %s:%d : " LOG_FORMAT "\n", __func__, __FILE__, __LINE__, ##__VA_ARGS__)

#       define LOG_TRACE(LOG_FORMAT, ...) LOG_MESSAGE("TRACE", LOG_FORMAT, ##__VA_ARGS__)
#       define LOG_DEBUG(LOG_FORMAT, ...) LOG_MESSAGE("DEBUG", LOG_FORMAT, ##__VA_ARGS__)
#       define LOG_WARNING(LOG_FORMAT, ...) LOG_MESSAGE("WARNING", LOG_FORMAT, ##__VA_ARGS__)
#       define LOG_ERROR(LOG_FORMAT, ...) LOG_MESSAGE("ERROR", LOG_FORMAT, ##__VA_ARGS__)
#else
#       define LOG_TRACE(...)
#       define LOG_DEBUG(...)
#       define LOG_WARNING(...)
#       define LOG_ERROR(...)
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

// State interface definition
// ==========================

struct StateNode {

        // The is_over method should only be used to signal that the entire program
        // should shut down. For transition between states an appropriate value
        // should be returned from the event handlers.
        //
        // For any method that returns a pointer to a state node, if it returns
        // null, then the current state remains the same, otherwise the loop code
        // will replace the current state with whatever has been returned.

        virtual ~StateNode() {}
        virtual bool is_over() const { return false; }
        virtual std::shared_ptr<StateNode> on_key(int key, bool down) { return {}; }
        virtual std::shared_ptr<StateNode> on_button(int button, bool down) { return {}; }
        virtual std::shared_ptr<StateNode> on_cursor(DimScreen position) { return {}; }
        virtual std::shared_ptr<StateNode> tick(double dt) { return {}; }
        virtual void draw(double weight) {}
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
        void real_time_loop(std::shared_ptr<StateNode> init_state);
};

}

#endif
