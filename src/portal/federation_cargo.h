/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file federation_cargo.h Portable cargo provenance attached to native cargo packets. */
#ifndef FEDERATION_CARGO_H
#define FEDERATION_CARGO_H

#include "federation_identity.h"

/** Extra identity only; native packets continue to own quantities and payment state. */
class FederationCargoRegistry {
public:
	static const GlobalCargoSourceID *Find(uint32_t packet);
	static void Set(uint32_t packet, const GlobalCargoSourceID &source);
	static void Copy(uint32_t from, uint32_t to);
	static void Release(uint32_t packet);
	static bool Same(uint32_t first, uint32_t second);
	static const std::map<uint32_t, GlobalCargoSourceID> &GetAll();
	static void Reset();
};

#endif /* FEDERATION_CARGO_H */
