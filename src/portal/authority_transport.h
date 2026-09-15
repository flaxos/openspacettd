/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file authority_transport.h Asynchronous, non-simulation authority requests. */
#ifndef AUTHORITY_TRANSPORT_H
#define AUTHORITY_TRANSPORT_H

#include "../network/core/http.h"
#include "../3rdparty/nlohmann/json.hpp"
#include <chrono>
#include <functional>
#include <memory>

enum class AuthorityOperation { RegisterWorld, Heartbeat, Initiate, Depart, Pending, Claim, Confirm };

/** One request with an independently retained HTTP callback.
 * Cancellation/timeout is asynchronous: the callback outlives a destroyed request
 * until the backend acknowledges completion. Responses are data only; callbacks must not
 * mutate trains, journals, RNG, or any other simulation state.
 */
class AuthorityRequest final : public HTTPCallback {
public:
	using Clock = std::chrono::steady_clock;
	using Sender = std::function<void(std::string_view, HTTPCallback *, std::string &&)>;
	static constexpr size_t MAX_RESPONSE_BYTES = 4 * 1024 * 1024;

	AuthorityRequest(std::string base_url, AuthorityOperation operation, nlohmann::json payload,
		uint32_t world = 0, std::chrono::seconds timeout = std::chrono::seconds(15));
	AuthorityRequest(const AuthorityRequest &) = delete;
	AuthorityRequest &operator=(const AuthorityRequest &) = delete;
	AuthorityRequest(AuthorityRequest &&) = delete;
	AuthorityRequest &operator=(AuthorityRequest &&) = delete;
	bool Start(const Sender &sender);
	bool Start();
	bool ExecuteSync(std::chrono::milliseconds timeout_limit = std::chrono::milliseconds(5000));
	void Cancel() { this->cancelled = true; }
	bool IsFinished() const { return this->finished; }
	bool Succeeded() const { return this->finished && this->error.empty(); }
	const std::string &GetError() const { return this->error; }
	const nlohmann::json &GetResponse() const { return this->response; }

	void OnFailure() override;
	void OnReceiveData(std::unique_ptr<char[]> data, size_t length) override;
	bool IsCancelled() const override;

private:
	/* Only game-thread callbacks access the target; HTTP backends marshal to that thread. */
	std::shared_ptr<AuthorityRequest *> callback_target;
	std::string uri;
	std::string body;
	std::string buffer;
	std::string error;
	nlohmann::json response;
	std::chrono::seconds timeout;
	Clock::time_point deadline{};
	bool started = false;
	bool finished = false;
	bool cancelled = false;
};

std::string Base64Encode(std::span<const uint8_t> data);
std::vector<uint8_t> Base64Decode(std::string_view encoded);

#endif /* AUTHORITY_TRANSPORT_H */
