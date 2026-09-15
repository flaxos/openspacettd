/* This file is part of OpenSpaceTTD, licensed under the GNU GPL version 2. */
/** @file test_command_authority_network.cpp Independent-process replication fixture for WP-05. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../command_func.h"
#include "../company_func.h"
#include "../company_base.h"
#include "../network/network.h"
#include "../network/network_client.h"
#include "../network/network_internal.h"
#include "../network/core/packet.h"
#include "../portal/portal_cmd.h"
#include "../portal/corporate_hq.h"
#include "mock_environment.h"

#include <array>
#include <cstdlib>
#include <stdexcept>
#include <string>
#ifndef _WIN32
#include <sys/socket.h>
#include <unistd.h>
#endif

/* Shared with the local authority tests; each process starts from the same real engine fixture. */
extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);
extern std::string CommandAuthorityState();
extern void SaveReloadCommandAuthority();

namespace {
#ifndef _WIN32
constexpr size_t MAX_MESSAGE = 65536;

void ReadExact(int fd, void *dst, size_t count)
{
	auto *p = static_cast<uint8_t *>(dst);
	while (count != 0) {
		ssize_t n = recv(fd, p, count, 0);
		if (n <= 0) throw std::runtime_error("loopback peer closed");
		p += n;
		count -= n;
	}
}

void WriteExact(int fd, const void *src, size_t count)
{
	const auto *p = static_cast<const uint8_t *>(src);
	while (count != 0) {
		ssize_t n = send(fd, p, count, MSG_NOSIGNAL);
		if (n <= 0) throw std::runtime_error("loopback peer closed");
		p += n;
		count -= n;
	}
}

std::pair<char, std::vector<uint8_t>> ReadMessage(int fd)
{
	std::array<uint8_t, 5> header{};
	ReadExact(fd, header.data(), header.size());
	uint32_t size = uint32_t(header[1]) | (uint32_t(header[2]) << 8) | (uint32_t(header[3]) << 16) | (uint32_t(header[4]) << 24);
	if (size > MAX_MESSAGE) throw std::runtime_error("oversized loopback message");
	std::vector<uint8_t> data(size);
	if (size != 0) ReadExact(fd, data.data(), size);
	return {static_cast<char>(header[0]), std::move(data)};
}

void WriteMessage(int fd, char op, std::span<const uint8_t> data = {})
{
	uint32_t size = static_cast<uint32_t>(data.size());
	std::array<uint8_t, 5> header{static_cast<uint8_t>(op), static_cast<uint8_t>(size), static_cast<uint8_t>(size >> 8), static_cast<uint8_t>(size >> 16), static_cast<uint8_t>(size >> 24)};
	WriteExact(fd, header.data(), header.size());
	if (!data.empty()) WriteExact(fd, data.data(), data.size());
}

void WriteState(int fd)
{
	std::string state = CommandAuthorityState();
	WriteMessage(fd, 'S', std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(state.data()), state.size()));
}

std::vector<uint8_t> WirePacket(NetworkGameSocketHandler &handler, const CommandPacket &cp)
{
	Packet packet(nullptr, PacketGameType::ClientCommand);
	handler.SendCommand(packet, cp);
	packet.PrepareToSend();
	std::vector<uint8_t> bytes(packet.Size());
	size_t offset = 0;
	while (offset < bytes.size()) {
		ssize_t n = packet.TransferOut([&](std::span<const uint8_t> part) -> ssize_t {
			std::copy(part.begin(), part.end(), bytes.begin() + offset);
			return part.size();
		});
		if (n <= 0) throw std::runtime_error("packet export failed");
		offset += n;
	}
	return bytes;
}

CommandPacket ParseWirePacket(NetworkGameSocketHandler &handler, std::span<const uint8_t> bytes)
{
	if (bytes.size() < 3 || bytes.size() > MAX_MESSAGE) throw std::runtime_error("invalid game packet length");
	Packet packet(&handler, bytes.size());
	size_t offset = 0;
	auto transfer = [&](std::span<uint8_t> part) -> ssize_t {
		std::copy_n(bytes.begin() + offset, part.size(), part.begin());
		offset += part.size();
		return part.size();
	};
	if (packet.TransferIn(transfer) != 2 || !packet.ParsePacketSize() || packet.Size() != bytes.size()) throw std::runtime_error("invalid game packet header");
	while (offset < bytes.size()) if (packet.TransferIn(transfer) <= 0) throw std::runtime_error("truncated game packet");
	if (!packet.PrepareToRead() || packet.Recv_uint8() != to_underlying(PacketGameType::ClientCommand)) throw std::runtime_error("wrong game packet type");
	CommandPacket cp;
	if (auto error = handler.ReceiveCommand(packet, cp)) throw std::runtime_error(std::string(*error));
	return cp;
}

