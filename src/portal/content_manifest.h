/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file content_manifest.h Strict content compatibility contract for federated transfers. */

#ifndef CONTENT_MANIFEST_H
#define CONTENT_MANIFEST_H

#include "../landscape_type.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

static constexpr uint16_t CONTENT_MANIFEST_VERSION = 1;
static constexpr size_t CONTENT_MANIFEST_MAX_NEWGRFS = 256;
static constexpr size_t CONTENT_MANIFEST_MAX_PARAMETERS = 128;
static constexpr size_t CONTENT_MANIFEST_MAX_BYTES = 1024 * 1024;

using ContentManifestToken = std::array<uint8_t, 32>;

enum class ContentManifestGRFFlag : uint8_t {
	System = 1U << 0,
	Static = 1U << 1,
	InitOnly = 1U << 2,
};

/** One simulation-relevant item in the effective NewGRF load order. */
struct ContentManifestGRF {
	std::array<uint8_t, 4> grfid{};
	std::array<uint8_t, 16> md5sum{};
	uint8_t palette = 0;
	uint8_t flags = 0;
	std::vector<uint32_t> parameters;

	auto operator<=>(const ContentManifestGRF &) const = default;
};

/** Version-independent representation of a strict federation content profile. */
struct UniverseContentManifest {
	uint16_t version = CONTENT_MANIFEST_VERSION;
	std::string network_revision;
	LandscapeType landscape = LandscapeType::Temperate;
	bool dynamic_engines = false;
	std::vector<ContentManifestGRF> newgrfs;

	auto operator<=>(const UniverseContentManifest &) const = default;
};

enum class ContentManifestError : uint8_t {
	None,
	Unavailable,
	InvalidField,
	TooManyNewGRFs,
	TooManyParameters,
	TooLarge,
	Truncated,
	InvalidMagic,
	UnsupportedVersion,
	LengthMismatch,
	ChecksumMismatch,
};

struct ContentManifestResult {
	ContentManifestError error = ContentManifestError::None;
	std::optional<UniverseContentManifest> manifest;

	bool Succeeded() const { return this->error == ContentManifestError::None && this->manifest.has_value(); }
};

struct ContentManifestBytes {
	ContentManifestError error = ContentManifestError::None;
	std::vector<uint8_t> bytes;

	bool Succeeded() const { return this->error == ContentManifestError::None; }
};

struct ContentManifestTokenResult {
	ContentManifestError error = ContentManifestError::None;
	ContentManifestToken token{};

	bool Succeeded() const { return this->error == ContentManifestError::None; }
};

enum class ContentCompatibility : uint8_t {
	Compatible,
	VersionMismatch,
	BuildRevisionMismatch,
	LandscapeMismatch,
	DynamicEnginesMismatch,
	NewGRFCountMismatch,
	NewGRFIdentityMismatch,
	NewGRFChecksumMismatch,
	NewGRFPaletteMismatch,
	NewGRFFlagsMismatch,
	NewGRFParametersMismatch,
};

struct ContentCompatibilityResult {
	ContentCompatibility reason = ContentCompatibility::Compatible;
	std::optional<size_t> newgrf_index;

	bool IsCompatible() const { return this->reason == ContentCompatibility::Compatible; }
};

class ContentManifestCodec {
public:
	/** Capture the effective content configuration of the current game. */
	static ContentManifestResult CaptureCurrent();

	/** Encode a canonical little-endian OSCM v1 descriptor. */
	static ContentManifestBytes Encode(const UniverseContentManifest &manifest);

	/** Decode and validate a canonical OSCM descriptor. */
	static ContentManifestResult Decode(std::span<const uint8_t> bytes);

	/** Hash the canonical descriptor with BLAKE2b-256. */
	static ContentManifestTokenResult Digest(const UniverseContentManifest &manifest);

	/** Return the first strict compatibility mismatch, including its NewGRF index when relevant. */
	static ContentCompatibilityResult Compare(const UniverseContentManifest &local, const UniverseContentManifest &remote);
};

#endif /* CONTENT_MANIFEST_H */
