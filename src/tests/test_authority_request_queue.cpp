/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file test_authority_request_queue.cpp Nonblocking authority retry and drain tests. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../portal/authority_request_queue.h"

#include "../safeguards.h"

TEST_CASE("Federation transport queue reuses pending requests and drains without new work")
{
	AuthorityRequestQueue queue;
	std::vector<HTTPCallback *> callbacks;
	const AuthorityRequest::Sender sender = [&](std::string_view, HTTPCallback *callback, std::string &&) {
		callbacks.push_back(callback);
	};
	auto poll = [&](const std::string &id) {
		return queue.Poll("http://localhost:8080", AuthorityOperation::Depart, id,
			{{"transfer_id", id}}, 1, sender);
	};
	CHECK_FALSE(poll("TX-1"));
	for (int i = 0; i < 100; ++i) CHECK_FALSE(poll("TX-1"));
	REQUIRE(callbacks.size() == 1);
	CHECK(queue.PendingCount() == 1);
	queue.SetQuiescing(true);
	CHECK_FALSE(poll("TX-2"));
	CHECK(callbacks.size() == 1);
	auto data = std::make_unique<char[]>(2);
	data[0] = '{';
	data[1] = '}';
	callbacks.front()->OnReceiveData(std::move(data), 2);
	callbacks.front()->OnReceiveData(nullptr, 0);
	CHECK(poll("TX-1").has_value());
	CHECK(queue.PendingCount() == 0);
	CHECK_FALSE(poll("TX-1"));
	CHECK(callbacks.size() == 1);
}

TEST_CASE("Federation transport queue retries stable payloads after backoff")
{
	AuthorityRequestQueue queue;
	std::vector<std::string> bodies;
	auto now = AuthorityRequestQueue::Clock::now();
	const AuthorityRequest::Sender sender = [&](std::string_view, HTTPCallback *callback, std::string &&body) {
		bodies.push_back(body);
		callback->OnFailure();
	};
	auto poll = [&]() {
		return queue.Poll("http://localhost:8080", AuthorityOperation::Initiate, "DEP-1",
			{{"request_id", "DEP-1"}}, 1, sender, now);
	};
	CHECK_FALSE(poll());
	CHECK_FALSE(poll());
	REQUIRE(bodies.size() == 1);
	now += std::chrono::seconds(1);
	CHECK_FALSE(poll());
	CHECK(bodies.size() == 1);
	now += std::chrono::seconds(1);
	CHECK_FALSE(poll());
	REQUIRE(bodies.size() == 2);
	CHECK(bodies[0] == bodies[1]);
}

TEST_CASE("Federation transport queue bounds abandoned requests and cancels late callbacks")
{
	AuthorityRequestQueue queue;
	std::vector<HTTPCallback *> callbacks;
	const AuthorityRequest::Sender sender = [&](std::string_view, HTTPCallback *callback, std::string &&) {
		callbacks.push_back(callback);
	};
	auto now = AuthorityRequestQueue::Clock::now();
	for (size_t i = 0; i < AuthorityRequestQueue::MAX_REQUESTS + 10; ++i) {
		CHECK_FALSE(queue.Poll("http://localhost:8080", AuthorityOperation::Claim, fmt::format("{}", i),
			nlohmann::json::object(), 1, sender, now));
	}
	CHECK(callbacks.size() == AuthorityRequestQueue::MAX_REQUESTS);
	queue.Sweep(now + std::chrono::seconds(31));
	CHECK(queue.PendingCount() == 0);
	for (auto *callback : callbacks) {
		CHECK(callback->IsCancelled());
		callback->OnFailure();
	}
}
