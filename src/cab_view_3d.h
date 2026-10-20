/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file cab_view_3d.h CPU cab renderer adapted from vaclavloubek/OpenTTD tag 15.3-cabview-alpha1 (GPL version 2). */

#ifndef CAB_VIEW_3D_H
#define CAB_VIEW_3D_H

#include "core/geometry_type.hpp"
#include "tile_type.h"
#include "vehicle_type.h"
#include "gfx_type.h"
#include "portal/planet_type.h"
#include "cab_view.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

struct VehicleSpriteSeq;

/** Smallest size of the picture of the 3D view compared to the view, when drawing at full size takes too long. */
constexpr float CAB_VIEW_3D_MIN_SCALE = 0.5f;
/** How far below the level of its tile the 3D view shows the surface of water, so canals, rivers and the sea have banks. */
constexpr int CAB_VIEW_3D_WATER_DROP = 1;
/** Height of the eye of the 3D view from a ship above the water. */
constexpr float CAB_VIEW_3D_SHIP_EYE = 4.0f;

/** Where the camera of the 3D cab view is and where it looks. */
struct CabViewCamera {
	float x = 0; ///< X position of the eye, in world coordinates.
	float y = 0; ///< Y position of the eye, in world coordinates.
	float z = 0; ///< Height of the eye, in world coordinates.
	float yaw = 0; ///< Direction of view in radians; 0 looks towards +x, pi/2 towards +y.
	float pitch = 0; ///< Tilt of the view in radians, upwards positive, on top of the slight look down of the 3D view.
	float fov = 64.0f; ///< Horizontal field of view in degrees.
	float altitude = 0; ///< How high above the ground an aircraft flies; the haze then starts further away, as the ground is seen from above.
	VehicleID vehicle = VehicleID::Invalid(); ///< The vehicle the camera rides on, which is not drawn.
	WorldID world = INVALID_WORLD; ///< World selected when this cab window opened.
	bool dark = false; ///< Nothing to see, e.g. in a depot.
	TileIndex tunnel = INVALID_TILE; ///< An entrance of the tunnel the train is in, whose inside the view shows.
	CabViewEffectMode effect_mode = CabViewEffectMode::Off;
	CabViewSceneEffect scene_effect = CabViewSceneEffect::None;
	uint32_t effect_elapsed_ms = 0; ///< Window-local callback time, not game time.

	bool operator==(const CabViewCamera &) const = default;
};

/** Read-only timings and geometry for the most recent renderer call. */
struct CabViewFrameStats {
	bool rendered = false; ///< A new scene was built, rather than reusing its frame.
	bool cache_hit = false; ///< The previous scene was copied to the output again.
	uint32_t frame_id = 0;
	int view_width = 0;
	int view_height = 0;
	int internal_width = 0;
	int internal_height = 0;
	float scale = 0; ///< Actual internal width divided by view width.
	size_t visible_tiles = 0;
	size_t triangles = 0;
	int threads = 0;
	uint64_t prepare_us = 0;
	uint64_t raster_us = 0;
	uint64_t effect_us = 0; ///< Reserved for presentation effects; zero in baseline.
	uint64_t output_us = 0;
	uint64_t total_us = 0;
};

/** Vector-capacity payload estimates, excluding container/node overhead. */
struct CabViewCacheStats {
	size_t raw_sprites = 0;
	size_t ground_textures = 0; ///< Legacy count of the shared ground/composite map, including vehicle entries.
	size_t vehicle_textures = 0; ///< Subset of ground_textures, not an additional cache.
	size_t box_textures = 0;
	size_t building_shapes = 0;
	size_t cache_entries = 0;
	size_t raw_sprite_payload_bytes = 0;
	size_t texture_payload_bytes = 0;
	size_t vehicle_texture_payload_bytes = 0; ///< Subset of texture_payload_bytes.
	size_t texture_key_payload_bytes = 0;
	size_t max_vehicle_union_width = 0;
	size_t max_vehicle_union_height = 0;
	size_t max_vehicle_union_pixels = 0;
	size_t cache_capacity_payload_bytes = 0;
	size_t frame_capacity_bytes = 0;
	size_t depth_capacity_bytes = 0;
	size_t scratch_capacity_bytes = 0;
	size_t effect_scratch_capacity_bytes = 0;
	bool operator==(const CabViewCacheStats &) const = default;
};

