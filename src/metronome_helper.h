#ifndef BITCOIN_METRONOMEHELPERS_H
#define BITCOIN_METRONOMEHELPERS_H

#include "uint256.h"
#include <map>
#include <memory>
#include <iostream>
#include <string>
#include <codecvt>
#include <locale>
//#include <boost/process.hpp>


#include "chainparamsbase.h"
#include "clientversion.h"
#include "fs.h"
#include "rpc/client.h"
#include "rpc/protocol.h"
#include "util.h"
#include "utilstrencodings.h"

#include <stdio.h>

#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include "support/events.h"

#include <univalue.h>

namespace Metronome {

	struct CMetronomeBeat {
		uint256 hash;
		int64_t blockTime;
		int64_t height;
		uint256 nextBlockHash;
	};

	class CMetronomeHelper
	{
		static std::map<std::string, std::shared_ptr<CMetronomeHelper>> metronomeCache;

	public:
		static std::shared_ptr<CMetronomeBeat> GetMetronomeBeat(uint256 hash);

		static std::shared_ptr<CMetronomeBeat> GetBlockInfo(uint256 hash);

		static UniValue GetMetronomeInfoRPC(const std::string& strMethod, const UniValue& params);

		static uint256 GetBestBlockHash();

		static std::shared_ptr<CMetronomeBeat> GetLatestMetronomeBeat();

		static UniValue ResilientGetMetronomeInfoRPC(const std::string& strMethod, const UniValue& params);
	};
}

#endif