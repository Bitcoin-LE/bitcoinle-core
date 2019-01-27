// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "primitives/block.h"
#include "uint256.h"
#include "metronome_helper.h"
#include <algorithm>

int64_t HF2_BLOCK_HEIGHT = 71850;
int64_t HF3_BLOCK_HEIGHT = 81150;

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

	if (pindexLast->nHeight + 1 == HF3_BLOCK_HEIGHT) {
		return CalculateNextWorkRequiredBigJump(pindexLast, params);
	}

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval(pindexLast->nHeight + 1) != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval(pindex->nHeight) != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

	if (pindexLast->nHeight + 1 > Consensus::Forks::HF4_BLOCK_HEIGHT) {
		return CalculateNextWorkRequiredLE_HF4(pindexLast, params);
	}
	else if (pindexLast->nHeight + 1 > HF2_BLOCK_HEIGHT) {
		return CalculateNextWorkRequiredLE(pindexLast, params);
	}

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval(0) - 1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int CalculateNextWorkRequiredBigJump(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
	if (params.fPowNoRetargeting)
		return pindexLast->nBits;

	int64_t avgMiningTime = 1;	

	// Retarget
	const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
	arith_uint256 bnNew;
	bnNew.SetCompact(pindexLast->nBits);

	LogPrintf("NEW DIFFICULTY: AVG=%d seconds, TARGET=%d seconds\n", avgMiningTime, params.nPowTargetMiningSpacing);

	bnNew *= avgMiningTime;
	bnNew /= params.nPowTargetMiningSpacing;

	if (bnNew > bnPowLimit)
		bnNew = bnPowLimit;

	return bnNew.GetCompact();
}

unsigned int CalculateNextWorkRequiredLE(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
	if (params.fPowNoRetargeting)
		return pindexLast->nBits;

	int64_t SAMPLING_PERIOD = 32L;
	int64_t avgMiningTime = 0;
	int64_t sampleCount = 0;
	const CBlockIndex* currentBlockIndex = pindexLast;
	for (int64_t i = 0; i < params.nMinerConfirmationWindow; ++i) {
		if (i % SAMPLING_PERIOD == 0) {
			std::shared_ptr<Metronome::CMetronomeBeat> beat = Metronome::CMetronomeHelper::GetMetronomeBeat(currentBlockIndex->hashMetronome);
			assert(beat);
			int64_t blockEpoch = currentBlockIndex->GetBlockTime();
			int64_t metroTime = beat->blockTime;
			int64_t miningTime = blockEpoch - metroTime;
			int64_t blockTime = blockEpoch - currentBlockIndex->pprev->GetBlockTime();
			miningTime = std::min(miningTime, blockTime);

			// Limit adjustment step
			if (miningTime < params.nPowTargetMiningSpacing / 4)
				miningTime = params.nPowTargetMiningSpacing / 4;
			if (miningTime > params.nPowTargetMiningSpacing * 4)
				miningTime = params.nPowTargetMiningSpacing * 4;
			
			avgMiningTime += miningTime;
			sampleCount++;
		}
		currentBlockIndex = currentBlockIndex->pprev;
	}
	avgMiningTime /= sampleCount;

	if (avgMiningTime == 0) {
		avgMiningTime = 1;
	}

	// Retarget
	const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
	arith_uint256 bnNew;
	bnNew.SetCompact(pindexLast->nBits);

	LogPrintf("NEW DIFFICULTY: AVG=%d seconds, TARGET=%d seconds\n", avgMiningTime, params.nPowTargetMiningSpacing);
	printf("NEW DIFFICULTY: AVG=%d seconds, TARGET=%d seconds\n", avgMiningTime, params.nPowTargetMiningSpacing);

	bnNew *= avgMiningTime;
	bnNew /= params.nPowTargetMiningSpacing;

	if (bnNew > bnPowLimit)
		bnNew = bnPowLimit;

	return bnNew.GetCompact();
}

unsigned int CalculateNextWorkRequiredLE_HF4(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
	if (params.fPowNoRetargeting)
		return pindexLast->nBits;

	int64_t SAMPLING_PERIOD = 1L;
	int64_t avgMiningTime = 0;
	int64_t sampleCount = 0;
	const CBlockIndex* currentBlockIndex = pindexLast;
	for (int64_t i = 0; i < params.nMinerConfirmationWindow_HF4; ++i) {
		if (i % SAMPLING_PERIOD == 0) {
			std::shared_ptr<Metronome::CMetronomeBeat> beat = Metronome::CMetronomeHelper::GetMetronomeBeat(currentBlockIndex->hashMetronome);
			assert(beat);
			int64_t blockEpoch = currentBlockIndex->GetBlockTime();
			int64_t metroTime = beat->blockTime;
			int64_t miningTime = blockEpoch - metroTime;
			int64_t blockTime = blockEpoch - currentBlockIndex->pprev->GetBlockTime();
			miningTime = std::min(miningTime, blockTime);

			// Limit adjustment step
			if (miningTime < params.nPowTargetMiningSpacing_HF4 / 4)
				miningTime = params.nPowTargetMiningSpacing_HF4 / 4;
			if (miningTime > params.nPowTargetMiningSpacing_HF4 * 4)
				miningTime = params.nPowTargetMiningSpacing_HF4 * 4;

			avgMiningTime += miningTime;
			sampleCount++;
		}
		currentBlockIndex = currentBlockIndex->pprev;
	}
	avgMiningTime /= sampleCount;

	if (avgMiningTime == 0) {
		avgMiningTime = 1;
	}

	// Retarget
	const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
	arith_uint256 bnNew;
	bnNew.SetCompact(pindexLast->nBits);

	LogPrintf("NEW DIFFICULTY: AVG=%d seconds, TARGET=%d seconds\n", avgMiningTime, params.nPowTargetMiningSpacing_HF4);
	printf("NEW DIFFICULTY: AVG=%d seconds, TARGET=%d seconds\n", avgMiningTime, params.nPowTargetMiningSpacing_HF4);

	bnNew *= avgMiningTime;
	bnNew /= params.nPowTargetMiningSpacing_HF4;

	if (bnNew > bnPowLimit)
		bnNew = bnPowLimit;

	return bnNew.GetCompact();
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

	//printf("CheckProofOfWork nBits -> %x\n", nBits);
	//printf("CheckProofOfWork -> %s <= %s\n", hash.GetHex().c_str(), bnTarget.GetHex().c_str());
	//printf("Pow Limit -> %s\n", params.powLimit.GetHex().c_str());

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
