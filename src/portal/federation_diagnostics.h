/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file federation_diagnostics.h Read-only native freight acceptance diagnostics. */
#ifndef FEDERATION_DIAGNOSTICS_H
#define FEDERATION_DIAGNOSTICS_H

/** Report native production, delivery, finances and portable train state. */
bool ConFederationFreightStatus(std::span<std::string_view> argv);

#endif /* FEDERATION_DIAGNOSTICS_H */
