#include "SemmetyFrameUtils.hpp"
#include <algorithm>
#include <numeric>

#include "SemmetyWorkspaceWrapper.hpp"
#include "log.hpp"

void replaceNode(
    SP<SemmetyFrame> target,
    SP<SemmetyFrame> source,
    SemmetyWorkspaceWrapper& workspace
) {
	auto* slot = &workspace.root;
	std::vector<int> targetPath = {}; // Path to target's position

	if (auto parent = findParent(target, workspace)) {
		if (parent->children.first == target) {
			slot = &parent->children.first;
			targetPath = parent->framePath;
			targetPath.push_back(0);
		} else if (parent->children.second == target) {
			slot = &parent->children.second;
			targetPath = parent->framePath;
			targetPath.push_back(1);
		} else {
			// this should not be possible based on findParent
			semmety_critical_error("Parent does not have child");
		}
	}

	*slot = source;

	// Update paths for entire source subtree
	updateFramePathsRecursive(source, targetPath);

	(*slot)->applyRecursive(workspace, target->geometry, true);

	while (true) {
		auto frame = workspace.getLargestEmptyFrame();
		if (!frame) { break; }

		auto window = workspace.getNextWindowForFrame(frame);
		if (!window) { break; }

		frame->setWindow(workspace, window);
	}

	for (auto& window: workspace.windows) {
		if (!window->isHidden() && !workspace.isWindowVisible(window)) { window->setHidden(true); }
	}
}

void updateFramePathsRecursive(SP<SemmetyFrame> frame, const std::vector<int>& newPath) {
	if (!frame) { return; }

	frame->framePath = newPath;

	if (frame->isSplit()) {
		auto split = frame->asSplit();
		const auto& children = split->getChildren();

		if (children.first) {
			auto firstPath = newPath;
			firstPath.push_back(0);
			updateFramePathsRecursive(children.first, firstPath);
		}

		if (children.second) {
			auto secondPath = newPath;
			secondPath.push_back(1);
			updateFramePathsRecursive(children.second, secondPath);
		}
	}
}

SP<SemmetySplitFrame>
findParentRecursive(const SP<SemmetyFrame> current, const SP<SemmetyFrame> target) {
	if (current->isLeaf()) { return nullptr; }

	auto split = current->asSplit();
	const auto& children = split->getChildren();

	// If either child is the target, return the current split.
	if (children.first == target || children.second == target) { return split; }

	if (auto res = findParentRecursive(children.first, target)) { return res; }

	if (auto res = findParentRecursive(children.second, target)) { return res; }

	return nullptr;
}

// Find the parent of a node in the workspace.
// If 'target' is the root, returns nullptr.
SP<SemmetySplitFrame>
findParent(const SP<SemmetyFrame> target, SemmetyWorkspaceWrapper& workspace) {
	if (workspace.getRoot() == target) { return nullptr; }

	if (auto res = findParentRecursive(workspace.getRoot(), target)) { return res; }

	semmety_critical_error("Failed to find parent");
}

SP<SemmetyLeafFrame> getMaxFocusOrderLeaf(const std::vector<SP<SemmetyLeafFrame>> leafFrames) {
	auto maxFocusOrderLeaf = std::max_element(
	    leafFrames.begin(),
	    leafFrames.end(),
	    [](const SP<SemmetyLeafFrame>& a, const SP<SemmetyLeafFrame>& b) {
		    return a->focusOrder < b->focusOrder;
	    }
	);

	if (maxFocusOrderLeaf == leafFrames.end()) { return nullptr; }

	return *maxFocusOrderLeaf;
}

bool overlap(int start1, int end1, int start2, int end2) {
	const int dx = std::max(0, std::min(end1, end2) - std::max(start1, start2));
	return dx > 0;
}

