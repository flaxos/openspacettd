/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file planet_sl.cpp Code handling saving and loading of planetary worlds, wormholes, and consist transit. */

#include "../stdafx.h"

#include "saveload.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/spaceport_manager.h"
#include "../portal/edge_conduit.h"
#include "../portal/federation_identity.h"
#include "../portal/transfer_journal.h"
#include "../portal/megacity_manager.h"
#include "../portal/company_stockpile.h"
#include "../portal/logistics_hub.h"
#include "../portal/corporate_hq.h"
#include "../portal/fabrication_manager.h"

#include "../safeguards.h"

/** Temporary storage for PlanetRegion serialization. */
struct SlPlanetRegion {
	uint32_t id;
	std::string name;
	uint8_t phase;
	uint8_t biome;
	uint32_t min_x;
	uint32_t min_y;
	uint32_t max_x;
	uint32_t max_y;
	uint32_t development_score;
};

static const SaveLoad _planet_region_desc[] = {
	    SLE_VAR(SlPlanetRegion, id,                VarTypes::U32),
	   SLE_SSTR(SlPlanetRegion, name,              VarTypes::STR),
	    SLE_VAR(SlPlanetRegion, phase,             VarTypes::U8),
	    SLE_VAR(SlPlanetRegion, biome,             VarTypes::U8),
	    SLE_VAR(SlPlanetRegion, min_x,             VarTypes::U32),
	    SLE_VAR(SlPlanetRegion, min_y,             VarTypes::U32),
	    SLE_VAR(SlPlanetRegion, max_x,             VarTypes::U32),
	    SLE_VAR(SlPlanetRegion, max_y,             VarTypes::U32),
	    SLE_VAR(SlPlanetRegion, development_score, VarTypes::U32),
};

/** Chunk handler for planetary worlds (PLNT). */
struct PLNTChunkHandler : ChunkHandler {
	PLNTChunkHandler() : ChunkHandler("PLNT", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_planet_region_desc);

		int i = 0;
		for (const auto &r : PlanetManager::GetAllRegions()) {
			SlPlanetRegion sl_reg{
				.id = r.id.base(),
				.name = r.name,
				.phase = to_underlying(r.phase),
				.biome = to_underlying(r.biome),
				.min_x = r.min_x,
				.min_y = r.min_y,
				.max_x = r.max_x,
				.max_y = r.max_y,
				.development_score = r.development_score,
			};
			SlSetArrayIndex(i++);
			SlObject(&sl_reg, _planet_region_desc);
		}
	}

	void Load() const override
	{
		PlanetManager::Reset();
		const std::vector<SaveLoad> slt = SlTableHeader(_planet_region_desc);

		SlPlanetRegion sl_reg{};
		while (SlIterateArray() != -1) {
			sl_reg = {};
			SlObject(&sl_reg, slt);
			PlanetRegion r{
				.id = WorldID{sl_reg.id},
				.name = sl_reg.name,
				.phase = static_cast<WorldPhase>(sl_reg.phase),
				.biome = static_cast<WorldBiome>(sl_reg.biome),
				.min_x = sl_reg.min_x,
				.min_y = sl_reg.min_y,
				.max_x = sl_reg.max_x,
				.max_y = sl_reg.max_y,
				.development_score = sl_reg.development_score,
			};
			PlanetManager::RegisterRegion(r);
		}
		PlanetManager::RebuildSpatialGrid();
	}
};

/** Temporary storage for PortalLink serialization. */
struct SlPortalLink {
	uint32_t id;
	uint32_t tile_a;
	uint8_t dir_a;
	uint32_t world_a;
	uint32_t tile_b;
	uint8_t dir_b;
	uint32_t world_b;
	uint32_t virtual_length;
	uint8_t bidirectional;
};

static const SaveLoad _portal_link_desc[] = {
	    SLE_VAR(SlPortalLink, id,             VarTypes::U32),
	    SLE_VAR(SlPortalLink, tile_a,         VarTypes::U32),
	    SLE_VAR(SlPortalLink, dir_a,          VarTypes::U8),
	    SLE_VAR(SlPortalLink, world_a,        VarTypes::U32),
	    SLE_VAR(SlPortalLink, tile_b,         VarTypes::U32),
	    SLE_VAR(SlPortalLink, dir_b,          VarTypes::U8),
	    SLE_VAR(SlPortalLink, world_b,        VarTypes::U32),
	    SLE_VAR(SlPortalLink, virtual_length, VarTypes::U32),
	    SLE_VAR(SlPortalLink, bidirectional,  VarTypes::U8),
};

/** Chunk handler for portal wormhole links (PORT). */
struct PORTChunkHandler : ChunkHandler {
	PORTChunkHandler() : ChunkHandler("PORT", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_portal_link_desc);

