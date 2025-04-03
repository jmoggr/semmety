#include "SemmetyFrame.hpp"
#include "utils.hpp"

SP<SemmetySplitFrame> findParent(const SP<SemmetyFrame> target, SemmetyWorkspaceWrapper& workspace);
void replaceNode(
    SP<SemmetyFrame> target,
    SP<SemmetyFrame> source,
    SemmetyWorkspaceWrapper& workspace
);
SP<SemmetyLeafFrame> getMaxFocusOrderLeaf(const std::vector<SP<SemmetyLeafFrame>> leafFrames);
SP<SemmetyLeafFrame> getNeighborByDirection(
    const SemmetyWorkspaceWrapper& workspace,
    const SP<SemmetyLeafFrame> basis,
    const Direction dir
);
