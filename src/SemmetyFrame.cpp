#include "SemmetyFrame.hpp"
#include <utility>

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
#include "src/config/ConfigDataValues.hpp"
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
		const auto leftWidth = geometry.size().x * splitRatio;
		const auto rightWidth = geometry.size().x * (1 - splitRatio);

		CBox leftRect(geometry.pos(), Vector2D(leftWidth, geometry.size().y));
		CBox rightRect(
		    Vector2D(geometry.pos().x + leftWidth, geometry.pos().y),
		    Vector2D(rightWidth, geometry.size().y)
		);

		return {leftRect.round(), rightRect.round()};
	}
	case SemmetySplitDirection::SplitH: {
		const auto topHeight = geometry.size().y * splitRatio;
		const auto bottomHeight = geometry.size().y * (1 - splitRatio);

		CBox topRect(geometry.pos(), Vector2D(geometry.size().x, topHeight));
		CBox bottomRect(
		    Vector2D(geometry.pos().x, geometry.pos().y + topHeight),
		    Vector2D(geometry.size().x, bottomHeight)
		);

		return {topRect.round(), bottomRect.round()};
	}
	}

	std::unreachable();
}

bool SemmetySplitFrame::isSameOrDescendant(const SP<SemmetyFrame>& target) const {
	if (self == target) {
		return true;
	}

	if (children.first == target || children.second == target) {
		return true;
	}

	if (children.first->isSplit()) {
		if (children.first->asSplit()->isSameOrDescendant(target)) {
			return true;
		}
	}

	if (children.second->isSplit()) {
		if (children.second->asSplit()->isSameOrDescendant(target)) {
			return true;
		}
	}

	return false;
}

std::optional<size_t> SemmetySplitFrame::pathLengthToDescendant(const SP<SemmetyFrame>& target
) const {
	if (self == target) {
		return 0;
	}

	if (children.first == target || children.second == target) {
		return 1;
	}

	if (children.first->isSplit()) {
		if (auto d = children.first->asSplit()->pathLengthToDescendant(target)) {
			return *d + 1;
		}
	}

	if (children.second->isSplit()) {
		if (auto d = children.second->asSplit()->pathLengthToDescendant(target)) {
			return *d + 1;
		}
	}

	return std::nullopt;
}