		int i = 0;
		for (const auto &[id, link] : PortalRegistry::GetAllPortals()) {
			SlPortalLink sl_link{
				.id = link.id.base(),
				.tile_a = link.end_a.tile.base(),
				.dir_a = to_underlying(link.end_a.enter_dir),
				.world_a = link.end_a.world_id.base(),
				.tile_b = link.end_b.tile.base(),
				.dir_b = to_underlying(link.end_b.enter_dir),
				.world_b = link.end_b.world_id.base(),
				.virtual_length = link.virtual_length,
				.bidirectional = static_cast<uint8_t>(link.bidirectional ? 1 : 0),
			};
			SlSetArrayIndex(i++);
			SlObject(&sl_link, _portal_link_desc);
		}

		for (const auto &[tile, endpoint] : PortalRegistry::GetUnlinkedGates()) {
			SlPortalLink sl_link{
				.id = 0,
				.tile_a = endpoint.tile.base(),
				.dir_a = to_underlying(endpoint.enter_dir),
				.world_a = endpoint.world_id.base(),
				.tile_b = INVALID_TILE.base(),
				.dir_b = 0,
				.world_b = INVALID_WORLD.base(),
				.virtual_length = 0,
				.bidirectional = 0,
			};
			SlSetArrayIndex(i++);
			SlObject(&sl_link, _portal_link_desc);
		}
	}

	void Load() const override
	{
		PortalRegistry::Reset();
		const std::vector<SaveLoad> slt = SlTableHeader(_portal_link_desc);

		SlPortalLink sl_link{};
		while (SlIterateArray() != -1) {
			sl_link = {};
			SlObject(&sl_link, slt);

			if (TileIndex{sl_link.tile_b} == INVALID_TILE) {
				PortalRegistry::RegisterUnlinkedGate(
					TileIndex{sl_link.tile_a},
					static_cast<DiagDirection>(sl_link.dir_a),
					WorldID{sl_link.world_a}
				);
			} else {
				PortalLink link;
				link.id = PortalID{sl_link.id};
				link.end_a = PortalEndpoint{
					TileIndex{sl_link.tile_a},
					static_cast<DiagDirection>(sl_link.dir_a),
					WorldID{sl_link.world_a},
				};
				link.end_b = PortalEndpoint{
					TileIndex{sl_link.tile_b},
					static_cast<DiagDirection>(sl_link.dir_b),
					WorldID{sl_link.world_b},
				};
				link.virtual_length = sl_link.virtual_length;
				link.bidirectional = (sl_link.bidirectional != 0);

				PortalRegistry::RestorePortalLink(link);
			}
		}
	}
};

/** Temporary storage for vehicle in-flight transit progress. */
struct SlPortalTransit {
	uint32_t veh_id;
	uint32_t progress;
};

static const SaveLoad _portal_transit_desc[] = {
	    SLE_VAR(SlPortalTransit, veh_id,   VarTypes::U32),
	    SLE_VAR(SlPortalTransit, progress, VarTypes::U32),
};

/** Chunk handler for in-flight consist transit progress (PRTX). */
struct PRTXChunkHandler : ChunkHandler {
	PRTXChunkHandler() : ChunkHandler("PRTX", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_portal_transit_desc);

		int i = 0;
		for (const auto &[veh_id, progress] : PortalRegistry::GetAllVehicleTransit()) {
			SlPortalTransit sl_transit{
				.veh_id = veh_id,
				.progress = progress,
			};
			SlSetArrayIndex(i++);
			SlObject(&sl_transit, _portal_transit_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlTableHeader(_portal_transit_desc);

		SlPortalTransit sl_transit{};
		while (SlIterateArray() != -1) {
			sl_transit = {};
			SlObject(&sl_transit, slt);
			PortalRegistry::SetVehicleTransitProgress(VehicleID{sl_transit.veh_id}, sl_transit.progress);
		}
	}
};

/** One metadata row followed by deterministic anchor-to-sequence mappings. */
struct SlFederationIdentity {
	uint8_t kind;
	uint32_t anchor_vehicle;
	uint64_t namespace_high;
	uint64_t namespace_low;
	uint64_t sequence;
	uint64_t next_sequence;
};

static const SaveLoad _federation_identity_desc[] = {
	SLE_VAR(SlFederationIdentity, kind,           VarTypes::U8),
	SLE_VAR(SlFederationIdentity, anchor_vehicle, VarTypes::U32),
	SLE_VAR(SlFederationIdentity, namespace_high, VarTypes::U64),
	SLE_VAR(SlFederationIdentity, namespace_low,  VarTypes::U64),
	SLE_VAR(SlFederationIdentity, sequence,       VarTypes::U64),
	SLE_VAR(SlFederationIdentity, next_sequence,  VarTypes::U64),
};

/** Chunk handler for federation namespace and global consist IDs (FIDS). */
struct FIDSChunkHandler : ChunkHandler {
	FIDSChunkHandler() : ChunkHandler("FIDS", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_federation_identity_desc);
		FederationNamespace name_space = FederationIdentityRegistry::GetNamespace();
		SlFederationIdentity metadata{
			.kind = 0,
			.anchor_vehicle = VehicleID::Invalid().base(),
			.namespace_high = name_space.high,
			.namespace_low = name_space.low,
			.sequence = 0,
			.next_sequence = FederationIdentityRegistry::GetNextSequence(),
		};
		SlSetArrayIndex(0);
		SlObject(&metadata, _federation_identity_desc);

