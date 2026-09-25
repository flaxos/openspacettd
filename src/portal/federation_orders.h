/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file federation_orders.h Native station-order routing through federation gates. */
#ifndef FEDERATION_ORDERS_H
#define FEDERATION_ORDERS_H

#include "../tile_type.h"
#include <optional>

struct Vehicle;
struct Order;

/** Resolve an explicitly mapped remote station to its local gate.
 * A present INVALID_TILE means the remote route is unavailable, so callers must
 * hold the train rather than falling back to the local station's platform.
 */
std::optional<TileIndex> GetFederationOrderGate(const Vehicle *vehicle, const Order *order);

#endif /* FEDERATION_ORDERS_H */
