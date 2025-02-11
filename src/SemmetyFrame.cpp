#include "globals.hpp"
#include "SemmetyFrame.hpp"


SP<SemmetyParentFrame> SemmetyFrame::get_parent() const {
    return parent.lock();
}


void SemmetyFrame::print() const {
    if (is_window()) {
        // std::cout << "SemmetyFrame (WindowId: " << std::get<Window>(data).window << ")\n";
    } else if (is_empty()) {
        // std::cout << "SemmetyFrame (Empty)\n";
    } else {
        // std::cout << "SemmetyFrame (Parent with children)\n";
        for (const auto& child : std::get<SemmetyFrame::Parent>(data).children) {
            child->print();
        }
    }
}