SP<SemmetyLeafFrame> getNeighborByDirection(
    const SemmetyWorkspaceWrapper& workspace,
    const SP<SemmetyLeafFrame> basis,
    const Direction dir
) {
	bool vertical;
	int sign;
	switch (dir) {
	case Direction::Up:
		vertical = true;
		sign = -1;
		break;
	case Direction::Down:
		vertical = true;
		sign = 1;
		break;
	case Direction::Left:
		vertical = false;
		sign = -1;
		break;
	case Direction::Right:
		vertical = false;
		sign = 1;
		break;
	}

	auto candidates = workspace.getRoot()->getLeafFrames();

	candidates.erase(
	    std::remove_if(
	        candidates.begin(),
	        candidates.end(),
	        [&](const SP<SemmetyLeafFrame>& tile) {
		        return vertical ? tile->geometry.pos().y * sign <= basis->geometry.pos().y * sign
		                        : tile->geometry.pos().x * sign <= basis->geometry.pos().x * sign;
	        }
	    ),
	    candidates.end()
	);

	candidates.erase(
	    std::remove_if(
	        candidates.begin(),
	        candidates.end(),
	        [&](const SP<SemmetyLeafFrame>& tile) {
		        return vertical ? !overlap(
		                              basis->geometry.pos().x,
		                              basis->geometry.pos().x + basis->geometry.size().x,
		                              tile->geometry.pos().x,
		                              tile->geometry.pos().x + tile->geometry.size().x
		                          )
		                        : !overlap(
		                              basis->geometry.pos().y,
		                              basis->geometry.pos().y + basis->geometry.size().y,
		                              tile->geometry.pos().y,
		                              tile->geometry.pos().y + tile->geometry.size().y
		                          );
	        }
	    ),
	    candidates.end()
	);

	if (candidates.empty()) { return nullptr; }

	auto min = sign
	         * std::accumulate(
	               candidates.begin(),
	               candidates.end(),
	               std::numeric_limits<int>::max(),
	               [&](int prevMin, const SP<SemmetyFrame>& tile) {
		               return vertical
		                        ? std::min(tile->geometry.pos().y * sign, static_cast<double>(prevMin))
		                        : std::min(tile->geometry.pos().x * sign, static_cast<double>(prevMin));
	               }
	         );

	std::vector<SP<SemmetyLeafFrame>> closest;
	for (const auto& tile: candidates) {
		if ((vertical && tile->geometry.pos().y == min) || (!vertical && tile->geometry.pos().x == min))
		{
			closest.push_back(tile->asLeaf());
		}
	}

	return getMaxFocusOrderLeaf(closest);
}

SP<SemmetyLeafFrame>
getMostOverlappingLeafFrame(SemmetyWorkspaceWrapper& workspace, const PHLWINDOWREF& window) {
	if (!window) { return nullptr; }

	const auto windowBox = CBox(window->m_position, window->m_size);

	double maxOverlapArea = 0.0;
	SP<SemmetyLeafFrame> bestFrame = nullptr;

	for (const auto& leaf: workspace.getRoot()->getLeafFrames()) {
		const auto& frameBox = leaf->geometry;

		const double windowLeft = windowBox.pos().x;
		const double windowRight = windowBox.pos().x + windowBox.size().x;
		const double windowTop = windowBox.pos().y;
		const double windowBottom = windowBox.pos().y + windowBox.size().y;

		const double frameLeft = frameBox.pos().x;
		const double frameRight = frameBox.pos().x + frameBox.size().x;
		const double frameTop = frameBox.pos().y;
		const double frameBottom = frameBox.pos().y + frameBox.size().y;

		const double overlapX =
		    std::max(0.0, std::min(windowRight, frameRight) - std::max(windowLeft, frameLeft));
		const double overlapY =
		    std::max(0.0, std::min(windowBottom, frameBottom) - std::max(windowTop, frameTop));

		const double overlapArea = overlapX * overlapY;

		if (overlapArea > maxOverlapArea) {
			maxOverlapArea = overlapArea;
			bestFrame = leaf;
		}
	}

	return bestFrame;
}

bool getPathNodes(
    const SP<SemmetyFrame>& target,
    const SP<SemmetyFrame>& current,
    std::vector<SP<SemmetyFrame>>& path
) {
	path.push_back(current);

	if (current == target) { return true; }

	if (current->isSplit()) {
		auto split = current->asSplit();
		const auto& children = split->getChildren();
		if (getPathNodes(target, children.first, path)) { return true; }
		if (getPathNodes(target, children.second, path)) { return true; }
	}

	path.pop_back();
	return false;
}