/** Premultiplied colour and coverage before mip generation or alpha widening. */
struct CabViewVehicleProbePixel {
	float r = 0, g = 0, b = 0, a = 0;
};

/** Focused inspection of the same vehicle-layer compositor used by the renderer. */
struct CabViewVehicleCompositeProbe {
	int left = 0, top = 0, width = 0, height = 0;
	size_t skipped_destination_layers = 0;
	size_t skipped_text_layers = 0;
	bool rejected_oversize = false;
	std::vector<CabViewVehicleProbePixel> pixels;
};

/** Offscreen composer probe; does not draw into the live cab or retain its cache. */
CabViewVehicleCompositeProbe ProbeCabViewVehicleComposite(const VehicleSpriteSeq &seq, PaletteID default_pal, bool crashed);
/** One synthetic source pixel through the same layer painter, before FinishTexture. */
CabViewVehicleProbePixel ProbeCabViewVehiclePixel(uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t m, PaletteID pal,
	const uint8_t *synthetic_remap = nullptr);
/** The same finite signed-union guard used before allocating a vehicle picture. */
bool CabViewVehicleUnionSupported(int64_t left, int64_t top, int64_t right, int64_t bottom);

/** Times one fresh build and aggregate same-key cache lookups in a private renderer. */
struct CabViewVehicleTextureProfile {
	uint64_t cold_build_ns = 0;
	uint64_t warm_lookup_total_ns = 0;
	uint32_t warm_lookups = 0;
	size_t cold_level0_pixels = 0;
	bool warm_texture_stable = false;
	CabViewCacheStats cache_after_cold;
	CabViewCacheStats cache;
	CabViewVehicleCompositeProbe composition;
};
CabViewVehicleTextureProfile ProfileCabViewVehicleTexture(const VehicleSpriteSeq &seq, PaletteID default_pal, bool crashed, bool widen, uint32_t warm_lookups);

/** Compare ordered composite keys in one private cache; does not alter live rendering. */
struct CabViewVehicleCacheIdentity {
	bool repeated_key_reused = false;
	bool variant_key_distinct = false;
	CabViewCacheStats after_first;
	CabViewCacheStats after_repeat;
	CabViewCacheStats after_variant;
};
CabViewVehicleCacheIdentity ProbeCabViewVehicleCacheIdentity(const VehicleSpriteSeq &first, const VehicleSpriteSeq &variant,
	PaletteID default_pal, bool crashed, bool first_widen, bool variant_widen);

/** Fixed-camera offscreen baseline from an isolated renderer (not the live cab). */
struct CabViewProfileSample {
	CabViewFrameStats frame;
	uint64_t wall_us = 0;
};

struct CabViewProfileBatch {
	CabViewProfileSample cold;
	std::vector<CabViewProfileSample> samples;
	CabViewProfileSample cached; ///< One identical-frame output after the fresh samples.
	CabViewCacheStats cache;
};

/** One measured fresh frame in the Stage 3C private, counterbalanced probe. */
struct CabViewCounterbalancedSample {
	CabViewFrameStats frame;
	uint64_t relative_start_us = 0;
	uint64_t wall_us = 0;
};

/** Process counters sampled at block boundaries, outside frame timing. */
struct CabViewProcessCounters {
	bool supported = false;
	uint64_t user_cpu_us = 0;
	uint64_t system_cpu_us = 0;
	uint64_t voluntary_context_switches = 0;
	uint64_t involuntary_context_switches = 0;
};

struct CabViewCounterbalancedBlock {
	CabViewEffectMode mode = CabViewEffectMode::Off;
	uint64_t relative_start_us = 0;
	CabViewProcessCounters before, after;
	std::vector<CabViewCounterbalancedSample> samples;
	std::vector<Colour> end_pixels; ///< Private block-end image, copied outside frame timing.
	CabViewCacheStats cache;
	bool exact_same_mode_image = false;
	bool exact_geometry = false;
	bool exact_same_mode_cache = false;
	bool frozen_state_unchanged = false;
};

