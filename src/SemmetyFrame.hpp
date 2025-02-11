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
    };

    class Window {
    public:
        PHLWINDOWREF window;
        Window(PHLWINDOWREF win) : window(win) {}
    };

    class Parent {
    public:
        std::list<SP<SemmetyFrame>> children;
        Parent() = default;
    };

    class FrameData {
    public:
        FrameData() = default;
        FrameData(Parent parent) : data(std::move(parent)) {}
        FrameData(PHLWINDOWREF window) : data(window) {}
        FrameData(Hy3GroupLayout layout) : data(Parent{}) {}
        FrameData(FrameData&& other) noexcept : data(std::move(other.data)) {}
        ~FrameData() = default;

        FrameData& operator=(PHLWINDOWREF window) {
            data = window;
            return *this;
        }

        FrameData& operator=(Hy3GroupLayout layout) {
            data = Parent{};
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

        Parent& as_group() {
            return std::get<Parent>(data);
        }

        PHLWINDOWREF as_window() {
            return std::get<Window>(data).window;
        }

    private:
        std::variant<Empty, Window, Parent> data;
    };

    FrameData data;

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
