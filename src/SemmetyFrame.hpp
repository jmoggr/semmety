#pragma once

#include <utility>
#include <vector>

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprutils/math/Box.hpp>

using namespace Hyprutils::Math;

enum class SemmetySplitDirection {
	SplitH,
	SplitV,
};

class SemmetySplitFrame;
class SemmetyLeafFrame;
class SemmetyWorkspaceWrapper;

class SemmetyFrame {
public:
	virtual ~SemmetyFrame() = default;

	CBox geometry;
	int focusOrder = 0;
	Vector2D gap_topleft_offset;
	Vector2D gap_bottomright_offset;
	WP<SemmetyFrame> self;

	SP<SemmetySplitFrame> asSplit() const;
	SP<SemmetyLeafFrame> asLeaf() const;
	std::vector<SP<SemmetyLeafFrame>> getEmptyFrames() const;
	SP<SemmetyLeafFrame> getLastFocussedLeaf() const;

	virtual bool isLeaf() const = 0;
	virtual bool isSplit() const = 0;
	virtual void applyRecursive(PHLWORKSPACE workspace, CBox newGeometry) = 0;
	virtual std::vector<SP<SemmetyLeafFrame>> getLeafFrames() const = 0;
	virtual std::string
	print(const SemmetyWorkspaceWrapper& workspace, int indentLevel = 0) const = 0;
};

class SemmetySplitFrame: public SemmetyFrame {
public:
	// how much larger the 1st child is
	int child0Offset = 0;
	SemmetySplitDirection splitDirection;
	std::pair<SP<SemmetyFrame>, SP<SemmetyFrame>> children;

	static SP<SemmetySplitFrame> create(SP<SemmetyFrame> firstChild, SP<SemmetyFrame> secondChild);
	SemmetySplitFrame(SP<SemmetyFrame> firstChild, SP<SemmetyFrame> secondChild);

	SP<SemmetyFrame> getOtherChild(const SP<SemmetyFrame>& child);
	std::pair<CBox, CBox> getChildGeometries() const;

	bool isLeaf() const override;
	bool isSplit() const override;
	void applyRecursive(PHLWORKSPACE workspace, CBox newGeometry) override;
	std::vector<SP<SemmetyLeafFrame>> getLeafFrames() const override;
	std::string print(const SemmetyWorkspaceWrapper& workspace, int indentLevel = 0) const override;
};

class SemmetyLeafFrame: public SemmetyFrame {
public:
	static SP<SemmetyLeafFrame> create(PHLWINDOWREF window);
	SemmetyLeafFrame(PHLWINDOWREF window);

	bool isEmpty() const;
	PHLWINDOWREF getWindow() const;
	void setWindow(PHLWINDOWREF win);
	void clearWindow();
	CBox getStandardWindowArea(CBox area, SBoxExtents extents, PHLWORKSPACE workspace) const;
	void damageEmptyFrameBox(const CMonitor& monitor);
	CBox getEmptyFrameBox(const CMonitor& monitor);

	bool isLeaf() const override;
	bool isSplit() const override;
	void applyRecursive(PHLWORKSPACE workspace, CBox newGeometry) override;
	std::vector<SP<SemmetyLeafFrame>> getLeafFrames() const override;
	std::string print(const SemmetyWorkspaceWrapper& workspace, int indentLevel = 0) const override;

private:
	PHLWINDOWREF window;
};
