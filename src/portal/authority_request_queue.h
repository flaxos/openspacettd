/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file authority_request_queue.h Bounded nonblocking authority request polling. */
#ifndef AUTHORITY_REQUEST_QUEUE_H
#define AUTHORITY_REQUEST_QUEUE_H

#include "authority_transport.h"
#include <algorithm>
#include <map>
#include <optional>
#include <tuple>

/** Server-local transport state; never serialized or consulted by simulation commands. */
class AuthorityRequestQueue {
public:
	using Clock = AuthorityRequest::Clock;
	static constexpr size_t MAX_REQUESTS = 16;

	/** Poll one stable operation identity. An absent result never waits for HTTP.
	 * Repeated calls while pending reuse the same request. Failed requests retain
	 * a bounded retry delay; successful responses are consumed exactly once.
	 */
	std::optional<nlohmann::json> Poll(const std::string &url, AuthorityOperation operation,
			const std::string &identity, nlohmann::json payload, uint32_t world,
			const AuthorityRequest::Sender &sender = {}, Clock::time_point now = Clock::now())
	{
		const Key key{operation, world, identity};
		auto found = this->requests.find(key);
		if (found != this->requests.end()) {
			auto &entry = found->second;
			entry.last_poll = now;
			if (entry.request != nullptr) {
				if (entry.request->IsCancelled() && !entry.request->IsFinished()) entry.request->OnFailure();
				if (!entry.request->IsFinished()) return std::nullopt;
				if (entry.request->Succeeded()) {
					auto response = entry.request->GetResponse();
					this->requests.erase(found);
					return response;
				}
				entry.request.reset();
				entry.retry_after = now + std::chrono::seconds(2);
			}
			if (now < entry.retry_after || this->quiescing) return std::nullopt;
		} else {
			if (this->quiescing || this->requests.size() >= MAX_REQUESTS) return std::nullopt;
			found = this->requests.emplace(key, Entry{}).first;
		}
		auto &entry = found->second;
		entry.last_poll = now;
		entry.request = std::make_unique<AuthorityRequest>(url, operation, std::move(payload), world);
		if (sender) entry.request->Start(sender);
		else entry.request->Start();
		return std::nullopt;
	}

	/** Discard abandoned operations so stale replies cannot exhaust the bound. */
	void Sweep(Clock::time_point now = Clock::now())
	{
		std::erase_if(this->requests, [&](const auto &item) {
			return now - item.second.last_poll > std::chrono::seconds(30);
		});
	}

	/** Stop issuing new HTTP work while callers consume outstanding results. */
	void SetQuiescing(bool value) { this->quiescing = value; }
	bool IsQuiescing() const { return this->quiescing; }
	size_t PendingCount() const
	{
		return std::count_if(this->requests.begin(), this->requests.end(), [](const auto &item) {
			return item.second.request != nullptr;
		});
	}
	void Reset()
	{
		this->requests.clear();
		this->quiescing = false;
	}

private:
	using Key = std::tuple<AuthorityOperation, uint32_t, std::string>;
	struct Entry {
		std::unique_ptr<AuthorityRequest> request;
		Clock::time_point retry_after{};
		Clock::time_point last_poll{};
	};
	std::map<Key, Entry> requests;
	bool quiescing = false;
};

#endif /* AUTHORITY_REQUEST_QUEUE_H */
