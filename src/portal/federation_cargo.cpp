/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file federation_cargo.cpp Preserve remote cargo identities through native packet operations. */

#include "../stdafx.h"
#include "federation_cargo.h"
#include "../safeguards.h"

static std::map<uint32_t, GlobalCargoSourceID> _federation_cargo_sources;

const GlobalCargoSourceID *FederationCargoRegistry::Find(uint32_t packet)
{
	auto found = _federation_cargo_sources.find(packet);
	return found == _federation_cargo_sources.end() ? nullptr : &found->second;
}

void FederationCargoRegistry::Set(uint32_t packet, const GlobalCargoSourceID &source)
{
	if (source.IsValid()) _federation_cargo_sources[packet] = source;
	else Release(packet);
}

void FederationCargoRegistry::Copy(uint32_t from, uint32_t to)
{
	if (const auto *source = Find(from)) Set(to, *source);
	else Release(to);
}

void FederationCargoRegistry::Release(uint32_t packet)
{
	_federation_cargo_sources.erase(packet);
}

bool FederationCargoRegistry::Same(uint32_t first, uint32_t second)
{
	const auto *a = Find(first);
	const auto *b = Find(second);
	return a == nullptr ? b == nullptr : b != nullptr && *a == *b;
}

const std::map<uint32_t, GlobalCargoSourceID> &FederationCargoRegistry::GetAll()
{
	return _federation_cargo_sources;
}

void FederationCargoRegistry::Reset()
{
	_federation_cargo_sources.clear();
}
