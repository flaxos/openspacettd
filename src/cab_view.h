/* This file is part of OpenSpaceTTD. Distributed under GNU GPL version 2. */
/** @file cab_view.h Local, read-only train cab presentation state. */
#ifndef CAB_VIEW_H
#define CAB_VIEW_H

#include "vehicle_type.h"
#include "company_type.h"
#include "tile_type.h"
#include "direction_type.h"
#include "portal/portal_type.h"
#include "core/geometry_type.hpp"
#include <algorithm>
#include <cstdint>

struct Vehicle;

/** An immutable snapshot of the authoritative train pose for one GUI sample. */
enum class CabViewPhase : uint8_t { Unsupported, Scene, LocalTransit };
/** Transient visual preference of one cab window; never saved or sent to the game. */
enum class CabViewEffectMode : uint8_t { Full, Reduced, Off };

/** Presentation-only arrival cue on a newly validated destination scene. */
enum class CabViewSceneEffect : uint8_t { None, Arrival };

struct CabViewTransitStats {
	uint64_t effect_us = 0;
	uint64_t total_us = 0;
};

struct CabViewPresentation {
	CabViewPhase phase = CabViewPhase::Unsupported;
	WorldID world = INVALID_WORLD;
	TileIndex tile = INVALID_TILE;
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;
	Direction moving = Direction::Invalid;

	bool operator==(const CabViewPresentation &) const = default;
};

/** Window-owned presentation clock. Observe from paint or tick BEFORE charging
 * a callback delta, so a phase first seen by either cannot inherit old time. */
struct CabViewEffectTimeline {
	uint64_t clock_ms = 0;
	uint64_t phase_epoch_ms = 0;
	bool arrival_active = false;
	bool skip_next_delta = false;

	void Observe(const CabViewPresentation &before, const CabViewPresentation &after, bool had_previous, bool reset)
	{
		if (had_previous && !reset) return;
		this->arrival_active = had_previous && after.phase == CabViewPhase::Scene &&
			(before.phase == CabViewPhase::LocalTransit || before.world != after.world);
		this->phase_epoch_ms = this->clock_ms;
		this->skip_next_delta = true;
	}
	void Advance(uint32_t delta_ms)
	{
		if (this->skip_next_delta) this->skip_next_delta = false;
		else this->clock_ms += delta_ms;
	}
	uint32_t PhaseElapsed() const { return static_cast<uint32_t>(std::min<uint64_t>(this->clock_ms - this->phase_epoch_ms, UINT32_MAX)); }
	bool ArrivalCue() const { return this->arrival_active && this->clock_ms - this->phase_epoch_ms < 250; }
	void CancelCue() { this->arrival_active = false; this->phase_epoch_ms = this->clock_ms; }
};

/** Classify a physically linked local head and both worlds without retaining a registry pointer. */
bool CabViewLocalGateVisible(TileIndex tile, WorldID selected_world);
/** Resolve current presentation from live authoritative state, without game mutation. */
CabViewPresentation ResolveCabViewPresentation(const Vehicle *v, CompanyID owner);
/** A transition invalidates every cached scene/depth/frame and pose history. */
bool CabViewTransitionRequiresReset(const CabViewPresentation &before, const CabViewPresentation &after);
/** Some discontinuities also recenter the user's transient look control. */
bool CabViewTransitionResetsLook(const CabViewPresentation &before, const CabViewPresentation &after);

bool CanShowCabView(const Vehicle *v);
void ShowCabViewWindow(const Vehicle *v);
/** Opaque local transit drawing, shared by the live widget and isolated profiling. */
CabViewTransitStats DrawCabViewTransit(const Rect &r, CabViewEffectMode mode, uint32_t elapsed_ms);

#endif
