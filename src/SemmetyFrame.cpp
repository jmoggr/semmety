#include "globals.hpp"
#include "SemmetyFrame.hpp"


SP<SemmetyFrame> SemmetyFrame::get_parent() const {
    auto parentFrame = parent.lock();
    if (!parentFrame) {
        return nullptr;
    }
    if (parentFrame->data.is_parent()) {
        return parentFrame;
    }
    return nullptr;
}


void SemmetyFrame::print() const {
    if (data.is_window()) {
        // std::cout << "SemmetyFrame (WindowId: " << std::get<Window>(data).window << ")\n";
    } else if (data.is_empty()) {
        // std::cout << "SemmetyFrame (Empty)\n";
    } else {
        // std::cout << "SemmetyFrame (Parent with children)\n";
        if (data.is_parent()) {
            for (const auto& child : this->data.as_parent().children) {
                child->print();
            }
        }
    }
}



