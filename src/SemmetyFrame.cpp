#include "SemmetyFrame.hpp"

#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprutils/math/Box.hpp>

#include "globals.hpp"
#include "src/desktop/Workspace.hpp"

bool SemmetyFrame::validateParentReferences() const {
	if (data.is_parent()) {
		for (const auto& child: data.as_parent().children) {
			auto childParent = child->get_parent();
			if (childParent.get() != this) {
				semmety_log(
				    ERR,
				    "Parent reference mismatch: child {} does not point back to parent {}",
				    child->print(),
				    this->print()
				);
				return false;
			}
			if (!child->validateParentReferences()) {
				return false;
			}
		}
	}
	return true;
}

SP<SemmetyFrame> SemmetyFrame::get_parent() const {
	auto parentFrame = parent.lock();
	if (!parentFrame) {
		return nullptr;
	}
	if (!parentFrame->data.is_parent()) {
		semmety_critical_error("Parent frame is not of type Parent");
	}
	return parentFrame;
}

void SemmetyFrame::propagateGeometry(const std::optional<CBox>& geometry) {
	if (geometry) {
		this->geometry = *geometry;
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
		const auto new_width = static_cast<int>(this->geometry.size().x / 2);

		CBox left_rect(
		    this->geometry.pos(),
		    Vector2D(
		        static_cast<double>(new_width + this->child0Offset),
		        static_cast<double>(this->geometry.size().y)
		    )
		);
		CBox right_rect(
		    Vector2D(
		        this->geometry.pos().x + new_width + this->gap_topleft_offset.x,
		        this->geometry.pos().y
		    ),
		    Vector2D(
		        static_cast<double>(new_width - this->child0Offset),
		        static_cast<double>(this->geometry.size().y)
		    )
		);

		return {left_rect, right_rect};
	}
	case SemmetySplitDirection::SplitH: {
		const auto new_height = static_cast<int>(this->geometry.size().y / 2);

		CBox top_rect(
		    this->geometry.pos(),
		    Vector2D(
		        static_cast<double>(this->geometry.size().x),
		        static_cast<double>(new_height + this->child0Offset)
		    )
		);
		CBox bottom_rect(
		    Vector2D(
		        this->geometry.pos().x,
		        this->geometry.pos().y + new_height + this->gap_topleft_offset.y
		    ),
		    Vector2D(
		        static_cast<double>(this->geometry.size().x),
		        static_cast<double>(new_height - this->child0Offset)
		    )
		);

		return {top_rect, bottom_rect};
	}
	default: {
		semmety_critical_error("Invalid split direction");
	}
	}
}

std::string SemmetyFrame::print(int indentLevel) const {
	std::string indent(indentLevel * 2, ' '); // 2 spaces per indent level
	std::string result;

	auto geometry_str = std::to_string(geometry.pos().x) + ", " + std::to_string(geometry.pos().y)
	                  + ", " + std::to_string(geometry.size().x) + ", "
	                  + std::to_string(geometry.size().y);

	if (data.is_window()) {
		result += indent + "SemmetyFrame (WindowId: " + data.as_window()->m_szTitle + "), "
		        + geometry_str + "\n";
	} else if (data.is_empty()) {
		result += indent + "SemmetyFrame (Empty) " + geometry_str + "\n";
	} else if (data.is_parent()) {
		result += indent + "SemmetyFrame (Parent with "
		        + std::to_string(data.as_parent().children.size()) + " children) " + geometry_str
		        + "\n";
		for (const auto& child: this->data.as_parent().children) {
			result += child->print(indentLevel + 1);
		}
	}

	return result;
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

CBox SemmetyFrame::getStandardWindowArea(CBox area, SBoxExtents extents, PHLWORKSPACE workspace) {
	static const auto p_gaps_in = ConfigValue<Hyprlang::CUSTOMTYPE, CCssGapData>("general:gaps_in");

	auto workspace_rule = g_pConfigManager->getWorkspaceRuleFor(workspace);
	auto gaps_in = workspace_rule.gapsIn.value_or(*p_gaps_in);

	SBoxExtents inner_gap_extents;
	inner_gap_extents.topLeft = {(int) -gaps_in.left, (int) -gaps_in.top};
	inner_gap_extents.bottomRight = {(int) -gaps_in.right, (int) -gaps_in.bottom};

	SBoxExtents combined_outer_extents;
	combined_outer_extents.topLeft = -this->gap_topleft_offset;
	combined_outer_extents.bottomRight = -this->gap_bottomright_offset;

	// auto area = this->geometry;
	area.addExtents(inner_gap_extents);
	area.addExtents(combined_outer_extents);
	area.addExtents(extents);

	area.round();
	return area;
}

void SemmetyFrame::applyRecursive(PHLWORKSPACE workspace) {
	if (this->data.is_empty()) {
		return;
	}

	if (this->data.is_parent()) {
		for (const auto& child: this->data.as_parent().children) {
			child->applyRecursive(workspace);
		}
		return;
	}

	auto window = this->data.as_window();

	if (!valid(window) || !window->m_bIsMapped) {
		semmety_log(
		    ERR,
		    "node {:x} is an unmapped window ({:x}), cannot apply node data, removing from tiled "
		    "layout",
		    (uintptr_t) this,
		    (uintptr_t) window.get()
		);
		// Handle error notification or removal logic here
		return;
	}

	if (window->isHidden()) {
		window->setHidden(false);
	}

	window->unsetWindowData(PRIORITY_LAYOUT);

	auto reserved = window->getFullWindowReservedArea();
	auto wb =
	    getStandardWindowArea(this->geometry, {-reserved.topLeft, -reserved.bottomRight}, workspace);

	*window->m_vRealPosition = wb.pos();
	*window->m_vRealSize = wb.size();
	window->updateWindowDecos();

	window->sendWindowSize(true);
}

// from CHyprBorderDecoration::draw
CBox SemmetyFrame::getEmptyFrameBox(const CMonitor& monitor) {
	static auto PBORDERSIZE = CConfigValue<Hyprlang::INT>("general:border_size");

	const auto borderSize = static_cast<int>(-*PBORDERSIZE);
	const auto borderOffset = Vector2D(borderSize, borderSize);
	const auto borderExtent = SBoxExtents {borderOffset, borderOffset};

	const auto workspace = monitor.activeWorkspace;
	// do we need to worry about m_vRenderOffset being animated if we are getting the frame box for
	// damage?
	// PWINDOWWORKSPACE->m_vRenderOffset->isBeingAnimated()
	const auto workspaceOffset = workspace ? workspace->m_vRenderOffset->value() : Vector2D();

	auto frameBox = this->getStandardWindowArea(this->geometry, borderExtent, workspace);

	return frameBox.translate(-monitor.vecPosition + workspaceOffset).scale(monitor.scale).round();
}

void SemmetyFrame::damageEmptyFrameBox(const CMonitor& monitor) {
	static auto PROUNDING = CConfigValue<Hyprlang::INT>("decoration:rounding");

	const auto rounding = static_cast<float>(*PROUNDING);
	const auto roundingSize = rounding - M_SQRT1_2 * rounding + 2;

	const auto frameBox = this->getEmptyFrameBox(monitor);

	CBox surfaceBoxShrunkRounding = frameBox;
	surfaceBoxShrunkRounding.expand(-roundingSize);

	CRegion borderRegion(frameBox);
	borderRegion.subtract(surfaceBoxShrunkRounding);

	g_pHyprRenderer->damageRegion(borderRegion);
}
