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

    class Hy3NodeData {
    public:
        Hy3NodeData() = default;
        Hy3NodeData(Parent parent) : data(std::move(parent)) {}
        Hy3NodeData(PHLWINDOWREF window) : data(window) {}
        Hy3NodeData(Hy3GroupLayout layout) : data(Parent{}) {}
        Hy3NodeData(Hy3NodeData&& other) noexcept : data(std::move(other.data)) {}
        ~Hy3NodeData() = default;

        Hy3NodeData& operator=(PHLWINDOWREF window) {
            data = window;
            return *this;
        }

        Hy3NodeData& operator=(Hy3GroupLayout layout) {
            data = Parent{};
            return *this;
        }

        Hy3NodeData& operator=(Hy3NodeData&& other) noexcept {
            data = std::move(other.data);
            return *this;
        }

        bool operator==(const Hy3NodeData& other) const {
            return data == other.data;
        }

        bool valid() const {
            return !std::holds_alternative<Empty>(data);
        }

        bool is_window() const {
            return std::holds_alternative<Window>(data);
        }

        bool is_group() const {
            return std::holds_alternative<Parent>(data);
        }

        Parent& as_group() {
            return std::get<Parent>(data);
        }

        PHLWINDOWREF as_window() {
            return std::get<Window>(data).window;
        }

    private:
        std::variant<Empty, Window, Parent> data;
    };

    Hy3NodeData data;

    FrameData data;
    WP<SemmetyFrame> parent;

    SemmetyFrame() : data(Empty{}) {}

    bool is_window() const { return std::holds_alternative<Window>(data); }
    bool is_empty() const { return std::holds_alternative<Empty>(data); }
    bool is_leaf() const { return is_empty() || is_window(); }

    void setWindow(PHLWINDOWREF window);
    void clearWindow();
    void print() const;

    SP<SemmetyFrame> get_parent() const;
    Vector2D position;
    Vector2D size;
    Vector2D gap_topleft_offset;
    Vector2D gap_bottomright_offset;
};