SP<SemmetySplitFrame> getCommonParent(
    SemmetyWorkspaceWrapper& workspace,
    SP<SemmetyFrame> frameA,
    SP<SemmetyFrame> frameB
) {
	std::vector<SP<SemmetyFrame>> pathA, pathB;
	if (!getPathNodes(frameA, workspace.getRoot(), pathA)) {
		semmety_critical_error("Frame A not found in the tree");
	}
	if (!getPathNodes(frameB, workspace.getRoot(), pathB)) {
		semmety_critical_error("Frame B not found in the tree");
	}

	SP<SemmetyFrame> commonAncestor = nullptr;
	size_t minSize = std::min(pathA.size(), pathB.size());
	for (size_t i = 0; i < minSize; ++i) {
		if (pathA[i] == pathB[i]) {
			commonAncestor = pathA[i];
		} else {
			break;
		}
	}

	if (!commonAncestor) { semmety_critical_error("No common parent found"); }

	if (commonAncestor->isLeaf()) {
		semmety_critical_error("Commond parent is a leaf, this should not be possible");
	}

	return commonAncestor->asSplit();
}

SP<SemmetySplitFrame> getResizeTarget(
    SemmetyWorkspaceWrapper& workspace,
    SP<SemmetyLeafFrame> frame,
    Direction posDirection,
    Direction negDirection
) {
	auto neighborPos = getNeighborByDirection(workspace, frame, posDirection);
	auto neighborNeg = getNeighborByDirection(workspace, frame, negDirection);

	if (!neighborPos && !neighborNeg) { return {}; }

	if (!neighborPos) {
		return getCommonParent(workspace, frame, neighborNeg);
	} else if (!neighborNeg) {
		return getCommonParent(workspace, frame, neighborPos);
	} else {
		auto commonParentPos = getCommonParent(workspace, frame, neighborPos);
		if (!commonParentPos) { semmety_critical_error("not possible"); }

		auto commonParentNeg = getCommonParent(workspace, frame, neighborNeg);
		if (!commonParentNeg) { semmety_critical_error("not possible"); }

		auto posLength = commonParentPos->pathLengthToDescendant(frame).value_or(0)
		               + commonParentPos->pathLengthToDescendant(neighborPos).value_or(0);

		auto negLength = commonParentNeg->pathLengthToDescendant(frame).value_or(0)
		               + commonParentNeg->pathLengthToDescendant(neighborNeg).value_or(0);

		return posLength > negLength ? commonParentNeg : commonParentPos;
	}
}

SP<SemmetySplitFrame>
getResizeTarget(SemmetyWorkspaceWrapper& workspace, SP<SemmetyLeafFrame> basis, Direction dir) {
	auto neighbor = getNeighborByDirection(workspace, basis, dir);
	if (!neighbor) { return {}; }

	return getCommonParent(workspace, basis, neighbor);
}

bool frameAreaGreater(const SP<SemmetyLeafFrame>& a, const SP<SemmetyLeafFrame>& b) {
	const auto sa = a->geometry.size();
	const auto sb = b->geometry.size();

	const double areaA = sa.x * sa.y;
	const double areaB = sb.x * sb.y;

	return areaA > areaB; // descending order
}

// Recursive helper function that searches for the target frame in the tree and records the path.
// The path is recorded as a vector of integers, where 0 refers to children.first and 1 refers to
// children.second.
bool frameDepthFirstSearch(
    const SP<SemmetyFrame>& current,
    const SP<SemmetyFrame>& target,
    std::vector<int>& path
) {
	// If the current frame is the target, we have found the path.
	if (current == target) { return true; }

	// Only split frames have children.
	if (current->isSplit()) {
		auto split = current->asSplit();
		const auto& children = split->getChildren();

		// Explore the first child (index 0)
		path.push_back(0);
		if (frameDepthFirstSearch(children.first, target, path)) { return true; }
		path.pop_back(); // Backtrack if not found in first child

		// Explore the second child (index 1)
		path.push_back(1);
		if (frameDepthFirstSearch(children.second, target, path)) { return true; }
		path.pop_back(); // Backtrack if not found in second child
	}

	return false;
}

std::string getFramePath(const SP<SemmetyFrame>& targetFrame, const SP<SemmetyFrame>& rootFrame) {
	std::vector<int> pathIndices;
	if (!frameDepthFirstSearch(rootFrame, targetFrame, pathIndices)) { return ""; }

	if (pathIndices.empty()) { return "root"; }

	std::ostringstream oss;
	for (size_t i = 0; i < pathIndices.size(); ++i) {
		oss << pathIndices[i];
		if (i + 1 < pathIndices.size()) oss << "/";
	}
	return oss.str();
}
