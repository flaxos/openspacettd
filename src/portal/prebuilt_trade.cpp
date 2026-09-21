/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file prebuilt_trade.cpp Implementation of prebuilt simulated trade gateways and economy proxy engine for Sprint 49. */

#include "../stdafx.h"
#include "prebuilt_trade.h"
#include "consist_materializer.h"
#include "portal_registry.h"
#include "universe_authority.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../train.h"
#include "../tunnelbridge_map.h"
#include "../core/format.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

/**
 * Get singleton instance of PrebuiltTradeManager.
 * @return Reference to the PrebuiltTradeManager singleton.
 */
PrebuiltTradeManager &PrebuiltTradeManager::Instance()
{
	static PrebuiltTradeManager instance;
	return instance;
}

void PrebuiltTradeManager::Reset()
{
	this->_next_trade_seq = 1;
	this->_gateways.clear();
	this->_scheduled_returns.clear();
}

bool PrebuiltTradeManager::RegisterTradeGateway(
	TileIndex portal_tile,
	const std::string &target_world_id,
	WorldID local_world,
	uint32_t virtual_length)
{
	if (portal_tile == INVALID_TILE || target_world_id.empty()) return false;

	const UniverseNode *node = UniverseGraphManager::Instance().FindNode(target_world_id);
	std::string display_name = (node != nullptr) ? node->display_name : target_world_id;
	float multiplier = (node != nullptr) ? node->economic_profile.tariff_multiplier : 1.0f;

	PrebuiltTradeGateway gw;
	gw.portal_tile = portal_tile;
	gw.gate_id = portal_tile.base();
	gw.local_world = local_world;
	gw.target_world_id = target_world_id;
	gw.target_world_name = display_name;
	gw.virtual_length_tiles = std::max(1U, virtual_length);
	gw.tariff_multiplier = multiplier;
	gw.active = true;

	this->_gateways[portal_tile] = gw;
	return true;
}

bool PrebuiltTradeManager::UnregisterTradeGateway(TileIndex portal_tile)
{
	return this->_gateways.erase(portal_tile) > 0;
}

bool PrebuiltTradeManager::IsTradeGateway(TileIndex portal_tile) const
{
	auto it = this->_gateways.find(portal_tile);
	return it != this->_gateways.end() && it->second.active;
}

const PrebuiltTradeGateway *PrebuiltTradeManager::GetTradeGateway(TileIndex portal_tile) const
{
	auto it = this->_gateways.find(portal_tile);
	return it != this->_gateways.end() ? &it->second : nullptr;
}

std::vector<PrebuiltTradeGateway> PrebuiltTradeManager::GetAllTradeGateways() const
{
	std::vector<PrebuiltTradeGateway> result;
	result.reserve(this->_gateways.size());
	for (const auto &[tile, gw] : this->_gateways) {
		result.push_back(gw);
	}
	return result;
}

int64_t PrebuiltTradeManager::CalculateFreightTariff(
	uint32_t cargo_units,
	float tariff_multiplier,
	uint32_t virtual_length_tiles) noexcept
{
	if (cargo_units == 0) return 0;
	float dist_factor = 1.0f + (static_cast<float>(virtual_length_tiles) / 100.0f);
	float revenue = static_cast<float>(cargo_units) * static_cast<float>(BASE_TARIFF_PER_UNIT) * tariff_multiplier * dist_factor;
	return std::max(static_cast<int64_t>(1), static_cast<int64_t>(revenue));
}

CommonwealthCargoID PrebuiltTradeManager::ResolveExportCargoID(const std::string &export_name) noexcept
{
	if (export_name == "STRUCTURAL_STEEL") return CommonwealthCargoID::StructuralSteel;
	if (export_name == "SUPERALLOYS") return CommonwealthCargoID::Superalloys;
	if (export_name == "QUANTUM_CRYSTALS") return CommonwealthCargoID::EnrichedQuantumCrystals;
	if (export_name == "CONSUMER_CRYSTALS") return CommonwealthCargoID::EncryptedConsumerCrystals;
	if (export_name == "BLANK_CRYSTALS") return CommonwealthCargoID::BlankCrystals;
	if (export_name == "SILICON_CHIPS") return CommonwealthCargoID::SiliconChips;
	if (export_name == "IRON_ORE") return CommonwealthCargoID::IronOre;
	if (export_name == "COPPER_ORE") return CommonwealthCargoID::CopperOre;
	if (export_name == "WIRING") return CommonwealthCargoID::ConductiveWiring;
	if (export_name == "SYNTHETIC_COMPOSITES") return CommonwealthCargoID::SyntheticComposites;
	if (export_name == "SILICA_SAND") return CommonwealthCargoID::SilicaSand;
	if (export_name == "RARE_EARTHS") return CommonwealthCargoID::RareEarthMinerals;
	return CommonwealthCargoID::StructuralSteel;
}

