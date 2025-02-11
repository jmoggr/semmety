#include "globals.hpp"
#include "SemmetyFrame.hpp"


SP<SemmetyParentFrame> SemmetyFrame::get_parent() const {
    return parent.lock();
}

bool SemmetyWindowFrame::is_window() const {
    return true;
}

void SemmetyWindowFrame::print() const {
    // std::cout << "SemmetyWindowFrame (WindowId: " << windowId << ")\n";
}

void SemmetyEmptyFrame::print() const {
    // std::cout << "SemmetyEmptyFrame (children = null)\n";
}

SemmetyParentFrame::SemmetyParentFrame(std::list<SP<SemmetyFrame>> ch) : children(std::move(ch)) {
    for (auto& child : children) {
        child->parent = shared_from_this();
    }
}

void SemmetyParentFrame::print() const {
    // std::cout << "SemmetyParentFrame with " << children.size() << " children\n";
    for (const auto& child : children) {
        child->print();
    }
}



