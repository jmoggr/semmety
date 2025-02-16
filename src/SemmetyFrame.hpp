#pragma once

#include <list>
#include <utility>
#include <variant>

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprutils/math/Box.hpp>

#include "log.hpp"
using namespace Hyprutils::Math;

enum class SemmetySplitDirection {
	SplitH,
	SplitV,
};

class SemmetyWorkspaceWrapper;

class SemmetyFrame {
public:
	struct Empty {};

	struct Window {
		PHLWINDOWREF window;
	};

	struct Parent {
		std::list<SP<SemmetyFrame>> children;
	};

	bool is_window() const { return std::holds_alternative<Window>(data); }
	bool is_empty() const { return std::holds_alternative<Empty>(data); }
	bool is_leaf() const { return is_empty() || is_window(); }
	bool is_parent() const { return std::holds_alternative<Parent>(data); }

	const Parent& as_parent() const {
		if (std::holds_alternative<Parent>(data)) {
			return std::get<Parent>(data);
		} else {
			semmety_critical_error("Attempted to get parent value of a non-parent FrameData");
		}
	}

	PHLWINDOWREF as_window() const {
		if (std::holds_alternative<Window>(data)) {
			return std::get<Window>(data).window;
		} else {
			semmety_critical_error("Attempted to get window value of a non-window FrameData");
		}
	}

	void makeWindow(PHLWINDOWREF window) { this->data = Window(window); }
	void makeParent(SP<SemmetyFrame> child_a, SP<SemmetyFrame> child_b) {
		Parent parent;
		parent.children.push_back(child_a);
		parent.children.push_back(child_b);
		this->data = parent;
	}
	void makeEmpty() { this->data = Empty {}; }
	void makeOther(SP<SemmetyFrame> other) { this->data = other->data; }

	CBox getStandardWindowArea(CBox area, SBoxExtents extents, PHLWORKSPACE workspace);
	WP<SemmetyFrame> parent;
	bool validateParentReferences() const;

	SemmetyFrame(): data(Empty {}) {}
	SemmetyFrame(SP<SemmetyFrame> other): data(other->data) {}
	// SemmetyFrame(Vector2D pos, Vector2D size): data(Empty {}), geometry(CBox {pos, size}) {}
	// SemmetyFrame(FrameData frameData): data(std::move(frameData)) {}

	static std::list<SP<SemmetyFrame>> getLeafDescendants(const SP<SemmetyFrame>& frame);
	void clearWindow();
	std::string print(int indentLevel = 0, SemmetyWorkspaceWrapper* = nullptr) const;
	void propagateGeometry(const std::optional<CBox>& geometry = std::nullopt);
	std::pair<CBox, CBox> getChildGeometries() const;
	void applyRecursive(PHLWORKSPACE workspace);

	SemmetySplitDirection split_direction = SemmetySplitDirection::SplitV;

	int focusOrder = 0;
	CBox geometry;
	Vector2D gap_topleft_offset;
	Vector2D gap_bottomright_offset;
	int child0Offset = 0;
	CBox getEmptyFrameBox(const CMonitor& monitor);
	void damageEmptyFrameBox(const CMonitor& monitor);

private:
	std::variant<Empty, Window, Parent> data;
};
