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
	virtual void applyRecursive(
	    SemmetyWorkspaceWrapper& workspace,
	    std::optional<CBox> newGeometry,
	    std::optional<bool> force
	) = 0;
	virtual std::vector<SP<SemmetyLeafFrame>> getLeafFrames() const = 0;
	virtual std::string print(SemmetyWorkspaceWrapper& workspace, int indentLevel = 0) const = 0;
};

class SemmetySplitFrame: public SemmetyFrame {
public:
	// how much larger the 1st child is
	int child0Offset = 0;
	SemmetySplitDirection splitDirection;
	std::pair<SP<SemmetyFrame>, SP<SemmetyFrame>> children;

	static SP<SemmetySplitFrame>
	create(SP<SemmetyFrame> firstChild, SP<SemmetyFrame> secondChild, CBox _geometry);

	SP<SemmetyFrame> getOtherChild(const SP<SemmetyFrame>& child);
	std::pair<CBox, CBox> getChildGeometries() const;

	bool isLeaf() const override;
	bool isSplit() const override;
	void applyRecursive(
	    SemmetyWorkspaceWrapper& workspace,
	    std::optional<CBox> newGeometry,
	    std::optional<bool> force
	) override;
	std::vector<SP<SemmetyLeafFrame>> getLeafFrames() const override;
	std::string print(SemmetyWorkspaceWrapper& workspace, int indentLevel = 0) const override;

private:
	SemmetySplitFrame(SP<SemmetyFrame> firstChild, SP<SemmetyFrame> secondChild, CBox _geometry);
	template <typename U, typename... Args>
	friend Hyprutils::Memory::CSharedPointer<U> Hyprutils::Memory::makeShared(Args&&...);
};

class SemmetyLeafFrame: public SemmetyFrame {
public:
	static SP<SemmetyLeafFrame> create(PHLWINDOWREF window);

	bool isEmpty() const;
	PHLWINDOWREF getWindow();
	void setWindow(SemmetyWorkspaceWrapper& workspace, PHLWINDOWREF win);
	PHLWINDOWREF replaceWindow(SemmetyWorkspaceWrapper& workspace, PHLWINDOWREF win);
	CBox getStandardWindowArea(CBox area, SBoxExtents extents, PHLWORKSPACE workspace) const;
	void damageEmptyFrameBox(const CMonitor& monitor);
	CBox getEmptyFrameBox(const CMonitor& monitor);
	void swapContents(SemmetyWorkspaceWrapper& workspace, SP<SemmetyLeafFrame> leafFrame);

	bool isLeaf() const override;
	bool isSplit() const override;
	void applyRecursive(
	    SemmetyWorkspaceWrapper& workspace,
	    std::optional<CBox> newGeometry,
	    std::optional<bool> force
	) override;
	std::vector<SP<SemmetyLeafFrame>> getLeafFrames() const override;
	std::string print(SemmetyWorkspaceWrapper& workspace, int indentLevel = 0) const override;

private:
	PHLWINDOWREF window;
	SemmetyLeafFrame(PHLWINDOWREF window);
	void _setWindow(SemmetyWorkspaceWrapper& workspace, PHLWINDOWREF win, bool force);

	template <typename U, typename... Args>
	friend Hyprutils::Memory::CSharedPointer<U> Hyprutils::Memory::makeShared(Args&&...);
};