		int index = 1;
		for (const auto &[anchor, sequence] : FederationIdentityRegistry::GetMappings()) {
			SlFederationIdentity mapping{
				.kind = 1,
				.anchor_vehicle = anchor,
				.namespace_high = 0,
				.namespace_low = 0,
				.sequence = sequence,
				.next_sequence = 0,
			};
			SlSetArrayIndex(index++);
			SlObject(&mapping, _federation_identity_desc);
		}

		for (const auto &[company, sequence] : FederationIdentityRegistry::GetCompanyMappings()) {
			SlFederationIdentity mapping{
				.kind = 2,
				.anchor_vehicle = company,
				.namespace_high = 0,
				.namespace_low = 0,
				.sequence = sequence,
				.next_sequence = 0,
			};
			SlSetArrayIndex(index++);
			SlObject(&mapping, _federation_identity_desc);
		}

		for (const auto &[station, sequence] : FederationIdentityRegistry::GetStationMappings()) {
			SlFederationIdentity mapping{
				.kind = 3,
				.anchor_vehicle = station,
				.namespace_high = 0,
				.namespace_low = 0,
				.sequence = sequence,
				.next_sequence = 0,
			};
			SlSetArrayIndex(index++);
			SlObject(&mapping, _federation_identity_desc);
		}

		for (const auto &[source_key, sequence] : FederationIdentityRegistry::GetSourceMappings()) {
			SlFederationIdentity mapping{
				.kind = 4,
				.anchor_vehicle = source_key,
				.namespace_high = 0,
				.namespace_low = 0,
				.sequence = sequence,
				.next_sequence = 0,
			};
			SlSetArrayIndex(index++);
			SlObject(&mapping, _federation_identity_desc);
		}

		SlFederationIdentity counters{
			.kind = 5,
			.anchor_vehicle = 0,
			.namespace_high = FederationIdentityRegistry::GetNextSourceSequence(),
			.namespace_low = 0,
			.sequence = FederationIdentityRegistry::GetNextCompanySequence(),
			.next_sequence = FederationIdentityRegistry::GetNextStationSequence(),
		};
		SlSetArrayIndex(index++);
		SlObject(&counters, _federation_identity_desc);
	}

	void Load() const override
	{
		FederationIdentityRegistry::Reset();
		const std::vector<SaveLoad> slt = SlTableHeader(_federation_identity_desc);
		SlFederationIdentity record{};
		while (SlIterateArray() != -1) {
			record = {};
			SlObject(&record, slt);
			if (record.kind == 0) {
				FederationIdentityRegistry::RestoreState({record.namespace_high, record.namespace_low}, record.next_sequence);
			} else if (record.kind == 1) {
				FederationIdentityRegistry::RestoreMapping(VehicleID{record.anchor_vehicle}, record.sequence);
			} else if (record.kind == 2) {
				FederationIdentityRegistry::RestoreCompanyMapping(CompanyID{static_cast<uint8_t>(record.anchor_vehicle)}, record.sequence);
			} else if (record.kind == 3) {
				FederationIdentityRegistry::RestoreStationMapping(StationID{static_cast<uint16_t>(record.anchor_vehicle)}, record.sequence);
			} else if (record.kind == 4) {
				FederationIdentityRegistry::RestoreSourceMapping(record.anchor_vehicle, record.sequence);
			} else if (record.kind == 5) {
				FederationIdentityRegistry::RestoreCounters(record.sequence, record.next_sequence, record.namespace_high);
			}
		}
		FederationIdentityRegistry::PruneStaleMappings();
		FederationIdentityRegistry::PruneStaleCompanyMappings();
		FederationIdentityRegistry::PruneStaleStationMappings();
		FederationIdentityRegistry::PruneStaleSourceMappings();
	}
};

/** Temporary storage for Spaceport serialization. */
struct SlSpaceport {
	uint32_t station_id;
	uint32_t world_id;
	uint32_t supplies_received;
	uint8_t offworld_trade_tier;
	uint32_t total_cargo_generated;
};

static const SaveLoad _spaceport_desc[] = {
	SLE_VAR(SlSpaceport, station_id,            VarTypes::U32),
	SLE_VAR(SlSpaceport, world_id,              VarTypes::U32),
	SLE_VAR(SlSpaceport, supplies_received,     VarTypes::U32),
	SLE_VAR(SlSpaceport, offworld_trade_tier,   VarTypes::U8),
	SLE_VAR(SlSpaceport, total_cargo_generated, VarTypes::U32),
};

