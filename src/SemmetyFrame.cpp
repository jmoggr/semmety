#include "globals.hpp"
#include "SemmetyFrame.hpp"


SP<SemmetyFrame> SemmetyFrame::get_parent() const {
    auto parentFrame = parent.lock();
    if (!parentFrame) {
        return nullptr;
    }
    if (!parentFrame->data.is_parent()) {
        throw std::runtime_error("Parent frame is not of type Parent");
    }
    return parentFrame;
}

void SemmetyFrame::propagateGeometry(const std::optional<CBox>& geometry) {
    if (geometry) {
        this->position = geometry->pos();
        this->size = geometry->size();
    }

    if (!this->data.is_parent()) {
        return;
    }

    const auto childGeometries = this->getChildGeometries();
    const auto& children = this->data.as_parent().children;
    children.front()->propagateGeometry(childGeometries.first);
    children.back()->propagateGeometry(childGeometries.second);
}

std::pair<CBox, CBox> SemmetyFrame::getChildGeometries() const {
    switch (this->split_direction) {
        case SemmetySplitDirection::SplitV: {
            const auto new_width = static_cast<int>(this->size.x / 2);

            CBox left_rect(
                this->position,
                Vector2D(new_width + this->child0Offset, this->size.y)
            );
            CBox right_rect(
                Vector2D(this->position.x + new_width + this->gap_topleft_offset.x, this->position.y),
                Vector2D(new_width - this->child0Offset, this->size.y)
            );

            return {left_rect, right_rect};
        }
        case SemmetySplitDirection::SplitH: {
            const auto new_height = static_cast<int>(this->size.y / 2);

            CBox top_rect(
                this->position,
                Vector2D(this->size.x, new_height + this->child0Offset)
            );
            CBox bottom_rect(
                Vector2D(this->position.x, this->position.y + new_height + this->gap_topleft_offset.y),
                Vector2D(this->size.x, new_height - this->child0Offset)
            );

            return {top_rect, bottom_rect};
        }
        default: {
            throw std::runtime_error("Invalid split direction");
        }
    }
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


// void Hy3Node::updateDecos() {
// 	switch (this->data.type()) {
// 	case Hy3NodeType::Window:
// 		g_pCompositor->updateWindowAnimatedDecorationValues(this->data.as_window());
// 		break;
// 	case Hy3NodeType::Group:
// 		for (auto* child: this->data.as_group().children) {
// 			child->updateDecos();
// 		}

// 		this->updateTabBar();
// 	}
// }

CBox SemmetyFrame::getStandardWindowArea(SBoxExtents extents) {
	// static const auto p_gaps_in = ConfigValue<Hyprlang::CUSTOMTYPE, CCssGapData>("general:gaps_in");

	// auto workspace_rule = g_pConfigManager->getWorkspaceRuleFor(this->workspace);
	// auto gaps_in = workspace_rule.gapsIn.value_or(*p_gaps_in);

	// SBoxExtents inner_gap_extents;
	// inner_gap_extents.topLeft = {(int) -gaps_in.left, (int) -gaps_in.top};
	// inner_gap_extents.bottomRight = {(int) -gaps_in.right, (int) -gaps_in.bottom};

	// SBoxExtents combined_outer_extents;
	// combined_outer_extents.topLeft = -this->gap_topleft_offset;
	// combined_outer_extents.bottomRight = -this->gap_bottomright_offset;

	auto area = CBox(this->position, this->size);
	// area.addExtents(inner_gap_extents);
	// area.addExtents(combined_outer_extents);
	area.addExtents(extents);

	area.round();
	return area;
}

void SemmetyFrame::applyRecursive() {
	if (this->data.is_window()) {
		// this->data.as_window()->setHidden(this->hidden);
		this->layout->applyNodeDataToWindow(this, no_animation);
		return;
	}
}
