/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file content_manifest.cpp Strict content compatibility contract for federated transfers. */

#include "../stdafx.h"
#include "content_manifest.h"

#include "../3rdparty/monocypher/monocypher.h"
#include "../network/core/network_game_info.h"
#include "../newgrf_config.h"
#include "../settings_type.h"

#include "../safeguards.h"

namespace {

static constexpr std::array<uint8_t, 4> MANIFEST_MAGIC{'O', 'S', 'C', 'M'};
static constexpr size_t MANIFEST_LENGTH_OFFSET = 8;
static constexpr uint8_t MANIFEST_KNOWN_GRF_FLAGS =
	static_cast<uint8_t>(ContentManifestGRFFlag::System) |
	static_cast<uint8_t>(ContentManifestGRFFlag::Static) |
	static_cast<uint8_t>(ContentManifestGRFFlag::InitOnly);

class ManifestWriter {
public:
	std::vector<uint8_t> data;

	void U8(uint8_t value) { this->data.push_back(value); }
	void U16(uint16_t value)
	{
		this->U8(static_cast<uint8_t>(value));
		this->U8(static_cast<uint8_t>(value >> 8));
	}
	void U32(uint32_t value)
	{
		for (uint shift = 0; shift < 32; shift += 8) this->U8(static_cast<uint8_t>(value >> shift));
	}
	template <size_t N>
	void Bytes(const std::array<uint8_t, N> &value)
	{
		this->data.insert(this->data.end(), value.begin(), value.end());
	}
	void String(std::string_view value)
	{
		this->U8(static_cast<uint8_t>(value.size()));
		this->data.insert(this->data.end(), value.begin(), value.end());
	}
};

class ManifestReader {
public:
	explicit ManifestReader(std::span<const uint8_t> bytes) : bytes(bytes) {}

