#include "SemmetyFrame.hpp"

#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/xwayland/XSurface.hpp>
#include <hyprutils/math/Box.hpp>
#include <hyprutils/memory/SharedPtr.hpp>

#include "SemmetyFrameUtils.hpp"
#include "log.hpp"
#include "utils.hpp"

//
// SemmetyFrame
//

std::vector<SP<SemmetyLeafFrame>> SemmetyFrame::getEmptyFrames() const {
	auto leafFrames = getLeafFrames();
	std::vector<SP<SemmetyLeafFrame>> emptyFrames;

	for (const auto& leaf: leafFrames) {
		if (leaf->isEmpty()) {
			emptyFrames.push_back(leaf);
		}
	}

	return emptyFrames;
}

SP<SemmetySplitFrame> SemmetyFrame::asSplit() const {
	if (!isSplit()) {
		semmety_critical_error("Tried to call asSplit on a frame that is not a split frame");
	}

	auto sp = hyprland_dynamic_pointer_cast<SemmetySplitFrame>(self.lock());
	if (!sp) {
		semmety_critical_error("Failed to cast to split frame");
	}

	return sp;
}

SP<SemmetyLeafFrame> SemmetyFrame::asLeaf() const {
	if (!isLeaf()) {
		semmety_critical_error("Tried to call asLeaf on a frame that is not a leaf frame");
	}

	auto sp = hyprland_dynamic_pointer_cast<SemmetyLeafFrame>(self.lock());
	if (!sp) {
		semmety_critical_error("Failed to cast to leaf frame");
	}

	return sp;
}

SP<SemmetyLeafFrame> SemmetyFrame::getLastFocussedLeaf() const {
	const auto descendantLeafs = getLeafFrames();
	const auto maxFocusOrderLeaf = getMaxFocusOrderLeaf(descendantLeafs);

	// this shouldn't be possible
	if (!maxFocusOrderLeaf) {
		semmety_critical_error("No non parent descendant leafs");
	}

	return maxFocusOrderLeaf;
}

//
// SemmetySplitFrame
//

SP<SemmetySplitFrame> SemmetySplitFrame::create(
    SP<SemmetyFrame> firstChild,
    SP<SemmetyFrame> secondChild,
    CBox _geometry
) {
	auto ptr = makeShared<SemmetySplitFrame>(firstChild, secondChild, _geometry);
	ptr->self = ptr;
	return ptr;
}

SemmetySplitFrame::SemmetySplitFrame(
    SP<SemmetyFrame> firstChild,
    SP<SemmetyFrame> secondChild,
    CBox _geometry
):
    children {firstChild, secondChild} {
	geometry = _geometry;
	if (geometry.width > geometry.height) {
		splitDirection = SemmetySplitDirection::SplitV;
	} else {
		splitDirection = SemmetySplitDirection::SplitH;
	}
}

void SemmetySplitFrame::applyRecursive(
    SemmetyWorkspaceWrapper& workspace,
    std::optional<CBox> newGeometry,
    std::optional<bool> force
) {
	if (newGeometry.has_value()) {
		geometry = newGeometry.value();
	}

	auto childGeometries = getChildGeometries();

	children.first->applyRecursive(workspace, childGeometries.first, force);
	children.second->applyRecursive(workspace, childGeometries.second, force);
}

std::vector<SP<SemmetyLeafFrame>> SemmetySplitFrame::getLeafFrames() const {
	std::vector<SP<SemmetyLeafFrame>> leafFrames;

	auto processChild = [&leafFrames](const SP<SemmetyFrame>& child) {
		auto childLeaves = child->getLeafFrames();
		leafFrames.insert(leafFrames.end(), childLeaves.begin(), childLeaves.end());
	};

	processChild(children.first);
	processChild(children.second);

	return leafFrames;
}

SP<SemmetyFrame> SemmetySplitFrame::getOtherChild(const SP<SemmetyFrame>& child) {
	if (children.first == child) {
		return children.second;
	} else if (children.second == child) {
		return children.first;
	} else {
		semmety_critical_error("Child not found in parent.");
	}
}

