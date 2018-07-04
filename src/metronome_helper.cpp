#include "metronome_helper.h"

#include "uint256.h"
#include <map>
#include <memory>
#include <iostream>
#include <string>
#include <codecvt>
#include <locale>
#include <boost/process.hpp>


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

using namespace Metronome;

static const char DEFAULT_METRONOME_ADDR[] = "127.0.0.1";
static const int DEFAULT_METRONOME_PORT = 8332;
static const int DEFAULT_HTTP_CLIENT_TIMEOUT = 900;
static const bool DEFAULT_NAMED = false;
static const int CONTINUE_EXECUTION = -1;
static const int MAX_RETRIES = 3;


class CConnectionFailed : public std::runtime_error
{
public:

	explicit inline CConnectionFailed(const std::string& msg) :
		std::runtime_error(msg)
	{}

};

struct HTTPReply
{
	HTTPReply() : status(0), error(-1) {}

	int status;
	int error;
	std::string body;
};

static void http_request_done(struct evhttp_request *req, void *ctx)
{
	HTTPReply *reply = static_cast<HTTPReply*>(ctx);

	if (req == nullptr) {
		/* If req is nullptr, it means an error occurred while connecting: the
		* error code will have been passed to http_error_cb.
		*/
		reply->status = 0;
		return;
	}

	reply->status = evhttp_request_get_response_code(req);

	struct evbuffer *buf = evhttp_request_get_input_buffer(req);
	if (buf)
	{
		size_t size = evbuffer_get_length(buf);
		const char *data = (const char*)evbuffer_pullup(buf, size);
		if (data)
			reply->body = std::string(data, size);
		evbuffer_drain(buf, size);
	}
}

const char *http_errorstring_metronome(int code)
{
	switch (code) {
#if LIBEVENT_VERSION_NUMBER >= 0x02010300
	case EVREQ_HTTP_TIMEOUT:
		return "timeout reached";
	case EVREQ_HTTP_EOF:
		return "EOF reached";
	case EVREQ_HTTP_INVALID_HEADER:
		return "error while reading header, or invalid header";
	case EVREQ_HTTP_BUFFER_ERROR:
		return "error encountered while reading or writing";
	case EVREQ_HTTP_REQUEST_CANCEL:
		return "request was canceled";
	case EVREQ_HTTP_DATA_TOO_LONG:
		return "response body is larger than allowed";
#endif
	default:
		return "unknown";
	}
}

std::shared_ptr<CMetronomeBeat> CMetronomeHelper::GetLatestMetronomeBeat() {
	uint256 bestBeat = GetBestBlockHash();
	
	if (bestBeat.IsNull()) {
		return std::shared_ptr<CMetronomeBeat>();
	}

	auto beat = GetMetronomeBeat(bestBeat);
	return beat;
}

std::shared_ptr<CMetronomeBeat> CMetronomeHelper::GetMetronomeBeat(uint256 hash) {
	auto beat = GetBlockInfo(hash);
	return beat;
}

uint256 CMetronomeHelper::GetBestBlockHash() {
	UniValue params(UniValue::VARR);

	UniValue reply = ResilientGetMetronomeInfoRPC("getblockchaininfo", params);
	UniValue error = find_value(reply, "error");

	if (!error.isNull()) {
		return uint256();
	}

	UniValue result = find_value(reply, "result");
	UniValue bestblockhash = find_value(result, "bestblockhash");

	if (!bestblockhash.isStr()) {
		return uint256();
	}

	std::string bestHash = bestblockhash.get_str();

	// printf("Metronome Best Beat Hash: %s", bestHash.c_str());

	return uint256S(bestHash);
}

std::shared_ptr<CMetronomeBeat> CMetronomeHelper::GetBlockInfo(uint256 hash) {
	UniValue params(UniValue::VARR);
	params.push_back(hash.GetHex());

	UniValue reply = ResilientGetMetronomeInfoRPC("getblockheader", params);
	UniValue error = find_value(reply, "error");

	if (!error.isNull()) {
		return std::shared_ptr<CMetronomeBeat>();
	}

	UniValue result = find_value(reply, "result");
	UniValue headerTime = find_value(result, "time");

	if (!headerTime.isNum()) {
		return std::shared_ptr<CMetronomeBeat>();
	}

	int64_t blockTime = headerTime.get_int64();

	std::shared_ptr<CMetronomeBeat> beat = std::make_shared<CMetronomeBeat>();
	beat->hash = hash;
	beat->blockTime = blockTime;

	// printf("Bitcoin Metronome Block Time: %lu", beat->blockTime);

	return beat;
}