/** Chunk handler for interplanetary spaceports (SPRT). */
struct SPRTChunkHandler : ChunkHandler {
	SPRTChunkHandler() : ChunkHandler("SPRT", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_spaceport_desc);

		int i = 0;
		for (const auto &[st_id, info] : SpaceportManager::GetAllSpaceports()) {
			SlSpaceport sl_sp{
				.station_id = info.station_id.base(),
				.world_id = info.world_id.base(),
				.supplies_received = info.supplies_received,
				.offworld_trade_tier = info.offworld_trade_tier,
				.total_cargo_generated = info.total_offworld_cargo_generated,
			};
			SlSetArrayIndex(i++);
			SlObject(&sl_sp, _spaceport_desc);
		}
	}

	void Load() const override
	{
		SpaceportManager::Reset();
		const std::vector<SaveLoad> slt = SlTableHeader(_spaceport_desc);

		SlSpaceport sl_sp{};
		while (SlIterateArray() != -1) {
			sl_sp = {};
			SlObject(&sl_sp, slt);
			SpaceportInfo info{
				.station_id = StationID{static_cast<uint16_t>(sl_sp.station_id)},
				.world_id = WorldID{sl_sp.world_id},
				.supplies_received = sl_sp.supplies_received,
				.offworld_trade_tier = sl_sp.offworld_trade_tier,
				.total_offworld_cargo_generated = sl_sp.total_cargo_generated,
			};
			SpaceportManager::RestoreSpaceport(info);
		}
	}
};

/** Temporary storage for EdgeConduit serialization. */
struct SlEdgeConduit {
	uint32_t id;
	uint32_t tile;
	uint8_t dir;
	uint32_t world_id;
	uint8_t cargo_type;
	uint32_t production_rate;
	uint8_t owner;
	uint32_t total_produced;
};

static const SaveLoad _edge_conduit_desc[] = {
	SLE_VAR(SlEdgeConduit, id,              VarTypes::U32),
	SLE_VAR(SlEdgeConduit, tile,            VarTypes::U32),
	SLE_VAR(SlEdgeConduit, dir,             VarTypes::U8),
	SLE_VAR(SlEdgeConduit, world_id,        VarTypes::U32),
	SLE_VAR(SlEdgeConduit, cargo_type,      VarTypes::U8),
	SLE_VAR(SlEdgeConduit, production_rate, VarTypes::U32),
	SLE_VAR(SlEdgeConduit, owner,           VarTypes::U8),
	SLE_VAR(SlEdgeConduit, total_produced,  VarTypes::U32),
};

/** Chunk handler for edge mineral extraction conduits (COND). */
struct CONDChunkHandler : ChunkHandler {
	CONDChunkHandler() : ChunkHandler("COND", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_edge_conduit_desc);

		int i = 0;
		for (const auto &[tile, conduit] : EdgeConduitManager::GetAllConduits()) {
			SlEdgeConduit sl_cond{
				.id = conduit.id,
				.tile = conduit.tile.base(),
				.dir = to_underlying(conduit.dir),
				.world_id = conduit.world_id.base(),
				.cargo_type = to_underlying(conduit.cargo_type),
				.production_rate = conduit.production_rate,
				.owner = conduit.owner.base(),
				.total_produced = conduit.total_produced,
			};
			SlSetArrayIndex(i++);
			SlObject(&sl_cond, _edge_conduit_desc);
		}
	}

	void Load() const override
	{
		EdgeConduitManager::Reset();
		const std::vector<SaveLoad> slt = SlTableHeader(_edge_conduit_desc);

		SlEdgeConduit sl_cond{};
		while (SlIterateArray() != -1) {
			sl_cond = {};
			SlObject(&sl_cond, slt);
			EdgeConduit cond{
				.id = sl_cond.id,
				.tile = TileIndex{sl_cond.tile},
				.dir = static_cast<DiagDirection>(sl_cond.dir),
				.world_id = WorldID{sl_cond.world_id},
				.cargo_type = CargoType{sl_cond.cargo_type},
				.production_rate = sl_cond.production_rate,
				.owner = Owner{sl_cond.owner},
				.total_produced = sl_cond.total_produced,
			};
			EdgeConduitManager::RestoreConduit(cond);
		}
	}
};

/** Temporary storage for Megacity serialization. */
struct SlMegacity {
	uint32_t town_id;
	uint32_t world_id;
	std::string town_name;
	uint32_t population;
	uint32_t quota_0;
	uint32_t quota_1;
	uint32_t quota_2;
	uint32_t deliv_curr_0;
	uint32_t deliv_curr_1;
	uint32_t deliv_curr_2;
	uint32_t deliv_last_0;
	uint32_t deliv_last_1;
	uint32_t deliv_last_2;
	uint8_t growth_state;
};

