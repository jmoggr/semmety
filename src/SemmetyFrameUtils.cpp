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
	if (auto parent = findParent(target, workspace)) {
		if (parent->children.first == target) {
			slot = &parent->children.first;
		} else if (parent->children.second == target) {
			slot = &parent->children.second;
		} else {
			// this should not be possible based on findParent
			semmety_critical_error("Parent does not have child");
		}
	}

	*slot = source;

	source->applyRecursive(workspace, target->geometry);

	std::vector<PHLWINDOWREF> oldWindows;
	for (const auto& leaf: target->getLeafFrames()) {
		if (auto win = leaf->getWindow()) {
			oldWindows.push_back(win);
		}
	}

	std::unordered_set<PHLWINDOWREF> currentWindows;
	for (const auto& leaf: workspace.root->getLeafFrames()) {
		if (auto win = leaf->getWindow()) {
			currentWindows.insert(win);
		}
	}

	auto emptyFrames = workspace.root->getEmptyFrames();

	// sort empty frames by size, largest first
	std::sort(emptyFrames.begin(), emptyFrames.end(), frameAreaGreater);
	size_t emptyFramesIndex = 0;

	for (const auto& win: oldWindows) {
		if (currentWindows.contains(win)) {
			continue;
		}

		if (emptyFramesIndex < emptyFrames.size()) {
			emptyFrames[emptyFramesIndex]->setWindow(workspace, win);
			emptyFramesIndex += 1;
		} else {
			win->setHidden(true);
		}
	}
}

SP<SemmetySplitFrame>
findParentRecursive(const SP<SemmetyFrame> current, const SP<SemmetyFrame> target) {
	if (current->isLeaf()) {
		return nullptr;
	}

	auto split = current->asSplit();
	const auto& children = split->children;

	// If either child is the target, return the current split.
	if (children.first == target || children.second == target) {
		return split;
	}

	if (auto res = findParentRecursive(children.first, target)) {
		return res;
	}

	if (auto res = findParentRecursive(children.second, target)) {
		return res;
	}

	return nullptr;
}

// Find the parent of a node in the workspace.
// If 'target' is the root, returns nullptr.
SP<SemmetySplitFrame>
findParent(const SP<SemmetyFrame> target, SemmetyWorkspaceWrapper& workspace) {
	if (workspace.root == target) {
		return nullptr;
	}

	if (auto res = findParentRecursive(workspace.root, target)) {
		return res;
	}

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

	if (maxFocusOrderLeaf == leafFrames.end()) {
		return nullptr;
	}

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
	default: return nullptr;
	}

	auto candidates = workspace.root->getLeafFrames();

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

	if (candidates.empty()) {
		return nullptr;
	}

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

bool frameAreaGreater(const SP<SemmetyLeafFrame>& a, const SP<SemmetyLeafFrame>& b) {
	const auto sa = a->geometry.size();
	const auto sb = b->geometry.size();

	const double areaA = sa.x * sa.y;
	const double areaB = sb.x * sb.y;

	return areaA > areaB; // descending order
}
