#pragma once

#include <memory>
#include <list>
#include <variant>

enum class SemmetyGroupLayout {
	SplitH,
	SplitV,
	Tabbed,
};

class SemmetyParentFrame;

class SemmetyFrame {
public:
    struct Empty {};
    struct Window {
        PHLWINDOWREF window;
    };
    struct Parent {
        std::list<SP<SemmetyFrame>> children;
    };

    using FrameData = std::variant<Empty, Window, Parent>;

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