static const SaveLoad _megacity_desc[] = {
	    SLE_VAR(SlMegacity, town_id,      VarTypes::U32),
	    SLE_VAR(SlMegacity, world_id,     VarTypes::U32),
	   SLE_SSTR(SlMegacity, town_name,    VarTypes::STR),
	    SLE_VAR(SlMegacity, population,   VarTypes::U32),
	    SLE_VAR(SlMegacity, quota_0,      VarTypes::U32),
	    SLE_VAR(SlMegacity, quota_1,      VarTypes::U32),
	    SLE_VAR(SlMegacity, quota_2,      VarTypes::U32),
	    SLE_VAR(SlMegacity, deliv_curr_0, VarTypes::U32),
	    SLE_VAR(SlMegacity, deliv_curr_1, VarTypes::U32),
	    SLE_VAR(SlMegacity, deliv_curr_2, VarTypes::U32),
	    SLE_VAR(SlMegacity, deliv_last_0, VarTypes::U32),
	    SLE_VAR(SlMegacity, deliv_last_1, VarTypes::U32),
	    SLE_VAR(SlMegacity, deliv_last_2, VarTypes::U32),
	    SLE_VAR(SlMegacity, growth_state, VarTypes::U8),
};

/** Chunk handler for Megacities (MEGA). */
struct MEGAChunkHandler : ChunkHandler {
	MEGAChunkHandler() : ChunkHandler("MEGA", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_megacity_desc);

		int i = 0;
		for (const auto &m : MegacityManager::GetAllMegacities()) {
			SlMegacity sl_mega{
				.town_id = m.town_id.base(),
				.world_id = m.world_id.base(),
				.town_name = m.town_name,
				.population = m.population,
				.quota_0 = m.monthly_quota[0],
				.quota_1 = m.monthly_quota[1],
				.quota_2 = m.monthly_quota[2],
				.deliv_curr_0 = m.delivered_current[0],
				.deliv_curr_1 = m.delivered_current[1],
				.deliv_curr_2 = m.delivered_current[2],
				.deliv_last_0 = m.delivered_last[0],
				.deliv_last_1 = m.delivered_last[1],
				.deliv_last_2 = m.delivered_last[2],
				.growth_state = to_underlying(m.growth_state),
			};
			SlSetArrayIndex(i++);
			SlObject(&sl_mega, _megacity_desc);
		}
	}

	void Load() const override
	{
		MegacityManager::Reset();
		const std::vector<SaveLoad> slt = SlTableHeader(_megacity_desc);

		SlMegacity sl_mega{};
		while (SlIterateArray() != -1) {
			sl_mega = {};
			SlObject(&sl_mega, slt);
			MegacityProfile p{
				.town_id = TownID{static_cast<uint16_t>(sl_mega.town_id)},
				.world_id = WorldID{sl_mega.world_id},
				.town_name = sl_mega.town_name,
				.population = sl_mega.population,
				.monthly_quota = {sl_mega.quota_0, sl_mega.quota_1, sl_mega.quota_2},
				.delivered_current = {sl_mega.deliv_curr_0, sl_mega.deliv_curr_1, sl_mega.deliv_curr_2},
				.delivered_last = {sl_mega.deliv_last_0, sl_mega.deliv_last_1, sl_mega.deliv_last_2},
				.growth_state = static_cast<MegacityGrowthState>(sl_mega.growth_state),
			};
			for (size_t tier = 0; tier < 3; ++tier) {
				if (p.monthly_quota[tier] > 0) {
					p.satisfaction_pct[tier] = static_cast<float>(p.delivered_current[tier]) / static_cast<float>(p.monthly_quota[tier]);
				}
			}
			p.overall_supply_index = (p.satisfaction_pct[0] + p.satisfaction_pct[1] + p.satisfaction_pct[2]) / 3.0f;
			switch (p.growth_state) {
				case MegacityGrowthState::Starvation:
					p.growth_multiplier = 0.0f;
					p.passenger_multiplier = 0.5f;
					break;
				case MegacityGrowthState::Subsistence:
					p.growth_multiplier = 1.0f;
					p.passenger_multiplier = 1.0f;
					break;
				case MegacityGrowthState::MetropolitanBoom:
					p.growth_multiplier = 1.5f;
					p.passenger_multiplier = 1.25f;
					break;
				case MegacityGrowthState::HyperGrowth:
					p.growth_multiplier = 2.0f;
					p.passenger_multiplier = 1.5f;
					break;
			}
			MegacityManager::RestoreMegacity(p);
		}
	}
};

/** Temporary storage for Stockpile serialization. */
struct SlStockpileRecord {
	uint32_t world_id;
	uint8_t company_id;
	uint8_t cargo_type;
	uint32_t amount;
};

