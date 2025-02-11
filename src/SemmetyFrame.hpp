#pragma once

#include <memory>
#include <list>
#include <variant>
#include <utility> // for std::move

enum class SemmetyGroupLayout {
	SplitH,
	SplitV,
	Tabbed,
};

class SemmetyParentFrame;

class SemmetyFrame {
public:
    class Empty {
    public:
        Empty() = default;
        bool operator==(const Empty&) const {
            return true; // All Empty instances are considered equal
        }
    };

    class Window {
    public:
        PHLWINDOWREF window;
        Window(PHLWINDOWREF win) : window(win) {}
        bool operator==(const Window& other) const {
            return window.get() == other.window.get();
        }
    };

    class Parent {
    public:
        std::list<SP<SemmetyFrame>> children;
        Parent() = default;

        bool operator==(const Parent& other) const {
            return children == other.children;
        }
    };

    class FrameData {
    public:
        FrameData() = default;
        FrameData(Parent parent) : data(std::move(parent)) {}
        FrameData(PHLWINDOWREF window) : data(window) {}
        FrameData(Empty empty) : data(empty) {}
        FrameData(FrameData&& other) noexcept : data(std::move(other.data)) {}
        ~FrameData() = default;

        FrameData& operator=(PHLWINDOWREF window) {
            data = window;
            return *this;
        }

        FrameData& operator=(FrameData&& other) noexcept {
            data = std::move(other.data);
            return *this;
        }

        bool operator==(const FrameData& other) const {
            return data == other.data;
        }

        bool is_window() const { return std::holds_alternative<Window>(data); }
        bool is_empty() const { return std::holds_alternative<Empty>(data); }
        bool is_leaf() const { return is_empty() || is_window(); }
        bool is_parent() const { return std::holds_alternative<Parent>(data); }

        Parent& as_parent() const {
            if (std::holds_alternative<Parent>(data)) {
                return std::get<Parent>(data);
            } else {
                throw std::runtime_error("Attempted to get parent value of a non-parent FrameData");
            }
        }

        PHLWINDOWREF as_window() {
            if (std::holds_alternative<Window>(data)) {
                return std::get<Window>(data).window;
            } else {
                throw std::runtime_error("Attempted to get window value of a non-window FrameData");
            }
        }

    private:
        std::variant<Empty, Window, Parent> data;
    };


    FrameData data;
    WP<SemmetyFrame> parent;

    SemmetyFrame() : data(Empty{}) {}

    void clearWindow();
    void print() const;

    SP<SemmetyFrame> get_parent() const;
    Vector2D position;
    Vector2D size;
    Vector2D gap_topleft_offset;
    Vector2D gap_bottomright_offset;
};
