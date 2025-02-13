#pragma once

#include <memory>
#include <list>
#include <variant>
#include <utility> // for std::move

#include "log.hpp"

enum class SemmetySplitDirection {
	SplitH,
	SplitV,
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

    class FrameData;

    class Parent {
    public:
        std::list<SP<SemmetyFrame>> children;
        // Parent(std::list<SP<SemmetyFrame>> childrenList = {}) : children(std::move(childrenList)) {}

        // Parent(std::list<SemmetyFrame::FrameData> frameDataList) {
        //     for (auto& frameData : frameDataList) {
        //         children.push_back(makeShared<SemmetyFrame>(std::move(frameData)));
        //     }
        // }

        Parent(SP<SemmetyFrame> parentFrame, SemmetyFrame::FrameData&& child_a, SemmetyFrame::FrameData&& child_b) {
            auto firstChild = makeShared<SemmetyFrame>(std::move(child_a));
            auto secondChild = makeShared<SemmetyFrame>(std::move(child_b));
            firstChild->parent = parentFrame;
            secondChild->parent = parentFrame;
            children.push_back(std::move(firstChild));

                        children.push_back(std::move(secondChild));

        }
        

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

        const Parent& as_parent() const {
            if (std::holds_alternative<Parent>(data)) {
                return std::get<Parent>(data);
            } else {
                throw *semmety_critical_error("Attempted to get parent value of a non-parent FrameData");
            }
        }

        PHLWINDOWREF as_window() const {
            if (std::holds_alternative<Window>(data)) {
                return std::get<Window>(data).window;
            } else {
                throw *semmety_critical_error("Attempted to get window value of a non-window FrameData");
            }
        }

    private:
        std::variant<Empty, Window, Parent> data;
    };


    CBox getStandardWindowArea(SBoxExtents extents, PHLWORKSPACE workspace);
    FrameData data;
    WP<SemmetyFrame> parent;

    SemmetyFrame() : data(Empty{}) {}
    SemmetyFrame(Vector2D pos, Vector2D size) : data(Empty{}), geometry(CBox{pos, size}) {}
    SemmetyFrame(FrameData frameData) : data(std::move(frameData)) {}

    void clearWindow();
    std::string print(int indentLevel = 0) const;
    void propagateGeometry(const std::optional<CBox>& geometry = std::nullopt);
    std::pair<CBox, CBox> getChildGeometries() const;
    void applyRecursive(PHLWORKSPACE workspace);

    SemmetySplitDirection split_direction = SemmetySplitDirection::SplitV;

    SP<SemmetyFrame> get_parent() const;
    CBox geometry;
    Vector2D gap_topleft_offset;
    Vector2D gap_bottomright_offset;
    int child0Offset = 0;
};
