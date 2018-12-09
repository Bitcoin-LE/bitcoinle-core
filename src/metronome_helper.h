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
#include "serialize.h"

#include <stdio.h>

#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include "support/events.h"

#include <univalue.h>

namespace Metronome {

	/*class CBanEntry
	{
	public:
		static const int CURRENT_VERSION = 1;
		int nVersion;
		int64_t nCreateTime;
		int64_t nBanUntil;
		uint8_t banReason;

		CBanEntry()
		{
			SetNull();
		}

		CBanEntry(int64_t nCreateTimeIn)
		{
			SetNull();
			nCreateTime = nCreateTimeIn;
		}

		ADD_SERIALIZE_METHODS;

		template <typename Stream, typename Operation>
		inline void SerializationOp(Stream& s, Operation ser_action) {
			READWRITE(this->nVersion);
			READWRITE(nCreateTime);
			READWRITE(nBanUntil);
			READWRITE(banReason);
		}

		void SetNull()
		{
			nVersion = CBanEntry::CURRENT_VERSION;
			nCreateTime = 0;
			nBanUntil = 0;
			banReason = BanReasonUnknown;
		}

		std::string banReasonToString()
		{
			switch (banReason) {
			case BanReasonNodeMisbehaving:
				return "node misbehaving";
			case BanReasonManuallyAdded:
				return "manually added";
			default:
				return "unknown";
			}
		}
	};*/

	struct CMetronomeBeat {
		uint256 hash;
		int64_t blockTime;
		int64_t height;
		uint256 nextBlockHash;

		ADD_SERIALIZE_METHODS;

		template <typename Stream, typename Operation>
		inline void SerializationOp(Stream& s, Operation ser_action) {
			READWRITE(this->hash);
			READWRITE(this->blockTime);
			READWRITE(this->height);
			READWRITE(this->nextBlockHash);
		}

		void SetNull() {
			hash.SetNull();
		}

		bool isNull() {
			return hash.IsNull();
		}
	};

	typedef std::map<uint256, CMetronomeBeat> metromap_t;

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
	
		static void SerializeMetronomes();

		static void LoadMetronomes();

		static std::string GetDefaultMetronomeIP();
	};
}

#endif