struct CabViewCounterbalancedProbe {
	std::vector<CabViewCounterbalancedBlock> blocks;
	bool exact_cross_mode_geometry = false;
	bool exact_cross_mode_scene_cache = false;
	bool exact_cross_mode_image = false;
};

/** Caller scopes a private 640x372 screen/DPI and canonical static palette. */
CabViewCounterbalancedProbe ProbeCabViewCounterbalanced(const Rect &r, const CabViewCamera &camera, const std::function<bool()> &frozen_check);

/** Fixed Stage 3D pair schedule; A and B are physically Off controls. */
enum class CabViewPairLabel : uint8_t { OR, AB, RO, BA };

struct CabViewPairedArm {
	char label = 'O';
	CabViewEffectMode actual_mode = CabViewEffectMode::Off;
	CabViewCounterbalancedSample sample;
};

struct CabViewPairedPair {
	uint32_t cycle = 0;
	uint8_t position = 0;
	CabViewPairLabel label = CabViewPairLabel::OR;
	std::array<CabViewPairedArm, 2> arms;
	bool first_image_exact = false;
	bool second_image_exact = false;
	bool pair_images_equal = false;
	bool first_geometry_exact = false;
	bool second_geometry_exact = false;
};

struct CabViewPairedCycle {
	bool cache_unchanged = false;
	bool frozen_state_unchanged = false;
	CabViewCacheStats cache;
};

struct CabViewPairedProbe {
	std::array<CabViewPairedPair, 200> pairs;
	std::array<CabViewPairedCycle, 50> cycles;
	std::array<std::vector<Colour>, 4> representative_images;
	CabViewCacheStats warm_cache;
	bool unique_measured_frame_ids = false;
	uint32_t completed_pairs = 0;
	uint32_t completed_cycles = 0;
};

/** One renderer/framebuffer, fixed schedule and immutable camera; caller owns private scope. */
CabViewPairedProbe ProbeCabViewPaired(const Rect &r, const CabViewCamera &camera, const std::function<bool()> &frozen_check);

/** Eligible source contributions, not a claim that a palette epoch changed. */
struct CabViewAnimatedLayerProbe {
	SpriteID sprite = 0;
	PaletteID palette = 0;
	int left = 0, top = 0, width = 0, height = 0;
	uint32_t raw_animated_pixels = 0;
	uint32_t eligible_animated_pixels = 0;
	uint32_t zero_remap_pixels = 0;
	uint32_t brightness_zero_pixels = 0;
	std::array<uint32_t, 28> eligible_slot_counts{};
	bool skipped_destination_dependent = false;
	bool skipped_text_recolour = false;
	bool raw_missing = false;
	bool black = false;
};

struct CabViewAnimatedSequenceProbe {
	std::vector<CabViewAnimatedLayerProbe> layers;
	bool complete = true;
	bool rejected_oversize = false;
	uint32_t raw_animated_pixels = 0;
	uint32_t eligible_animated_pixels = 0;
};

/** Inspect only a caller-provided, world-validated vehicle sequence at Normal zoom. */
CabViewAnimatedSequenceProbe ProbeCabViewAnimatedSequence(const VehicleSpriteSeq &seq, PaletteID default_pal, bool crashed);

void DrawCabView3D(const Rect &r, const CabViewCamera &camera, uint32_t frame_id, float budget_ms);
CabViewFrameStats GetLastCabViewFrameStats();
/** This scan can be expensive; call outside the timed render interval. */
CabViewCacheStats GetCabViewCacheStats();
/** Caller supplies a private DPI/screen scope; budget zero pins this context at full scale. */
CabViewProfileBatch ProfileCabView3D(const Rect &r, const CabViewCamera &camera, uint32_t warmup_count, uint32_t sample_count);
float GetCabView3DFrameTime(float scale);
float GetCabView3DShipWaterZ(const Vehicle *v);
void ResetCabView3D();
bool CabViewTileVisible(TileIndex tile, WorldID world);
/** Decide whether a car can contribute any sprite in this camera world, before GetImage. */
bool CabViewVehicleVisible(const Vehicle *v, WorldID world);

#endif /* CAB_VIEW_3D_H */
