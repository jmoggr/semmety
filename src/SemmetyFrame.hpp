#pragma once

#include <utility>
#include <vector>

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprutils/math/Box.hpp>

#include "src/config/ConfigDataValues.hpp"
#include "src/helpers/AnimatedVariable.hpp"

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

	virtual bool isSameOrDescendant(const SP<SemmetyFrame>& target) const = 0;
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
	float splitRatio = 0.5;
	SemmetySplitDirection splitDirection;
	std::pair<SP<SemmetyFrame>, SP<SemmetyFrame>> children;

	static SP<SemmetySplitFrame>
	create(SP<SemmetyFrame> firstChild, SP<SemmetyFrame> secondChild, CBox _geometry);

	SP<SemmetyFrame> getOtherChild(const SP<SemmetyFrame>& child);
	std::pair<CBox, CBox> getChildGeometries() const;
	void resize(double distance);

	bool isSameOrDescendant(const SP<SemmetyFrame>& target) const override;
	bool isLeaf() const override;
	bool isSplit() const override;
	void applyRecursive(
	    SemmetyWorkspaceWrapper& workspace,
	    std::optional<CBox> newGeometry,
	    std::optional<bool> force
	) override;
	std::vector<SP<SemmetyLeafFrame>> getLeafFrames() const override;
	std::optional<size_t> pathLengthToDescendant(const SP<SemmetyFrame>& target) const;
	std::string print(SemmetyWorkspaceWrapper& workspace, int indentLevel = 0) const override;

private:
	SemmetySplitFrame(SP<SemmetyFrame> firstChild, SP<SemmetyFrame> secondChild, CBox _geometry);
	template <typename U, typename... Args>
	friend Hyprutils::Memory::CSharedPointer<U> Hyprutils::Memory::makeShared(Args&&...);
};

class SemmetyLeafFrame: public SemmetyFrame {
public:
	static SP<SemmetyLeafFrame> create(PHLWINDOWREF window);
	CGradientValueData m_cRealBorderColor = {0};
	CGradientValueData m_cRealBorderColorPrevious = {0};
	PHLANIMVAR<float> m_fBorderFadeAnimationProgress;

	bool isEmpty() const;
	PHLWINDOWREF getWindow();
	void setBorderColor(CGradientValueData grad);
	void setWindow(SemmetyWorkspaceWrapper& workspace, PHLWINDOWREF win);
	PHLWINDOWREF replaceWindow(SemmetyWorkspaceWrapper& workspace, PHLWINDOWREF win);
	CBox getStandardWindowArea(CBox area, SBoxExtents extents, PHLWORKSPACE workspace) const;
	void damageEmptyFrameBox(const CMonitor& monitor);
	CBox getEmptyFrameBox(const CMonitor& monitor);
	void swapContents(SemmetyWorkspaceWrapper& workspace, SP<SemmetyLeafFrame> leafFrame);

	bool isSameOrDescendant(const SP<SemmetyFrame>& target) const override;
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