UniValue CMetronomeHelper::GetMetronomeInfoRPC(const std::string& strMethod, const UniValue& params)
{
	std::string host;
	int port;
	std::string user;
	std::string password;

	SplitHostPort(gArgs.GetArg("-metronomeAddr", DEFAULT_METRONOME_ADDR), port, host);
	port = gArgs.GetArg("-metronomePort", DEFAULT_METRONOME_PORT);
	// Get credentials
	// TODO: replace test with empty string
	std::string strRPCUserColonPass = gArgs.GetArg("-metronomeUser", "") + ":" + gArgs.GetArg("-metronomePassword", "");

	// printf("Metronome args: %s@%s:%d\n", strRPCUserColonPass.c_str(), host.c_str(), port);

	// Obtain event base
	raii_event_base base = obtain_event_base();

	// Synchronously look up hostname
	raii_evhttp_connection evcon = obtain_evhttp_connection_base(base.get(), host, port);
	evhttp_connection_set_timeout(evcon.get(), gArgs.GetArg("-rpcclienttimeout", DEFAULT_HTTP_CLIENT_TIMEOUT));

	HTTPReply response;
	raii_evhttp_request req = obtain_evhttp_request(http_request_done, (void*)&response);
	if (req == nullptr)
		throw std::runtime_error("create http request failed");



	struct evkeyvalq* output_headers = evhttp_request_get_output_headers(req.get());
	assert(output_headers);
	evhttp_add_header(output_headers, "Host", host.c_str());
	evhttp_add_header(output_headers, "Connection", "close");
	evhttp_add_header(output_headers, "Authorization", (std::string("Basic ") + EncodeBase64(strRPCUserColonPass)).c_str());

	// Attach request data
	std::string strRequest = JSONRPCRequestObj(strMethod, params, 1).write() + "\n";
	struct evbuffer* output_buffer = evhttp_request_get_output_buffer(req.get());
	assert(output_buffer);
	evbuffer_add(output_buffer, strRequest.data(), strRequest.size());

	// check if we should use a special wallet endpoint
	std::string endpoint = "/";
	std::string walletName = gArgs.GetArg("-rpcwallet", "");
	if (!walletName.empty()) {
		char *encodedURI = evhttp_uriencode(walletName.c_str(), walletName.size(), false);
		if (encodedURI) {
			endpoint = "/wallet/" + std::string(encodedURI);
			free(encodedURI);
		}
		else {
			throw CConnectionFailed("uri-encode failed");
		}
	}

	int r = evhttp_make_request(evcon.get(), req.get(), EVHTTP_REQ_POST, endpoint.c_str());
	req.release(); // ownership moved to evcon in above call
	if (r != 0) {
		throw CConnectionFailed("send http request failed");
	}

	event_base_dispatch(base.get());

	if (response.status == 0)
		throw CConnectionFailed(strprintf("couldn't connect to metonome server: %s (code %d)\n(make sure server is running and you are connecting to the correct RPC port)", http_errorstring_metronome(response.error), response.error));
	else if (response.status == HTTP_UNAUTHORIZED)
		throw std::runtime_error("incorrect metronomeUser or metronomePassword (authorization failed)");
	else if (response.status >= 400 && response.status != HTTP_BAD_REQUEST && response.status != HTTP_NOT_FOUND && response.status != HTTP_INTERNAL_SERVER_ERROR)
		throw std::runtime_error(strprintf("metronome server returned HTTP error %d", response.status));
	else if (response.body.empty())
		throw std::runtime_error("no response from metronome server");

	// Parse reply
	UniValue valReply(UniValue::VSTR);

	if (!valReply.read(response.body))
		throw std::runtime_error("couldn't parse reply from metronome server");
	const UniValue& reply = valReply.get_obj();
	if (reply.empty())
		throw std::runtime_error("expected metronome reply to have result, error and id properties");

	// printf("\nGetMetronomeInfoRPC -> %s\n\n", reply.write().c_str());

	return reply;
}

UniValue CMetronomeHelper::ResilientGetMetronomeInfoRPC(const std::string& strMethod, const UniValue& params) {
	for (int i = 0; i < MAX_RETRIES; ++i) {
		try {
			return GetMetronomeInfoRPC(strMethod, params);
		}
		catch (...) {

		}
	}
	throw std::exception();
}