/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */

/** @file transfer_journal.h Savegame checkpoint records for federation recovery.
 * These records alone are not a write-ahead log: an external handoff must not be
 * acknowledged until its checkpoint and physical consist are durably committed.
 */
#ifndef TRANSFER_JOURNAL_H
#define TRANSFER_JOURNAL_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

enum class TransferCheckpointState : uint8_t {
	Prepared,
	Departed,
	Materialized,
	Confirmed,
};

struct TransferCheckpoint {
	std::string request_id;
	std::string transfer_id;
	std::string arrival_receipt;
	uint64_t namespace_high = 0;
	uint64_t namespace_low = 0;
	uint64_t consist_sequence = 0;
	uint32_t source_world = 0;
	uint32_t destination_world = 0;
	TransferCheckpointState state = TransferCheckpointState::Prepared;
	std::vector<uint8_t> snapshot;

	bool IsValid() const
	{
		if (request_id.empty() || request_id.size() > 128 || transfer_id.size() > 128 ||
				arrival_receipt.size() > 128 || snapshot.empty() ||
				(namespace_high == 0 && namespace_low == 0) || consist_sequence == 0) return false;
		switch (state) {
			case TransferCheckpointState::Prepared: return arrival_receipt.empty();
			case TransferCheckpointState::Departed: return !transfer_id.empty() && arrival_receipt.empty();
			case TransferCheckpointState::Materialized:
			case TransferCheckpointState::Confirmed: return !transfer_id.empty() && !arrival_receipt.empty();
			default: return false;
		}
	}

	bool operator==(const TransferCheckpoint &) const = default;
};

/** Deterministic checkpoint store. Mutations belong on the simulation command
 * thread, never in a transport callback. Snapshot decoding/content admission is
 * the caller's responsibility; restore retains bytes for later reconciliation.
 */
class TransferJournal {
	inline static std::map<std::pair<uint32_t, std::string>, TransferCheckpoint> records;

public:
	static void Reset() { records.clear(); }
	static const auto &GetAll() { return records; }
	static const TransferCheckpoint *Find(uint32_t source_world, const std::string &request_id)
	{
		auto it = records.find({source_world, request_id});
		return it == records.end() ? nullptr : &it->second;
	}

	static const TransferCheckpoint *FindByTransferId(const std::string &transfer_id)
	{
		if (transfer_id.empty()) return nullptr;
		for (const auto &[key, record] : records) {
			if (record.transfer_id == transfer_id) return &record;
		}
		return nullptr;
	}

	static bool HasArrival(const std::string &transfer_id)
	{
		const auto *rec = FindByTransferId(transfer_id);
		return rec != nullptr && (rec->state == TransferCheckpointState::Materialized || rec->state == TransferCheckpointState::Confirmed);
	}

	/** Identical replays are harmless; conflicting request identities fail closed. */
	static bool Restore(const TransferCheckpoint &record)
	{
		if (!record.IsValid()) return false;
		auto [it, inserted] = records.emplace(std::make_pair(record.source_world, record.request_id), record);
		return inserted || it->second == record;
	}

	static bool Prepare(const TransferCheckpoint &record)
	{
		return record.state == TransferCheckpointState::Prepared && record.transfer_id.empty() && Restore(record);
	}

	/** Called only after authority admission. Does not release the physical train. */
	static bool BindTransfer(uint32_t source_world, const std::string &request_id, const std::string &transfer_id)
	{
		auto it = records.find({source_world, request_id});
		if (it == records.end() || transfer_id.empty() || transfer_id.size() > 128) return false;
		auto &record = it->second;
		if (!record.transfer_id.empty()) return record.transfer_id == transfer_id;
		if (record.state != TransferCheckpointState::Prepared) return false;
		record.transfer_id = transfer_id;
		return true;
	}

	/** The source checkpoint and despawn must be saved together. */
	static bool MarkDeparted(uint32_t source_world, const std::string &request_id)
	{
		auto it = records.find({source_world, request_id});
		if (it == records.end() || it->second.transfer_id.empty()) return false;
		auto &record = it->second;
		if (record.state == TransferCheckpointState::Departed) return true;
		if (record.state != TransferCheckpointState::Prepared) return false;
		record.state = TransferCheckpointState::Departed;
		return true;
	}

	/** Destination stores a stable receipt with the physical materialisation. */
	static bool RecordArrival(const TransferCheckpoint &record)
	{
		return record.state == TransferCheckpointState::Materialized && Restore(record);
	}

	static bool ConfirmArrival(uint32_t source_world, const std::string &request_id, const std::string &receipt)
	{
		auto it = records.find({source_world, request_id});
		if (it == records.end()) return false;
		auto &record = it->second;
		if (receipt.empty() || receipt != record.arrival_receipt ||
				(record.state != TransferCheckpointState::Materialized && record.state != TransferCheckpointState::Confirmed)) return false;
		record.state = TransferCheckpointState::Confirmed;
		return true;
	}
};

#endif /* TRANSFER_JOURNAL_H */
