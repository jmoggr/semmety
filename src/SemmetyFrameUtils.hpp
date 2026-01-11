#include "SemmetyFrame.hpp"
#include "utils.hpp"

SP<SemmetySplitFrame> findParent(const SP<SemmetyFrame> target, SemmetyWorkspaceWrapper& workspace);
void replaceNode(
    SP<SemmetyFrame> target,
    SP<SemmetyFrame> source,
    SemmetyWorkspaceWrapper& workspace
);
void updateFramePathsRecursive(SP<SemmetyFrame> frame, const std::vector<int>& newPath);
SP<SemmetyLeafFrame> getMaxFocusOrderLeaf(const std::vector<SP<SemmetyLeafFrame>> leafFrames);
SP<SemmetyLeafFrame> getNeighborByDirection(
    const SemmetyWorkspaceWrapper& workspace,
    const SP<SemmetyLeafFrame> basis,
    const Direction dir
);
SP<SemmetyLeafFrame>
getMostOverlappingLeafFrame(SemmetyWorkspaceWrapper& workspace, const PHLWINDOWREF& window);
bool frameAreaGreater(const SP<SemmetyLeafFrame>& a, const SP<SemmetyLeafFrame>& b);
std::string getFramePath(const SP<SemmetyFrame>& targetFrame, const SP<SemmetyFrame>& rootFrame);
SP<SemmetySplitFrame>
getResizeTarget(SemmetyWorkspaceWrapper& workspace, SP<SemmetyLeafFrame> basis, Direction dir);
SP<SemmetySplitFrame> getResizeTarget(
    SemmetyWorkspaceWrapper& workspace,
    SP<SemmetyLeafFrame> frame,
    Direction posDirection,
    Direction negDirection
);
bool getPathNodes(
    const SP<SemmetyFrame>& target,
    const SP<SemmetyFrame>& current,
    std::vector<SP<SemmetyFrame>>& path
);
SP<SemmetySplitFrame> getCommonParent(
    SemmetyWorkspaceWrapper& workspace,
    SP<SemmetyFrame> frameA,
    SP<SemmetyFrame> frameB
);