void SemmetySplitFrame::resize(double distance) {
	const bool isVertical = splitDirection == SemmetySplitDirection::SplitV;
	const auto firstChildSize = children.first->geometry.size();
	const double baseSize = isVertical ? firstChildSize.x : firstChildSize.y;
	const double totalSize = isVertical ? geometry.size().x : geometry.size().y;

	const double rawRatio = (baseSize + distance) / totalSize;
	splitRatio = std::clamp(rawRatio, 0.1, 0.9);
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

SP<SemmetyLeafFrame> SemmetyLeafFrame::create(PHLWINDOWREF window, std::optional<bool> isActive) {
	auto ptr = makeShared<SemmetyLeafFrame>(window, isActive);
	ptr->self = ptr;
	return ptr;
}

SemmetyLeafFrame::SemmetyLeafFrame(PHLWINDOWREF window, std::optional<bool> isActive):
    window(window) {
	static auto PINACTIVECOL = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.inactive_border");
	static auto PACTIVECOL = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.active_border");

	g_pAnimationManager->createAnimation(
	    0.f,
	    this->m_fBorderFadeAnimationProgress,
	    g_pConfigManager->getAnimationPropertyConfig("border"),
	    AVARDAMAGE_ENTIRE
	);

	CGradientValueData* color;
	if (isActive.value_or(false)) {
		color = (CGradientValueData*) (PACTIVECOL.ptr())->getData();
	} else {
		color = (CGradientValueData*) (PINACTIVECOL.ptr())->getData();
	}

	m_cRealBorderColor = *color;
}

bool SemmetyLeafFrame::isEmpty() const { return window == nullptr; }

PHLWINDOWREF SemmetyLeafFrame::getWindow() { return window; }

void SemmetyLeafFrame::setWindow(SemmetyWorkspaceWrapper& workspace, PHLWINDOWREF win) {
	if (window) {
		// The caller must handle the case where there is an existing window
		semmety_critical_error("setWindow called on non-empty frame");
	}

	_setWindow(workspace, win, true);
}

PHLWINDOWREF
SemmetyLeafFrame::replaceWindow(SemmetyWorkspaceWrapper& workspace, PHLWINDOWREF win) {
	const auto oldWin = window;
	_setWindow(workspace, win, true);
	return oldWin;
}

void SemmetyLeafFrame::swapContents(
    SemmetyWorkspaceWrapper& workspace,
    SP<SemmetyLeafFrame> other
) {
	// update the Z order of the windows so that they appear over other windows during the swap
	// animation
	if (other->window) {
		g_pCompositor->changeWindowZOrder(other->window.lock(), true);
	}

	if (window) {
		g_pCompositor->changeWindowZOrder(window.lock(), true);
	}

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

bool SemmetyLeafFrame::isSameOrDescendant(const SP<SemmetyFrame>& target) const {
	return self == target;
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
	inner_gap_extents.topLeft = {(int) -gaps_in.m_left, (int) -gaps_in.m_top};
	inner_gap_extents.bottomRight = {(int) -gaps_in.m_right, (int) -gaps_in.m_bottom};

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

	if (!valid(window) || !window->m_isMapped) {
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

	if (window->isFullscreen()) {
		const auto& monitor = window->m_monitor;

		*window->m_realPosition = monitor->m_position;
		*window->m_realSize = monitor->m_size;
	} else {
		geometry.round();

		window->m_size = geometry.size();
		window->m_position = geometry.pos();

		auto reserved = window->getFullWindowReservedArea();
		auto wb = this->getStandardWindowArea(
		    this->geometry,
		    {-reserved.topLeft, -reserved.bottomRight},
		    workspace.workspace.lock()
		);

		*window->m_realSize = wb.size();
		*window->m_realPosition = wb.pos();
	}

	if (force.has_value() && force.value()) {
		g_pHyprRenderer->damageWindow(window.lock());

		window->m_realPosition->warp();
		window->m_realSize->warp();

		g_pHyprRenderer->damageWindow(window.lock());
	}

	window->updateWindowDecos();
}

// from void CCompositor::updateWindowAnimatedDecorationValues(PHLWINDOW pWindow) {
void SemmetyLeafFrame::setBorderColor(CGradientValueData grad) {
	if (grad == m_cRealBorderColor) {
		return;
	}

	m_cRealBorderColorPrevious = m_cRealBorderColor;
	m_cRealBorderColor = grad;
	m_fBorderFadeAnimationProgress->setValueAndWarp(0.f);
	*m_fBorderFadeAnimationProgress = 1.f;
}

// from CHyprBorderDecoration::draw
CBox SemmetyLeafFrame::getEmptyFrameBox(const CMonitor& monitor) {
	static auto PBORDERSIZE = CConfigValue<Hyprlang::INT>("general:border_size");

	const auto borderSize = static_cast<int>(-*PBORDERSIZE);
	const auto borderOffset = Vector2D(borderSize, borderSize);
	const auto borderExtent = SBoxExtents {borderOffset, borderOffset};

	const auto workspace = monitor.m_activeWorkspace;
	// do we need to worry about m_vRenderOffset being animated if we are getting the frame box for
	// damage?
	// PWINDOWWORKSPACE->m_vRenderOffset->isBeingAnimated()
	const auto workspaceOffset = workspace ? workspace->m_renderOffset->value() : Vector2D();

	auto frameBox = this->getStandardWindowArea(this->geometry, borderExtent, workspace);

	return frameBox.translate(-monitor.m_position + workspaceOffset).scale(monitor.m_scale).round();
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