static const SaveLoad _stockpile_desc[] = {
	SLE_VAR(SlStockpileRecord, world_id,   VarTypes::U32),
	SLE_VAR(SlStockpileRecord, company_id, VarTypes::U8),
	SLE_VAR(SlStockpileRecord, cargo_type, VarTypes::U8),
	SLE_VAR(SlStockpileRecord, amount,     VarTypes::U32),
};

/** Chunk handler for planetary company stockpiles (STCK). */
struct STCKChunkHandler : ChunkHandler {
	STCKChunkHandler() : ChunkHandler("STCK", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_stockpile_desc);

		int i = 0;
		for (const auto &sp : StockpileManager::GetAllStockpiles()) {
			for (const auto &[cargo, amount] : sp.inventory) {
				if (amount > 0) {
					SlStockpileRecord rec{
						.world_id = sp.world_id.base(),
						.company_id = sp.company_id.base(),
						.cargo_type = to_underlying(cargo),
						.amount = amount,
					};
					SlSetArrayIndex(i++);
					SlObject(&rec, _stockpile_desc);
				}
			}
		}
	}

	void Load() const override
	{
		StockpileManager::Reset();
		const std::vector<SaveLoad> slt = SlTableHeader(_stockpile_desc);

		SlStockpileRecord rec{};
		while (SlIterateArray() != -1) {
			rec = {};
			SlObject(&rec, slt);
			StockpileManager::AddCargo(WorldID{rec.world_id}, CompanyID{rec.company_id}, CargoType{rec.cargo_type}, rec.amount);
		}
	}
};

/** Temporary storage for LogisticsHub and Reserve Floor serialization. */
struct SlLogisticsHubRecord {
	uint8_t kind; ///< 0 = Hub info, 1 = Reserve floor
	uint32_t hub_id;
	uint32_t tile;
	uint32_t world_id;
	uint8_t company_id;
	uint16_t station_id;
	std::string name;
	uint64_t total_deposited;
	uint64_t total_dispatched;
	uint8_t cargo_type;
	uint32_t min_reserve;
};

static const SaveLoad _logistics_hub_desc[] = {
	    SLE_VAR(SlLogisticsHubRecord, kind,             VarTypes::U8),
	    SLE_VAR(SlLogisticsHubRecord, hub_id,           VarTypes::U32),
	    SLE_VAR(SlLogisticsHubRecord, tile,             VarTypes::U32),
	    SLE_VAR(SlLogisticsHubRecord, world_id,         VarTypes::U32),
	    SLE_VAR(SlLogisticsHubRecord, company_id,       VarTypes::U8),
	    SLE_VAR(SlLogisticsHubRecord, station_id,       VarTypes::U16),
	   SLE_SSTR(SlLogisticsHubRecord, name,             VarTypes::STR),
	    SLE_VAR(SlLogisticsHubRecord, total_deposited,  VarTypes::U64),
	    SLE_VAR(SlLogisticsHubRecord, total_dispatched, VarTypes::U64),
	    SLE_VAR(SlLogisticsHubRecord, cargo_type,       VarTypes::U8),
	    SLE_VAR(SlLogisticsHubRecord, min_reserve,      VarTypes::U32),
};

/** Chunk handler for dedicated company logistics hubs (LHUB). */
struct LHUBChunkHandler : ChunkHandler {
	LHUBChunkHandler() : ChunkHandler("LHUB", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_logistics_hub_desc);

		int i = 0;
		for (const auto &hub : LogisticsHubManager::GetAllHubs()) {
			SlLogisticsHubRecord hub_rec{
				.kind = 0,
				.hub_id = hub.hub_id,
				.tile = hub.tile.base(),
				.world_id = hub.world_id.base(),
				.company_id = hub.company_id.base(),
				.station_id = hub.station_id.base(),
				.name = hub.name,
				.total_deposited = hub.total_deposited,
				.total_dispatched = hub.total_dispatched,
				.cargo_type = 0,
				.min_reserve = 0,
			};
			SlSetArrayIndex(i++);
			SlObject(&hub_rec, _logistics_hub_desc);

			for (const auto &[cargo, min_reserve] : hub.reserve_floors) {
				SlLogisticsHubRecord res_rec{
					.kind = 1,
					.hub_id = hub.hub_id,
					.tile = 0,
					.world_id = 0,
					.company_id = 0,
					.station_id = 0,
					.name = "",
					.total_deposited = 0,
					.total_dispatched = 0,
					.cargo_type = to_underlying(cargo),
					.min_reserve = min_reserve,
				};
				SlSetArrayIndex(i++);
				SlObject(&res_rec, _logistics_hub_desc);
			}
		}
	}

	void Load() const override
	{
		LogisticsHubManager::Reset();
		const std::vector<SaveLoad> slt = SlTableHeader(_logistics_hub_desc);

		SlLogisticsHubRecord rec{};
		while (SlIterateArray() != -1) {
			rec = {};
			SlObject(&rec, slt);

			if (rec.kind == 0) {
				LogisticsHub hub{
					.hub_id = rec.hub_id,
					.tile = TileIndex{rec.tile},
					.world_id = WorldID{rec.world_id},
					.company_id = CompanyID{rec.company_id},
					.station_id = StationID{rec.station_id},
					.name = rec.name,
					.reserve_floors = {},
					.total_deposited = rec.total_deposited,
					.total_dispatched = rec.total_dispatched,
				};
				LogisticsHubManager::RestoreHub(hub);
			} else if (rec.kind == 1) {
				LogisticsHubManager::SetReserveFloor(rec.hub_id, CargoType{rec.cargo_type}, rec.min_reserve);
			}
		}
	}
};