CommandPacket MakeCommand(char op)
{
	CommandPacket cp;
	cp.company = CompanyID{0};
	cp.err_msg = StringID{0};
	cp.callback = nullptr;
	switch (op) {
		case 'U':
			cp.cmd = Commands::UpgradeCorporateHQ;
			cp.data = EndianBufferWriter<CommandDataBuffer>::FromValue(std::make_tuple(CompanyID{0}, CorporateHQTier::PlanetaryHQ));
			break;
		case 'D':
			cp.cmd = Commands::UpgradeCorporateHQ;
			cp.data = EndianBufferWriter<CommandDataBuffer>::FromValue(std::make_tuple(CompanyID{1}, CorporateHQTier::PlanetaryHQ));
			break;
		case 'I':
			cp.cmd = Commands::UpgradeCorporateHQ;
			cp.data = EndianBufferWriter<CommandDataBuffer>::FromValue(std::make_tuple(CompanyID{0}, static_cast<CorporateHQTier>(255)));
			break;
		case 'C':
			cp.cmd = Commands::ColonizeOutpost;
			cp.data = EndianBufferWriter<CommandDataBuffer>::FromValue(std::make_tuple(TileXY(20, 20), std::string("Relay Outpost")));
			break;
		case 'P':
			cp.cmd = Commands::PromoteWorld;
			cp.data = EndianBufferWriter<CommandDataBuffer>::FromValue(std::make_tuple(WorldID{0}));
			break;
		case 'V':
			cp.cmd = Commands::PromoteWorld;
			cp.data = EndianBufferWriter<CommandDataBuffer>::FromValue(std::make_tuple(WorldID{250}));
			break;
		default: throw std::runtime_error("unknown command request");
	}
	return cp;
}
#endif
}

TEST_CASE("WP-05 isolated loopback command worker", "[authority-network-worker]")
{
#ifdef _WIN32
	SUCCEED("POSIX TCP worker is exercised by the Python relay on supported hosts");
#else
	const char *fd_env = std::getenv("OSTTD_AUTHORITY_LOOPBACK_FD");
	if (fd_env == nullptr) return; // Ordinary suite does not start the worker protocol.
	int fd = std::atoi(fd_env);
	REQUIRE(fd >= 3);
	(void)MockEnvironment::Instance();
	SetupCommandAuthorityWorld(WorldPhase::Phase4_Expansion, 10000);
	_networking = true;
	const char *role = std::getenv("OSTTD_AUTHORITY_LOOPBACK_ROLE");
	const bool server = role != nullptr && std::string_view(role) == "server";
	_network_server = server;
	_local_company = CompanyID{0};
	_current_company = _local_company;
	_frame_counter = 0;
	_frame_counter_max = 0;
	ClientNetworkGameSocketHandler handler(INVALID_SOCKET, "loopback-test");
	WriteState(fd);
	for (;;) {
		auto [op, payload] = ReadMessage(fd);
		if (op == 'Z') break;
		if (op == 'S') { WriteState(fd); continue; }
		if (op == 'R') { SaveReloadCommandAuthority(); WriteState(fd); continue; }
		if (op == 'L' || op == 'H') {
			Company::Get(CompanyID{0})->money = op == 'L' ? 0 : 10000000;
			WriteState(fd);
			continue;
		}
		if (op == 'B') {
			CommandPacket cp = ParseWirePacket(handler, payload);
			if (server) {
				/* The server accepts the sanitized packet, schedules and distributes it
				 * through the engine's server command queues, then executes frame N. */
				_frame_counter_max = _frame_counter;
				NetworkSendCommand(cp.cmd, cp.err_msg, nullptr, cp.company, cp.data);
				NetworkDistributeCommands();
				++_frame_counter;
				NetworkExecuteLocalCommandQueue();
				/* Forward the canonical, sanitized command payload to the two clients. */
				auto canonical = WirePacket(handler, cp);
				WriteMessage(fd, 'W', canonical);
			} else {
				cp.frame = ++_frame_counter;
				cp.my_cmd = false;
				handler.incoming_queue.push_back(std::move(cp));
				NetworkExecuteLocalCommandQueue();
			}
			WriteState(fd);
			continue;
		}
		if (op == 'U' || op == 'D' || op == 'I' || op == 'C' || op == 'P' || op == 'V') {
			auto bytes = WirePacket(handler, MakeCommand(op));
			WriteMessage(fd, 'K', bytes);
			continue;
		}
		throw std::runtime_error("unknown loopback operation");
	}
	close(fd);
#endif
}