	bool U8(uint8_t &value)
	{
		if (this->offset >= this->bytes.size()) return false;
		value = this->bytes[this->offset++];
		return true;
	}
	bool U16(uint16_t &value)
	{
		uint8_t a, b;
		if (!this->U8(a) || !this->U8(b)) return false;
		value = static_cast<uint16_t>(a) | (static_cast<uint16_t>(b) << 8);
		return true;
	}
	bool U32(uint32_t &value)
	{
		value = 0;
		for (uint shift = 0; shift < 32; shift += 8) {
			uint8_t byte;
			if (!this->U8(byte)) return false;
			value |= static_cast<uint32_t>(byte) << shift;
		}
		return true;
	}
	template <size_t N>
	bool Bytes(std::array<uint8_t, N> &value)
	{
		if (this->offset + N > this->bytes.size()) return false;
		std::copy_n(this->bytes.begin() + this->offset, N, value.begin());
		this->offset += N;
		return true;
	}
	bool String(std::string &value)
	{
		uint8_t size;
		if (!this->U8(size) || this->offset + size > this->bytes.size()) return false;
		value.assign(reinterpret_cast<const char *>(this->bytes.data() + this->offset), size);
		this->offset += size;
		return true;
	}
	size_t Remaining() const { return this->bytes.size() - this->offset; }

private:
	std::span<const uint8_t> bytes;
	size_t offset = 0;
};

static uint32_t ManifestChecksum(std::span<const uint8_t> bytes)
{
	uint32_t crc = 0xFFFFFFFFU;
	for (uint8_t byte : bytes) {
		crc ^= byte;
		for (uint bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
	}
	return ~crc;
}

static bool IsAll(const std::array<uint8_t, 4> &value, uint8_t byte)
{
	return std::ranges::all_of(value, [byte](uint8_t item) { return item == byte; });
}

static ContentManifestError ValidateManifest(const UniverseContentManifest &manifest)
{
	if (manifest.version != CONTENT_MANIFEST_VERSION || manifest.network_revision.empty() || manifest.network_revision.size() > UINT8_MAX) {
		return ContentManifestError::InvalidField;
	}
	if (to_underlying(manifest.landscape) >= NUM_LANDSCAPE) return ContentManifestError::InvalidField;
	if (manifest.newgrfs.size() > CONTENT_MANIFEST_MAX_NEWGRFS) return ContentManifestError::TooManyNewGRFs;
	for (const ContentManifestGRF &grf : manifest.newgrfs) {
		if (IsAll(grf.grfid, 0) || IsAll(grf.grfid, UINT8_MAX) || (grf.flags & ~MANIFEST_KNOWN_GRF_FLAGS) != 0) {
			return ContentManifestError::InvalidField;
		}
		if (grf.parameters.size() > CONTENT_MANIFEST_MAX_PARAMETERS) return ContentManifestError::TooManyParameters;
	}
	return ContentManifestError::None;
}

} // namespace

ContentManifestResult ContentManifestCodec::CaptureCurrent()
{
	UniverseContentManifest manifest;
	manifest.network_revision = GetNetworkRevisionString();
	manifest.landscape = _settings_game.game_creation.landscape;
	manifest.dynamic_engines = _settings_game.vehicle.dynamic_engines;

	for (const auto &config : _grfconfig) {
		if (config->status == GRFStatus::Disabled) continue;
		if (config->status == GRFStatus::NotFound || config->flags.Test(GRFConfigFlag::Invalid)) {
			return {ContentManifestError::Unavailable, std::nullopt};
		}
		if (config->status != GRFStatus::Activated &&
				!(config->flags.Test(GRFConfigFlag::InitOnly) && config->status == GRFStatus::Initialised)) {
			return {ContentManifestError::Unavailable, std::nullopt};
		}
		if (manifest.newgrfs.size() == CONTENT_MANIFEST_MAX_NEWGRFS) {
			return {ContentManifestError::TooManyNewGRFs, std::nullopt};
		}

		ContentManifestGRF grf;
		std::copy(config->ident.grfid.begin(), config->ident.grfid.end(), grf.grfid.begin());
		std::copy(config->ident.md5sum.begin(), config->ident.md5sum.end(), grf.md5sum.begin());
		grf.palette = config->palette;
		if (config->flags.Test(GRFConfigFlag::System)) grf.flags |= static_cast<uint8_t>(ContentManifestGRFFlag::System);
		if (config->flags.Test(GRFConfigFlag::Static)) grf.flags |= static_cast<uint8_t>(ContentManifestGRFFlag::Static);
		if (config->flags.Test(GRFConfigFlag::InitOnly)) grf.flags |= static_cast<uint8_t>(ContentManifestGRFFlag::InitOnly);
		grf.parameters = config->param;
		manifest.newgrfs.push_back(std::move(grf));
	}

	ContentManifestError error = ValidateManifest(manifest);
	return error == ContentManifestError::None ? ContentManifestResult{error, std::move(manifest)} : ContentManifestResult{error, std::nullopt};
}

ContentManifestBytes ContentManifestCodec::Encode(const UniverseContentManifest &manifest)
{
	ContentManifestError error = ValidateManifest(manifest);
	if (error != ContentManifestError::None) return {error, {}};

	ManifestWriter writer;
	writer.Bytes(MANIFEST_MAGIC);
	writer.U16(CONTENT_MANIFEST_VERSION);
	writer.U16(0);
	writer.U32(0);
	writer.String(manifest.network_revision);
	writer.U8(to_underlying(manifest.landscape));
	writer.U8(manifest.dynamic_engines ? 1 : 0);
	writer.U16(static_cast<uint16_t>(manifest.newgrfs.size()));
	for (const ContentManifestGRF &grf : manifest.newgrfs) {
		writer.Bytes(grf.grfid);
		writer.Bytes(grf.md5sum);
		writer.U8(grf.palette);
		writer.U8(grf.flags);
		writer.U8(static_cast<uint8_t>(grf.parameters.size()));
		writer.U8(0);
		for (uint32_t parameter : grf.parameters) writer.U32(parameter);
	}

	if (writer.data.size() + sizeof(uint32_t) > CONTENT_MANIFEST_MAX_BYTES) return {ContentManifestError::TooLarge, {}};
	uint32_t total_size = static_cast<uint32_t>(writer.data.size() + sizeof(uint32_t));
	for (uint shift = 0; shift < 32; shift += 8) writer.data[MANIFEST_LENGTH_OFFSET + shift / 8] = static_cast<uint8_t>(total_size >> shift);
	writer.U32(ManifestChecksum(writer.data));
	return {ContentManifestError::None, std::move(writer.data)};
}

ContentManifestResult ContentManifestCodec::Decode(std::span<const uint8_t> bytes)
{
	if (bytes.size() > CONTENT_MANIFEST_MAX_BYTES) return {ContentManifestError::TooLarge, std::nullopt};
	if (bytes.size() < 21) return {ContentManifestError::Truncated, std::nullopt};

	uint32_t stored_checksum = 0;
	for (uint shift = 0; shift < 32; shift += 8) stored_checksum |= static_cast<uint32_t>(bytes[bytes.size() - 4 + shift / 8]) << shift;
	if (ManifestChecksum(bytes.first(bytes.size() - 4)) != stored_checksum) return {ContentManifestError::ChecksumMismatch, std::nullopt};

	ManifestReader reader(bytes.first(bytes.size() - 4));
	std::array<uint8_t, 4> magic{};
	uint16_t version, reserved, grf_count;
	uint32_t total_size;
	UniverseContentManifest manifest;
	uint8_t landscape, dynamic_engines;
	if (!reader.Bytes(magic) || !reader.U16(version) || !reader.U16(reserved) || !reader.U32(total_size) ||
			!reader.String(manifest.network_revision) || !reader.U8(landscape) || !reader.U8(dynamic_engines) || !reader.U16(grf_count)) {
		return {ContentManifestError::Truncated, std::nullopt};
	}
	if (magic != MANIFEST_MAGIC) return {ContentManifestError::InvalidMagic, std::nullopt};
	if (version != CONTENT_MANIFEST_VERSION) return {ContentManifestError::UnsupportedVersion, std::nullopt};
	if (reserved != 0 || dynamic_engines > 1) return {ContentManifestError::InvalidField, std::nullopt};
	if (total_size != bytes.size()) return {ContentManifestError::LengthMismatch, std::nullopt};
	if (grf_count > CONTENT_MANIFEST_MAX_NEWGRFS) return {ContentManifestError::TooManyNewGRFs, std::nullopt};

	manifest.version = version;
	manifest.landscape = static_cast<LandscapeType>(landscape);
	manifest.dynamic_engines = dynamic_engines != 0;
	manifest.newgrfs.reserve(grf_count);
	for (uint i = 0; i < grf_count; ++i) {
		ContentManifestGRF grf;
		uint8_t parameter_count, entry_reserved;
		if (!reader.Bytes(grf.grfid) || !reader.Bytes(grf.md5sum) || !reader.U8(grf.palette) || !reader.U8(grf.flags) ||
				!reader.U8(parameter_count) || !reader.U8(entry_reserved)) {
			return {ContentManifestError::Truncated, std::nullopt};
		}
		if (entry_reserved != 0) return {ContentManifestError::InvalidField, std::nullopt};
		if (parameter_count > CONTENT_MANIFEST_MAX_PARAMETERS) return {ContentManifestError::TooManyParameters, std::nullopt};
		grf.parameters.reserve(parameter_count);
		for (uint parameter = 0; parameter < parameter_count; ++parameter) {
			uint32_t value;
			if (!reader.U32(value)) return {ContentManifestError::Truncated, std::nullopt};
			grf.parameters.push_back(value);
		}
		manifest.newgrfs.push_back(std::move(grf));
	}
	if (reader.Remaining() != 0) return {ContentManifestError::LengthMismatch, std::nullopt};

	ContentManifestError error = ValidateManifest(manifest);
	return error == ContentManifestError::None ? ContentManifestResult{error, std::move(manifest)} : ContentManifestResult{error, std::nullopt};
}

ContentManifestTokenResult ContentManifestCodec::Digest(const UniverseContentManifest &manifest)
{
	ContentManifestBytes encoded = Encode(manifest);
	if (!encoded.Succeeded()) return {encoded.error, {}};

	ContentManifestToken token{};
	crypto_blake2b(token.data(), token.size(), encoded.bytes.data(), encoded.bytes.size() - sizeof(uint32_t));
	return {ContentManifestError::None, token};
}

ContentCompatibilityResult ContentManifestCodec::Compare(const UniverseContentManifest &local, const UniverseContentManifest &remote)
{
	if (local.version != remote.version) return {ContentCompatibility::VersionMismatch, std::nullopt};
	if (local.network_revision != remote.network_revision) return {ContentCompatibility::BuildRevisionMismatch, std::nullopt};
	if (local.landscape != remote.landscape) return {ContentCompatibility::LandscapeMismatch, std::nullopt};
	if (local.dynamic_engines != remote.dynamic_engines) return {ContentCompatibility::DynamicEnginesMismatch, std::nullopt};
	if (local.newgrfs.size() != remote.newgrfs.size()) return {ContentCompatibility::NewGRFCountMismatch, std::nullopt};
	for (size_t i = 0; i < local.newgrfs.size(); ++i) {
		const ContentManifestGRF &a = local.newgrfs[i];
		const ContentManifestGRF &b = remote.newgrfs[i];
		if (a.grfid != b.grfid) return {ContentCompatibility::NewGRFIdentityMismatch, i};
		if (a.md5sum != b.md5sum) return {ContentCompatibility::NewGRFChecksumMismatch, i};
		if (a.palette != b.palette) return {ContentCompatibility::NewGRFPaletteMismatch, i};
		if (a.flags != b.flags) return {ContentCompatibility::NewGRFFlagsMismatch, i};
		if (a.parameters != b.parameters) return {ContentCompatibility::NewGRFParametersMismatch, i};
	}
	return {};
}
