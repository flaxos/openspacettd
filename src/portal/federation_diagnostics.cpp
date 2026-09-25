/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file federation_diagnostics.cpp Read-only evidence for native scheduled freight. */

#include "../stdafx.h"
#include "federation_diagnostics.h"
#include "federation_cargo.h"
#include "federation_identity.h"
#include "transfer_journal.h"
#include "../3rdparty/nlohmann/json.hpp"
#include "../cargotype.h"
#include "../company_base.h"
#include "../console_func.h"
#include "../industry.h"
#include "../station_base.h"
#include "../train.h"
#include "../order_base.h"
#include "../timer/timer_game_calendar.h"
#include "../safeguards.h"

bool ConFederationFreightStatus([[maybe_unused]] std::span<std::string_view> argv)
{
	const CargoType coal = GetCargoTypeByLabel(CargoLabel{"COAL"});
	if (!IsValidCargoType(coal)) return false;
	using nlohmann::json;
	json result{{"date", TimerGameCalendar::date.base()}, {"coal", to_underlying(coal)},
		{"companies", json::array()}, {"trains", json::array()}, {"receipts", json::array()}};
	uint64_t produced = 0, transported = 0, accepted = 0, waiting = 0, onboard = 0;
	for (const Industry *industry : Industry::Iterate()) {
		for (const auto &cargo : industry->produced) if (cargo.cargo == coal) {
			for (uint i = 0; i < HISTORY_MONTH.last; ++i) {
				produced += cargo.history[i].production;
				transported += cargo.history[i].transported;
			}
		}
		for (const auto &cargo : industry->accepted) if (cargo.cargo == coal && cargo.history != nullptr) {
			for (uint i = 0; i < HISTORY_MONTH.last; ++i) accepted += (*cargo.history)[i].accepted;
		}
	}
	for (const Station *station : Station::Iterate()) waiting += station->goods[coal].TotalCount();
	for (const Company *company : Company::Iterate()) {
		int64_t income = company->cur_economy.income.base(), expenses = company->cur_economy.expenses.base();
		uint64_t delivered = company->cur_economy.delivered_cargo[coal];
		for (uint i = 0; i < company->num_valid_stat_ent; ++i) {
			income += company->old_economy[i].income.base();
			expenses += company->old_economy[i].expenses.base();
			delivered += company->old_economy[i].delivered_cargo[coal];
		}
		const auto identity = FederationIdentityRegistry::FindCompany(company->index);
		result["companies"].push_back({{"company", company->index.base()}, {"money", company->money.base()},
			{"income", income}, {"expenses", expenses}, {"delivered", delivered},
			{"global_id", identity ? fmt::format("{:x}:{:x}:{}", identity->name_space.high, identity->name_space.low, identity->sequence) : ""}});
	}
	for (const Train *train : Train::Iterate()) if (train->IsFrontEngine()) {
		json units = json::array(), orders = json::array();
		for (const Train *unit = train; unit != nullptr; unit = unit->Next()) {
			if (unit->cargo_type == coal) onboard += unit->cargo.StoredCount();
			json packets = json::array();
			for (const CargoPacket *packet : *unit->cargo.Packets()) {
				const auto *source = FederationCargoRegistry::Find(packet->index.base());
				packets.push_back({{"count", packet->Count()}, {"age", packet->GetPeriodsInTransit()},
					{"feeder_share", packet->GetFeederShare().base()},
					{"origin_world", source ? source->origin_world.base() : 0},
					{"origin_id", source ? fmt::format("{:x}:{:x}:{}", source->name_space.high, source->name_space.low, source->source_sequence) : ""}});
			}
			units.push_back({{"engine", unit->engine_type.base()}, {"cargo", unit->cargo.StoredCount()},
				{"owner", unit->owner.base()}, {"packets", packets}});
		}
		for (const Order &order : train->Orders()) {
			const auto station = order.IsType(OT_GOTO_STATION) ? FederationIdentityRegistry::FindStation(order.GetDestination().ToStationID()) : std::nullopt;
			orders.push_back({{"type", order.GetType()}, {"load", to_underlying(order.GetLoadType())},
				{"world", station ? station->world_id.base() : 0},
				{"station", station ? fmt::format("{:x}:{:x}:{}", station->name_space.high, station->name_space.low, station->sequence) : ""}});
		}
		const auto identity = FederationIdentityRegistry::Find(train);
		result["trains"].push_back({{"vehicle", train->index.base()}, {"tile", train->tile.base()},
			{"active_order", train->cur_real_order_index}, {"units", units}, {"orders", orders},
			{"global_id", identity ? fmt::format("{:x}:{:x}:{}", identity->name_space.high, identity->name_space.low, identity->sequence) : ""}});
	}
	for (const auto &[key, checkpoint] : TransferJournal::GetAll()) {
		if (checkpoint.state == TransferCheckpointState::Materialized || checkpoint.state == TransferCheckpointState::Confirmed) result["receipts"].push_back(checkpoint.transfer_id);
	}
	result["produced"] = produced;
	result["transported"] = transported;
	result["accepted"] = accepted;
	result["waiting"] = waiting;
	result["onboard"] = onboard;
	IConsolePrint(CC_DEFAULT, "Federation freight {}", result.dump());
	return true;
}