/** Temporary storage for Corporate Headquarters serialization. */
struct SlCorporateHQRecord {
	uint8_t company_id;
	uint32_t world_id;
	uint32_t tile;
	uint8_t tier;
	std::string campus_name;
	uint64_t founding_date;
};

static const SaveLoad _corporate_hq_desc[] = {
	    SLE_VAR(SlCorporateHQRecord, company_id,    VarTypes::U8),
	    SLE_VAR(SlCorporateHQRecord, world_id,      VarTypes::U32),
	    SLE_VAR(SlCorporateHQRecord, tile,          VarTypes::U32),
	    SLE_VAR(SlCorporateHQRecord, tier,          VarTypes::U8),
	   SLE_SSTR(SlCorporateHQRecord, campus_name,   VarTypes::STR),
	    SLE_VAR(SlCorporateHQRecord, founding_date, VarTypes::U64),
};

/** Chunk handler for Corporate Headquarters (CHQS). */
struct CHQSChunkHandler : ChunkHandler {
	CHQSChunkHandler() : ChunkHandler("CHQS", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_corporate_hq_desc);

		int i = 0;
		for (const auto &hq : CorporateHQManager::GetAllHQ()) {
			SlCorporateHQRecord rec{
				.company_id = hq.company_id.base(),
				.world_id = hq.world_id.base(),
				.tile = hq.tile.base(),
				.tier = to_underlying(hq.tier),
				.campus_name = hq.campus_name,
				.founding_date = hq.founding_date,
			};
			SlSetArrayIndex(i++);
			SlObject(&rec, _corporate_hq_desc);
		}
	}

	void Load() const override
	{
		CorporateHQManager::Reset();
		const std::vector<SaveLoad> slt = SlTableHeader(_corporate_hq_desc);

		SlCorporateHQRecord rec{};
		while (SlIterateArray() != -1) {
			rec = {};
			SlObject(&rec, slt);
			CorporateHQProfile hq{
				.company_id = CompanyID{rec.company_id},
				.world_id = WorldID{rec.world_id},
				.tile = TileIndex{rec.tile},
				.tier = static_cast<CorporateHQTier>(rec.tier),
				.campus_name = rec.campus_name,
				.founding_date = rec.founding_date,
			};
			CorporateHQManager::RestoreHQ(hq);
		}
	}
};

struct SlFabricationModeRecord {
	uint8_t company_id;
	uint8_t enabled;
};

static const SaveLoad _fabrication_mode_desc[] = {
	SLE_VAR(SlFabricationModeRecord, company_id, VarTypes::U8),
	SLE_VAR(SlFabricationModeRecord, enabled,    VarTypes::U8),
};

/** Chunk handler for Company Fabrication Modes (FABR). */
struct FABRChunkHandler : ChunkHandler {
	FABRChunkHandler() : ChunkHandler("FABR", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_fabrication_mode_desc);

		int i = 0;
		for (const auto &[comp, enabled] : FabricationManager::GetAllCompanyModes()) {
			SlFabricationModeRecord rec{
				.company_id = comp.base(),
				.enabled = static_cast<uint8_t>(enabled ? 1 : 0),
			};
			SlSetArrayIndex(i++);
			SlObject(&rec, _fabrication_mode_desc);
		}
	}

	void Load() const override
	{
		FabricationManager::Reset();
		const std::vector<SaveLoad> slt = SlTableHeader(_fabrication_mode_desc);

		SlFabricationModeRecord rec{};
		while (SlIterateArray() != -1) {
			rec = {};
			SlObject(&rec, slt);
			FabricationManager::RestoreCompanyMode(CompanyID{rec.company_id}, rec.enabled != 0);
		}
	}
};