std::pair<CBox, CBox> SemmetySplitFrame::getChildGeometries() const {
	switch (this->splitDirection) {
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

std::string SemmetySplitFrame::print(SemmetyWorkspaceWrapper& workspace, int indentLevel) const {
	std::string indent(indentLevel * 2, ' ');
	std::string result;
	std::string geometryString = getGeometryString(geometry);

	result += indent + "SemmetySplitFrame " + geometryString + "\n";
	result += children.first->print(workspace, indentLevel + 1);
	result += children.second->print(workspace, indentLevel + 1);

	return result;
}

bool SemmetySplitFrame::isLeaf() const { return false; }
bool SemmetySplitFrame::isSplit() const { return true; }

//
// SemmetyLeafFrame
//

SP<SemmetyLeafFrame> SemmetyLeafFrame::create(PHLWINDOWREF window) {
	auto ptr = makeShared<SemmetyLeafFrame>(window);
	ptr->self = ptr;
	return ptr;
}
SemmetyLeafFrame::SemmetyLeafFrame(PHLWINDOWREF window): window(window) {}

bool SemmetyLeafFrame::isEmpty() const { return window == nullptr; }

PHLWINDOWREF SemmetyLeafFrame::getWindow() { return window; }

void SemmetyLeafFrame::setWindow(SemmetyWorkspaceWrapper& workspace, PHLWINDOWREF win) {
	if (window) {
		// The caller must handle the case where there is an existing window
		semmety_critical_error("setWindow called on non-empty frame");
	}

	_setWindow(workspace, win, true);
}

PHLWINDOWREF SemmetyLeafFrame::replaceWindow(SemmetyWorkspaceWrapper& workspace, PHLWINDOWREF win) {
	const auto oldWin = window;
	_setWindow(workspace, win, true);
	return oldWin;
}

void SemmetyLeafFrame::swapContents(
    SemmetyWorkspaceWrapper& workspace,
    SP<SemmetyLeafFrame> other
) {
	const auto tmp = window;
	_setWindow(workspace, other->window, false);
	other->_setWindow(workspace, tmp, false);
}

void SemmetyLeafFrame::_setWindow(
    SemmetyWorkspaceWrapper& workspace,
    PHLWINDOWREF win,
    bool force
) {
	window = win;
	applyRecursive(workspace, std::nullopt, force);
	if (window) {
		workspace.updateFrameHistory(self.lock(), window);
	}
}

std::string SemmetyLeafFrame::print(SemmetyWorkspaceWrapper& workspace, int indentLevel) const {
	std::string indent(indentLevel * 2, ' ');
	std::string result;
	std::string geometryString = getGeometryString(geometry);

	const auto isFocus = workspace.getFocusedFrame() == self.lock();
	const auto focusIndicator = isFocus ? " [Focus] " : " ";

	// const auto ptrString =
	//     window ? std::to_string(reinterpret_cast<uintptr_t>(window.lock().get())) : "";

	if (window) {
		result += indent + "SemmetyFrame (WindowId: " + std::format("{:x}", (uintptr_t) window.get())
		        + ")" + focusIndicator + geometryString + "\n";
	} else {
		result += indent + "SemmetyFrame (Empty)" + focusIndicator + geometryString + "\n";
	}

	return result;
}

bool SemmetyLeafFrame::isLeaf() const { return true; }
bool SemmetyLeafFrame::isSplit() const { return false; }

std::vector<SP<SemmetyLeafFrame>> SemmetyLeafFrame::getLeafFrames() const {
	const auto l = asLeaf();

	return {l};
}

CBox SemmetyLeafFrame::getStandardWindowArea(CBox area, SBoxExtents extents, PHLWORKSPACE workspace)
    const {
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

void SemmetyLeafFrame::applyRecursive(
    SemmetyWorkspaceWrapper& workspace,
    std::optional<CBox> newGeometry,
    std::optional<bool> force
) {
	if (newGeometry.has_value()) {
		geometry = newGeometry.value();
	}

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
	window->updateWindowData();
	geometry.round();

	window->m_vSize = geometry.size();
	window->m_vPosition = geometry.pos();

	auto reserved = window->getFullWindowReservedArea();
	auto wb = this->getStandardWindowArea(
	    this->geometry,
	    {-reserved.topLeft, -reserved.bottomRight},
	    workspace.workspace.lock()
	);

	*window->m_vRealSize = wb.size();
	*window->m_vRealPosition = wb.pos();

	if (force.has_value() && force.value()) {
		g_pHyprRenderer->damageWindow(window.lock());

		window->m_vRealPosition->warp();
		window->m_vRealSize->warp();

		g_pHyprRenderer->damageWindow(window.lock());
	}

	window->updateWindowDecos();
}

// from CHyprBorderDecoration::draw
CBox SemmetyLeafFrame::getEmptyFrameBox(const CMonitor& monitor) {
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

void SemmetyLeafFrame::damageEmptyFrameBox(const CMonitor& monitor) {
	static auto PROUNDING = CConfigValue<Hyprlang::INT>("decoration:rounding");

	const auto rounding = static_cast<float>(*PROUNDING);
	const auto roundingSize = rounding - M_SQRT1_2 * rounding + 2;

	const auto frameBox = this->getEmptyFrameBox(monitor);

	CBox surfaceBoxShrunkRounding = frameBox;
	surfaceBoxShrunkRounding.expand(-roundingSize);

	CRegion borderRegion(frameBox);
	borderRegion.subtract(surfaceBoxShrunkRounding);

	// TODO: try to damage less
	// g_pHyprRenderer->damageRegion(borderRegion);
	g_pHyprRenderer->damageRegion(this->geometry);
}