std::string PrebuiltTradeManager::DispatchOutboundConsist(
	TileIndex portal_tile,
	const ConsistSnapshot &consist,
	uint64_t current_tick,
	CompanyID company)
{
	auto it = this->_gateways.find(portal_tile);
	if (it == this->_gateways.end() || !it->second.active) {
		return "";
	}

	PrebuiltTradeGateway &gw = it->second;
	const UniverseNode *node = UniverseGraphManager::Instance().FindNode(gw.target_world_id);

	/* Count outbound cargo units */
	uint32_t total_cargo = 0;
	CommonwealthCargoID primary_outbound_cargo = CommonwealthCargoID::StructuralSteel;
	for (const auto &unit : consist.units) {
		total_cargo += unit.cargo_count;
		if (unit.cargo_count > 0 && unit.cargo_type < static_cast<uint8_t>(CommonwealthCargoID::Count)) {
			primary_outbound_cargo = static_cast<CommonwealthCargoID>(unit.cargo_type);
		}
	}

	/* Calculate freight tariff credits */
	float multiplier = (node != nullptr) ? node->economic_profile.tariff_multiplier : gw.tariff_multiplier;
	int64_t tariff = CalculateFreightTariff(total_cargo, multiplier, gw.virtual_length_tiles);

	/* Credit owning company */
	if (Company::IsValidID(company)) {
		Company *c = Company::Get(company);
		if (c != nullptr) {
			c->money += tariff;
		}
	}

	/* Record financial clearing with Universe Authority */
	UniverseAuthorityService::Instance().RecordFinancialClearing(gw.local_world, INVALID_WORLD, company, tariff);
	UniverseAuthorityService::Instance().RecordSpaceportThroughput(total_cargo);

	/* Determine return cargo based on partner world's lore profile */
	CommonwealthCargoID return_cargo = CommonwealthCargoID::EnrichedQuantumCrystals;
	if (node != nullptr && !node->economic_profile.primary_exports.empty()) {
		return_cargo = ResolveExportCargoID(node->economic_profile.primary_exports.front());
	} else if (node != nullptr) {
		if (node->phase == WorldPhase::Phase1_Core) return_cargo = CommonwealthCargoID::EnrichedQuantumCrystals;
		else if (node->phase == WorldPhase::Phase2_Developed) return_cargo = CommonwealthCargoID::Superalloys;
		else return_cargo = CommonwealthCargoID::BlankCrystals;
	}

	/* Build scheduled return consist */
	ConsistSnapshot return_snap = consist;
	uint32_t return_cargo_total = 0;
	for (auto &unit : return_snap.units) {
		if (unit.cargo_capacity > 0) {
			unit.cargo_type = static_cast<uint8_t>(return_cargo);
			unit.cargo_count = unit.cargo_capacity;
			return_cargo_total += unit.cargo_count;
		}
	}

	std::ostringstream ss;
	ss << "TRADE-X" << std::setw(8) << std::setfill('0') << this->_next_trade_seq++;
	std::string trade_id = ss.str();

	ScheduledTradeReturn ret;
	ret.trade_id = trade_id;
	ret.portal_tile = portal_tile;
	ret.local_world = gw.local_world;
	ret.target_world_id = gw.target_world_id;
	ret.target_world_name = gw.target_world_name;
	ret.dispatch_tick = current_tick;
	ret.arrival_tick = current_tick + gw.virtual_length_tiles;
	ret.tariff_credited = tariff;
	ret.exported_cargo_id = primary_outbound_cargo;
	ret.exported_cargo_units = total_cargo;
	ret.return_cargo_id = return_cargo;
	ret.return_cargo_units = return_cargo_total;
	ret.return_snapshot = std::move(return_snap);
	ret.status = TradeTransactionStatus::Queued;

	this->_scheduled_returns[trade_id] = std::move(ret);

	/* Update gateway lifetime metrics */
	gw.total_trains_exported++;
	gw.total_cargo_exported += total_cargo;
	gw.total_tariffs_earned += tariff;

	return trade_id;
}

size_t PrebuiltTradeManager::ProcessScheduledReturns(uint64_t current_tick)
{
	size_t completed = 0;
	for (auto &[id, ret] : this->_scheduled_returns) {
		if (ret.status != TradeTransactionStatus::Arrived && current_tick >= ret.arrival_tick) {
			/* Materialize train onto return portal track if map is initialized, tile is tunnel, and throat is clear */
			if (ret.portal_tile != INVALID_TILE && Map::Size() > 0 && ret.portal_tile < Map::Size() && IsTunnelTile(ret.portal_tile)) {
				DiagDirection enter_dir = ReverseDiagDir(GetTunnelBridgeDirection(ret.portal_tile));
				if (!ConsistMaterializer::CheckThroatClearance(ret.portal_tile, enter_dir)) {
					/* Throat is temporarily occupied; retry on next tick */
					continue;
				}
				ConsistMaterializer::MaterializeFromTransfer(ret.return_snapshot, ret.portal_tile, enter_dir);
			}

			ret.status = TradeTransactionStatus::Arrived;

			/* Update gateway lifetime import metrics */
			auto it_gw = this->_gateways.find(ret.portal_tile);
			if (it_gw != this->_gateways.end()) {
				it_gw->second.total_trains_imported++;
				it_gw->second.total_cargo_imported += ret.return_cargo_units;
			}

			completed++;
		}
	}
	return completed;
}

void PrebuiltTradeManager::RestoreGateway(const PrebuiltTradeGateway &gw)
{
	this->_gateways[gw.portal_tile] = gw;
}

const ScheduledTradeReturn *PrebuiltTradeManager::GetTradeReturn(const std::string &trade_id) const
{
	auto it = this->_scheduled_returns.find(trade_id);
	return it != this->_scheduled_returns.end() ? &it->second : nullptr;
}

std::vector<ScheduledTradeReturn> PrebuiltTradeManager::GetPendingReturns() const
{
	std::vector<ScheduledTradeReturn> res;
	for (const auto &[id, ret] : this->_scheduled_returns) {
		if (ret.status != TradeTransactionStatus::Arrived) {
			res.push_back(ret);
		}
	}
	return res;
}
