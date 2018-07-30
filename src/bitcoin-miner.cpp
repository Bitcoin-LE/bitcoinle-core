// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2018 The Bitcoin LE Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "miner.h"
#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "crypto/sha256.h"
#include "fs.h"
#include "key.h"
#include "validation.h"
#include "miner.h"
#include "net_processing.h"
#include "pubkey.h"
#include "random.h"
#include "txdb.h"
#include "wallet/wallet.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "rpc/server.h"
#include "rpc/register.h"
#include "script/sigcache.h"
#include "base58.h"
#include "scheduler.h"
#include "metronome_helper.h"

#include <boost/thread.hpp>
#include <thread>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

const int MAX_N_THREADS = 6;

struct MinerHandler {
	bool found;
	bool interrupt;
	CBlock block;
	uint64_t currentOffset[MAX_N_THREADS];
	MinerHandler() : found(false), block(CBlock()), interrupt(false) {

	}
	void clear() {
		found = false;
		block = CBlock();
		interrupt = false;
	}
};

MinerHandler handler;

void proofOfWorkFinder(uint32_t idx, CBlock block, uint64_t from, uint64_t to, MinerHandler* handler);

CBlock CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns, const CScript& scriptPubKey)
{
	const CChainParams& chainparams = Params();

	uint32_t WAIT_TIME = 1000;
	uint32_t PRINTF_PERIOD = 10000;

	std::shared_ptr<Metronome::CMetronomeBeat> beat;

	for (uint64_t i = 0;;++i) {
		if (handler.interrupt) {
			return CBlock();
		}

		CBlockIndex* headBlock = chainActive.Tip();

		std::shared_ptr<Metronome::CMetronomeBeat> latestBeat = Metronome::CMetronomeHelper::GetLatestMetronomeBeat();
		if (CheckBlockRestWindowCompliance(headBlock->GetBlockTime(), latestBeat->hash, chainparams, GetAdjustedTime())) {
			int age = GetAdjustedTime() - latestBeat->blockTime;
			int sleepTime = latestBeat->blockTime - headBlock->GetBlockTime();
			printf("Found beat -> Hash: %s, Time: %lu, Age: %ds\n", latestBeat->hash.GetHex().c_str(), latestBeat->blockTime, age);
			printf("Previous Block -> Height: %d, Time: %lu, Sleep: %ds\n", headBlock->nHeight, headBlock->GetBlockTime(), sleepTime);
			printf("AdjustedTime: %d, Time: %d\n", GetAdjustedTime(), GetTime());
			beat = latestBeat;
			break;
		}

		//if (i % (PRINTF_PERIOD / WAIT_TIME) == 0) {
			//printf("Current Height: %d\n", pindexPrev->nHeight);
			printf("Waiting for metronome beat... %lu ms\n", i * WAIT_TIME);
		//}

		MilliSleep(WAIT_TIME);
	}

	printf("Creating new block...\n");

	std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(chainparams).CreateNewBlock(scriptPubKey, true, beat->hash);
	CBlock& block = pblocktemplate->block;

	printf("Block difficulty nBits: %x \n", block.nBits);

	arith_uint256 bnTarget;
	bool fNegative, fOverflow;
	bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
	printf("Target Hash: %s\n", bnTarget.GetHex().c_str());

	// Replace mempool-selected txns with just coinbase plus passed-in txns:
	//block.vtx.resize(1);
	//for (const CMutableTransaction& tx : txns)
	//	block.vtx.push_back(MakeTransactionRef(tx));
	// IncrementExtraNonce creates a valid coinbase and merkleRoot
	unsigned int extraNonce = 0;

	printf("Incrementing extra nonce...\n");
	IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);

	/*uint256 bestHash = uint256S("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");

	while (!CheckProofOfWork(block.GetHash(), block.nBits, chainparams.GetConsensus())) {
		uint256 currentHash = block.GetHash();

		if (currentHash.GetHex() < bestHash.GetHex()) {
			bestHash = currentHash;
			printf("Best Hash: %s\n", bestHash.GetHex().c_str());
		}

		if (block.nNonce >= 0xFFFFFFFF) {
			printf("Nonce MAX VALUE reached! Restarting...\n");
			return CBlock();
		}

		if (chainActive.Tip()->GetBlockHash().GetHex() != block.hashPrevBlock.GetHex()) {
			printf("Someone else mined a block! Restarting...\n");
			return CBlock();
		}

		++block.nNonce;

		// TODO: change rules so that block with timestamps in the past don't get in??
		if (block.nNonce % 10 == 0) {
			block.nTime = GetAdjustedTime();
		}

		if (block.nNonce % 100000 == 0) {
			printf("Nonce: %lu \r", block.nNonce);
		}

		// TODO: restart calculation if new block has been found by someone else
		// TODO: check if nonce has reached MAX value and if true, restart calculation
	}

	printf("Processing new block: %s, BlockTime: %lu, Now: %lu\n", block.GetHash().GetHex().c_str(), block.GetBlockTime(), GetTime());

	//block.nTime += 120;

	std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(block);
	bool success = ProcessNewBlock(chainparams, shared_pblock, true, nullptr);

	printf("Ending... Block accepted? %s...\n", success ? "true" : "false");
	CBlock result = block;
	return result;*/

	handler.clear();
	std::thread thds[MAX_N_THREADS];
	uint64_t PAGE_SIZE_MINER = 0x100000000L / MAX_N_THREADS;
	for (uint32_t i = 0; i < MAX_N_THREADS; ++i) {
		thds[i] = std::thread(proofOfWorkFinder, i, CBlock(block), i * PAGE_SIZE_MINER, (i + 1) * PAGE_SIZE_MINER, &handler);
	}

	for (uint32_t i = 0; i < MAX_N_THREADS; ++i) {
		thds[i].join();
	}

	if (handler.found) {
		return handler.block;
	}

	return CBlock();
}

