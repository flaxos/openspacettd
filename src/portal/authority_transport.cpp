/** @file authority_transport.cpp HTTP transport relay for Universe Authority coordination. */

#include "../stdafx.h"
#include "authority_transport.h"
#include <thread>

#include "../safeguards.h"

namespace {
/** This relay lives until HTTP's terminal notification, not the stack request.
 * A weak target lets timeout return promptly without leaving a dangling callback. */
class AuthorityCallbackRelay final : public HTTPCallback {
public:
	explicit AuthorityCallbackRelay(const std::shared_ptr<AuthorityRequest *> &target) : target(target) {}
	bool IsCancelled() const override
	{
		auto live = this->target.lock();
		return !live || (*live)->IsCancelled();
	}
	void OnFailure() override
	{
		std::unique_ptr<AuthorityCallbackRelay> completed(this);
		if (auto live = this->target.lock()) (*live)->OnFailure();
	}
	void OnReceiveData(std::unique_ptr<char[]> data, size_t length) override
	{
		const bool terminal = data == nullptr;
		std::unique_ptr<AuthorityCallbackRelay> completed(terminal ? this : nullptr);
		if (auto live = this->target.lock()) (*live)->OnReceiveData(std::move(data), length);
	}
private:
	std::weak_ptr<AuthorityRequest *> target;
};
} // namespace

AuthorityRequest::AuthorityRequest(std::string base_url, AuthorityOperation operation,
	nlohmann::json payload, uint32_t world, std::chrono::seconds timeout) : callback_target(std::make_shared<AuthorityRequest *>(this)), timeout(timeout)
{
	/* Configuration is an origin, not an arbitrary URL path or credential carrier. */
	auto scheme = base_url.find("://");
	if (scheme == std::string::npos || (base_url.substr(0, scheme) != "http" && base_url.substr(0, scheme) != "https")) {
		this->error = "Authority URL requires http or https";
		return;
	}
	if (base_url.ends_with('/')) base_url.pop_back();
	std::string host = base_url.substr(scheme + 3);
	if (host.empty() || host.find_first_of("/@?#\\ \t\r\n") != std::string::npos) {
		this->error = "Authority URL must contain only an origin";
		return;
	}
	if (timeout <= std::chrono::seconds::zero()) {
		this->error = "Authority timeout must be positive";
		return;
	}
	std::string path;
	switch (operation) {
		case AuthorityOperation::RegisterWorld: path = "/worlds/register"; break;
		case AuthorityOperation::Heartbeat: path = "/worlds/heartbeat"; break;
		case AuthorityOperation::Initiate: path = "/transfers/initiate"; break;
		case AuthorityOperation::Depart: path = "/transfers/depart"; break;
		case AuthorityOperation::Pending: path = fmt::format("/transfers/pending?dest_world={}", world); break;
		case AuthorityOperation::Claim: path = "/transfers/claim"; break;
		case AuthorityOperation::Confirm: path = "/transfers/confirm"; break;
		default: this->error = "Unknown authority operation"; return;
	}
	this->uri = base_url + path;
	if (operation != AuthorityOperation::Pending) {
		if (!payload.is_object()) {
			this->error = "Authority request must be a JSON object";
			return;
		}
		this->body = payload.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
		if (this->body.size() > MAX_RESPONSE_BYTES) this->error = "Authority request exceeds size limit";
	}
}

bool AuthorityRequest::Start(const Sender &sender)
{
	if (this->started || this->finished) return false;
	if (!this->error.empty() || !sender || this->cancelled) {
		if (this->error.empty()) this->error = "Authority request cancelled or missing sender";
		this->finished = true;
		return false;
	}
	this->started = true;
	this->deadline = Clock::now() + this->timeout;
	/* Sender may synchronously fail (including builds without HTTP support). */
	sender(this->uri, new AuthorityCallbackRelay(this->callback_target), std::string(this->body));
	return true;
}

bool AuthorityRequest::Start()
{
	return this->Start([](std::string_view uri, HTTPCallback *callback, std::string &&body) {
		NetworkHTTPSocketHandler::Connect(uri, callback, std::move(body));
	});
}

bool AuthorityRequest::IsCancelled() const
{
	return this->cancelled || (this->started && Clock::now() >= this->deadline);
}