static const SaveLoad _transfer_checkpoint_desc[] = {
	SLE_SSTR(TransferCheckpoint, request_id, VarTypes::STR),
	SLE_SSTR(TransferCheckpoint, transfer_id, VarTypes::STR),
	SLE_SSTR(TransferCheckpoint, arrival_receipt, VarTypes::STR),
	SLE_VAR(TransferCheckpoint, namespace_high, VarTypes::U64),
	SLE_VAR(TransferCheckpoint, namespace_low, VarTypes::U64),
	SLE_VAR(TransferCheckpoint, consist_sequence, VarTypes::U64),
	SLE_VAR(TransferCheckpoint, source_world, VarTypes::U32),
	SLE_VAR(TransferCheckpoint, destination_world, VarTypes::U32),
	SLE_VAR(TransferCheckpoint, state, VarTypes::U8),
	SLE_CONDVECTOR(TransferCheckpoint, snapshot, VarTypes::U8, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion),
};

struct FTJRChunkHandler : ChunkHandler {
	FTJRChunkHandler() : ChunkHandler("FTJR", ChunkType::Table) {}
	void Save() const override
	{
		SlTableHeader(_transfer_checkpoint_desc);
		int index = 0;
		for (const auto &[key, record] : TransferJournal::GetAll()) {
			SlSetArrayIndex(index++);
			auto copy = record;
			SlObject(&copy, _transfer_checkpoint_desc);
		}
	}
	void Load() const override
	{
		TransferJournal::Reset();
		const auto table = SlTableHeader(_transfer_checkpoint_desc);
		while (SlIterateArray() != -1) {
			TransferCheckpoint record;
			SlObject(&record, table);
			if (!TransferJournal::Restore(record)) SlErrorCorrupt("Invalid or conflicting federation checkpoint");
		}
	}
};

/** Temporary storage for InterServerPortalLink serialization. */
struct SlInterServerPortal {
	uint32_t id;
	uint32_t tile;
	uint8_t enter_dir;
	uint32_t local_world;
	uint32_t remote_world;
	uint32_t remote_gate_id;
	uint32_t virtual_length;
};

static const SaveLoad _interserver_portal_desc[] = {
	SLE_VAR(SlInterServerPortal, id,             VarTypes::U32),
	SLE_VAR(SlInterServerPortal, tile,           VarTypes::U32),
	SLE_VAR(SlInterServerPortal, enter_dir,      VarTypes::U8),
	SLE_VAR(SlInterServerPortal, local_world,    VarTypes::U32),
	SLE_VAR(SlInterServerPortal, remote_world,   VarTypes::U32),
	SLE_VAR(SlInterServerPortal, remote_gate_id, VarTypes::U32),
	SLE_VAR(SlInterServerPortal, virtual_length, VarTypes::U32),
};

/** Chunk handler for inter-server portal links (ISPR). */
struct ISPRChunkHandler : ChunkHandler {
	ISPRChunkHandler() : ChunkHandler("ISPR", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_interserver_portal_desc);
		int i = 0;
		for (const auto &[tile, link] : PortalRegistry::GetAllInterServerPortals()) {
			SlInterServerPortal sl_link{
				.id = link.id.base(),
				.tile = link.local_endpoint.tile.base(),
				.enter_dir = to_underlying(link.local_endpoint.enter_dir),
				.local_world = link.local_endpoint.world_id.base(),
				.remote_world = link.remote_world.base(),
				.remote_gate_id = link.remote_gate_id,
				.virtual_length = link.virtual_length,
			};
			SlSetArrayIndex(i++);
			SlObject(&sl_link, _interserver_portal_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlTableHeader(_interserver_portal_desc);
		SlInterServerPortal sl_link{};
		while (SlIterateArray() != -1) {
			sl_link = {};
			SlObject(&sl_link, slt);
			InterServerPortalLink link;
			link.id = PortalID{sl_link.id};
			link.local_endpoint = PortalEndpoint{
				TileIndex{sl_link.tile},
				static_cast<DiagDirection>(sl_link.enter_dir),
				WorldID{sl_link.local_world}
			};
			link.remote_world = WorldID{sl_link.remote_world};
			link.remote_gate_id = sl_link.remote_gate_id;
			link.virtual_length = sl_link.virtual_length;
			PortalRegistry::RestoreInterServerPortal(link);
		}
	}
};

static const FTJRChunkHandler FTJR;
static const ISPRChunkHandler ISPR;
static const PLNTChunkHandler PLNT;
static const PORTChunkHandler PORT;
static const PRTXChunkHandler PRTX;
static const FIDSChunkHandler FIDS;
static const SPRTChunkHandler SPRT;
static const CONDChunkHandler COND;
static const MEGAChunkHandler MEGA;
static const STCKChunkHandler STCK;
static const LHUBChunkHandler LHUB;
static const CHQSChunkHandler CHQS;
static const FABRChunkHandler FABR;

static const ChunkHandlerRef planet_chunk_handlers[] = {
	PLNT,
	PORT,
	ISPR,
	PRTX,
	FIDS,
	SPRT,
	COND,
	MEGA,
	STCK,
	LHUB,
	CHQS,
	FABR,
	FTJR,
};

extern const ChunkHandlerTable _planet_chunk_handlers(planet_chunk_handlers);