void proofOfWorkFinder(uint32_t idx, CBlock block, uint64_t from, uint64_t to, MinerHandler* handler) {
	const CChainParams& chainparams = Params();
	block.nNonce = from;
	while (!CheckProofOfWork(block.GetHash(), block.nBits, chainparams.GetConsensus())) {
		handler->currentOffset[idx] = block.nNonce;

		if (handler->found || handler->interrupt) {
			block.SetNull();
			break;
		}

		if (chainActive.Tip()->GetBlockHash().GetHex() != block.hashPrevBlock.GetHex()) {
			if (idx == 0) {
				printf("Someone else mined a block! Restarting...\n");
			}
			block.SetNull();
			break;
		}

		block.nNonce++;

		if (block.nNonce >= to || block.nNonce < from) {
			block.SetNull();
			break;
		}

		if (block.nNonce % 10 == 0) {
			block.nTime = GetAdjustedTime();
		}

		if (idx == 0 && block.nNonce % 100000 == 0) {
			std::cout << "Nonces: ";
			for (int i = 0; i < MAX_N_THREADS; ++i) {
				std::cout << handler->currentOffset[i] << " ";
			}
			std::cout << "\r";
		}
	}

	if (block.IsNull()) {
		//printf("Ending thread: %d\n", idx);
		return;
	}

	handler->found = true;
	handler->block = block;

	//MilliSleep(1000);

	printf("Processing new block: %s, BlockTime: %lu, Now: %lu\n", block.GetHash().GetHex().c_str(), block.GetBlockTime(), GetTime());

	//block.nTime += 120;

	std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(block);
	bool success = ProcessNewBlock(chainparams, shared_pblock, true, nullptr);

	printf("Ending... Block accepted? %s.\n", success ? "Yes" : "No");
	//MilliSleep(10000);
}

static void my_handler(int s) {
	//printf("Caught signal %d\n", s);
	printf("Shutting down... Please wait...\n", s);

	handler.interrupt = true;
	MilliSleep(100);

	//Shutdown();
	//exit(1);
}

int main(int argc, char* argv[])
{
	// signal(SIGINT, my_handler);

	boost::thread_group threadGroup;
	CScheduler scheduler;

	gArgs.ParseParameters(argc, argv);

	try
	{
		gArgs.ReadConfigFile(gArgs.GetArg("-conf", BITCOIN_CONF_FILENAME));
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Error reading configuration file: %s\n", e.what());
		return false;
	}

	SelectParams(CBaseChainParams::MAIN);

	InitLogging();
	InitParameterInteraction();
	if (!AppInitBasicSetup())
	{
		// InitError will have been called with detailed error, which ends up on console
		exit(EXIT_FAILURE);
	}
	if (!AppInitParameterInteraction())
	{
		// InitError will have been called with detailed error, which ends up on console
		exit(EXIT_FAILURE);
	}
	if (!AppInitSanityChecks())
	{
		// InitError will have been called with detailed error, which ends up on console
		exit(EXIT_FAILURE);
	}

	if (!AppInitLockDataDirectory())
	{
		// If locking the data directory failed, exit immediately
		exit(EXIT_FAILURE);
	}
	bool fRet = AppInitMain(threadGroup, scheduler);

#ifdef _WIN32
	signal(SIGINT, my_handler);
#else
	struct sigaction satmp;
	sigemptyset(&satmp.sa_mask);
	satmp.sa_flags = 0;
	satmp.sa_handler = my_handler;
	sigaction(SIGTERM, &satmp, NULL);
	sigaction(SIGQUIT, &satmp, NULL);
	if (sigaction(SIGINT, &satmp, NULL) == -1) {
		printf("Could not register SIGINT handler.\n");
	}
#endif

	std::vector<CTransaction> coinbaseTxns;
	CKey coinbaseKey;
	coinbaseKey.MakeNewKey(true);
	//printf("Payment Address: %s\n", coinbaseKey.GetPubKey().GetID().GetHex().c_str());

	// Como converter uma coinbase key em bitcoin address
	//CBitcoinAddress addr(coinbaseKey.GetPubKey().GetID());
	std::cout << "Wallet Count: " << vpwallets.size() << std::endl;

	// CBitcoinAddress addr("JhZGKE8uFDTmCA1gce1wnr4FU9ip7LRe3f");
	// std::string s = addr.ToString();
	// printf("Payment Address: %s\n\n", s.c_str());

	//CScript scriptPubKey = CScript() << ToByteVector(coinbaseKey.GetPubKey().GetHash()) << OP_CHECKSIG;
	std::shared_ptr<CReserveScript> scriptPubKey = std::make_shared<CReserveScript>();
	vpwallets[0]->GetScriptForMining(scriptPubKey);

	for (;;)
	{
		try {
			if (handler.interrupt) {
				break;
			}
			std::vector<CMutableTransaction> noTxns;
			CBlock b = CreateAndProcessBlock(noTxns, scriptPubKey->reserveScript);
			if (!b.IsNull()) {
				coinbaseTxns.push_back(*b.vtx[0]);
			}
		}
		catch (...) {
			std::cout << "Exception raised!" << std::endl;
		}
	}

	Interrupt(threadGroup);
	Shutdown();
	return 0;
}