void AuthorityRequest::OnFailure()
{
	if (this->finished) return;
	if (this->error.empty()) this->error = this->IsCancelled() ? "Authority request cancelled or timed out" : "Authority HTTP request failed";
	this->finished = true;
	this->buffer.clear();
}

void AuthorityRequest::OnReceiveData(std::unique_ptr<char[]> data, size_t length)
{
	if (this->finished) return;
	if (data != nullptr) {
		if (this->IsCancelled()) return;
		if (length > MAX_RESPONSE_BYTES - this->buffer.size()) {
			this->error = "Authority response exceeds size limit";
			this->Cancel();
			return; // Retain callback ownership until terminal notification.
		}
		this->buffer.append(data.get(), length);
		return;
	}
	if (this->IsCancelled()) {
		this->OnFailure();
		return;
	}
	this->response = nlohmann::json::parse(this->buffer, nullptr, false);
	if (!this->response.is_object()) {
		this->error = "Authority returned invalid JSON envelope";
	} else if (this->response.contains("error")) {
		this->error = "Authority rejected request";
	}
	this->buffer.clear();
	this->finished = true;
}

bool AuthorityRequest::ExecuteSync(std::chrono::milliseconds timeout_limit)
{
	if (!this->started && !this->Start()) return false;
	auto start = Clock::now();
	while (!this->IsFinished() && (Clock::now() - start) < timeout_limit) {
		NetworkHTTPSocketHandler::HTTPReceive();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (!this->IsFinished()) {
		this->Cancel();
		this->OnFailure();
	}
	return this->Succeeded();
}

namespace {
static const char base64_chars[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";
} // namespace

/** Encode bytes to base64. */
std::string Base64Encode(std::span<const uint8_t> data)
{
	std::string ret;
	size_t len = data.size();
	ret.reserve(((len + 2) / 3) * 4);

	size_t i = 0;
	while (i < len) {
		size_t remaining = len - i;
		if (remaining >= 3) {
			uint32_t n = (static_cast<uint32_t>(data[i]) << 16) |
				(static_cast<uint32_t>(data[i + 1]) << 8) |
				static_cast<uint32_t>(data[i + 2]);
			ret.push_back(base64_chars[(n >> 18) & 0x3F]);
			ret.push_back(base64_chars[(n >> 12) & 0x3F]);
			ret.push_back(base64_chars[(n >> 6) & 0x3F]);
			ret.push_back(base64_chars[n & 0x3F]);
			i += 3;
		} else if (remaining == 2) {
			uint32_t n = (static_cast<uint32_t>(data[i]) << 16) |
				(static_cast<uint32_t>(data[i + 1]) << 8);
			ret.push_back(base64_chars[(n >> 18) & 0x3F]);
			ret.push_back(base64_chars[(n >> 12) & 0x3F]);
			ret.push_back(base64_chars[(n >> 6) & 0x3F]);
			ret.push_back('=');
			i += 2;
		} else { // remaining == 1
			uint32_t n = (static_cast<uint32_t>(data[i]) << 16);
			ret.push_back(base64_chars[(n >> 18) & 0x3F]);
			ret.push_back(base64_chars[(n >> 12) & 0x3F]);
			ret.push_back('=');
			ret.push_back('=');
			i += 1;
		}
	}
	return ret;
}

/** Decode base64 to bytes. */
std::vector<uint8_t> Base64Decode(std::string_view encoded)
{
	std::vector<uint8_t> ret;
	if (encoded.empty()) return ret;

	auto char_value = [](char c) -> int {
		if (c >= 'A' && c <= 'Z') return c - 'A';
		if (c >= 'a' && c <= 'z') return c - 'a' + 26;
		if (c >= '0' && c <= '9') return c - '0' + 52;
		if (c == '+') return 62;
		if (c == '/') return 63;
		return -1;
	};

	uint32_t buf = 0;
	int bits = 0;
	for (char c : encoded) {
		if (c == '=' || c == '\r' || c == '\n' || c == ' ') continue;
		int val = char_value(c);
		if (val < 0) continue;
		buf = (buf << 6) | static_cast<uint32_t>(val);
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			ret.push_back(static_cast<uint8_t>((buf >> bits) & 0xFF));
		}
	}
	return ret;
}
