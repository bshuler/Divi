#include <BlockInvalidationHelpers.h>

#include <chain.h>
#include <blockmap.h>
#include <BlockFileHelpers.h>
#include <sync.h>
#include <ForkWarningHelpers.h>
#include <NodeStateRegistry.h>
#include <ChainstateManager.h>
#include <utiltime.h>
#include <Logging.h>
#include <I_ChainTipManager.h>
#include <ValidationState.h>

static BlockIndexCandidates setBlockIndexCandidates;

BlockIndexCandidates& GetBlockIndexCandidates()
{
    return setBlockIndexCandidates;
}


void InvalidChainFound(
    const bool isInitialBlockDownload,
    const Settings& settings,
    CCriticalSection& mainCriticalSection,
    CBlockIndex* pindexNew)
{
    updateMostWorkInvalidBlockIndex(pindexNew);

    const ChainstateManager::Reference chainstate;
    const auto& chain = chainstate->ActiveChain();

    LogPrintf("InvalidChainFound: invalid block=%s  height=%d  log2_work=%.8g  date=%s\n",
              pindexNew->GetBlockHash(), pindexNew->nHeight,
              log(pindexNew->nChainWork.getdouble()) / log(2.0), DateTimeStrFormat("%Y-%m-%d %H:%M:%S",
                                                                                   pindexNew->GetBlockTime()));
    LogPrintf("InvalidChainFound:  current best=%s  height=%d  log2_work=%.8g  date=%s\n",
              chain.Tip()->GetBlockHash(), chain.Height(), log(chain.Tip()->nChainWork.getdouble()) / log(2.0),
              DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chain.Tip()->GetBlockTime()));
    CheckForkWarningConditions(settings, mainCriticalSection, isInitialBlockDownload);
}

bool InvalidateBlock(
    const I_ChainTipManager& chainTipManager,
    const bool isInitialBlockDownload,
    const Settings& settings,
    CCriticalSection& mainCriticalSection,
    ChainstateManager& chainstate,
    CBlockIndex* pindex)
{
    AssertLockHeld(mainCriticalSection);

    const auto& chain = chainstate.ActiveChain();
    auto& blockMap = chainstate.GetBlockMap();

    // Mark the block itself as invalid.
    pindex->nStatus |= BLOCK_FAILED_VALID;
    BlockFileHelpers::RecordDirtyBlockIndex(pindex);
    setBlockIndexCandidates.erase(pindex);

    while (chain.Contains(pindex)) {
        CBlockIndex* pindexWalk = blockMap.at(chain.Tip()->GetBlockHash());
        pindexWalk->nStatus |= BLOCK_FAILED_CHILD;
        BlockFileHelpers::RecordDirtyBlockIndex(pindexWalk);
        setBlockIndexCandidates.erase(pindexWalk);
        // ActivateBestChain considers blocks already in chainActive
        // unconditionally valid already, so force disconnect away from it.
        if (!chainTipManager.disconnectTip()) {
            return false;
        }
    }

    // The resulting new best tip may not be in setBlockIndexCandidates anymore, so
    // add them again.
    for (const auto& entry : blockMap) {
        if (entry.second->IsValid(BLOCK_VALID_TRANSACTIONS) && entry.second->nChainTx && !setBlockIndexCandidates.value_comp()(entry.second, chain.Tip())) {
            setBlockIndexCandidates.insert(entry.second);
        }
    }

    InvalidChainFound(isInitialBlockDownload, settings, mainCriticalSection, pindex);
    return true;
}


//! List of asynchronously-determined block rejections to notify this peer about.
void InvalidBlockFound(
    const std::map<uint256, NodeId>& peerIdByBlockHash,
    const bool isInitialBlockDownload,
    const Settings& settings,
    CCriticalSection& mainCriticalSection,
    CBlockIndex* pindex,
    const CValidationState& state)
{
    int nDoS = 0;
    if (state.IsInvalid(nDoS)) {
        std::map<uint256, NodeId>::const_iterator it = peerIdByBlockHash.find(pindex->GetBlockHash());
        if (it != peerIdByBlockHash.end()) {
            Misbehaving(it->second,nDoS,"Invalid block sourced from peer");
        }
    }
    if (!state.CorruptionPossible()) {
        pindex->nStatus |= BLOCK_FAILED_VALID;
        BlockFileHelpers::RecordDirtyBlockIndex(pindex);
        setBlockIndexCandidates.erase(pindex);
        InvalidChainFound(isInitialBlockDownload, settings, mainCriticalSection, pindex);
    }
}
