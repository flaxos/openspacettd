#!/usr/bin/env python3
"""
Generate commonwealth_universe.json from docs/world_roadmap.txt.
Produces the canonical 108-world reference catalogue, 16 special bodies,
and graph connection edges for Sprint 49 Commonwealth Graph Engine.
"""

import json
import os
import re
import sys
from pathlib import Path

BIG15 = {
    "Augusta", "Bayovar", "Buta", "Democratic Republic of New Germany",
    "EdenBurg", "Granada", "Kerensk", "Los Vada", "Mito", "Orleans",
    "Piura", "Shayoni", "StLincoln", "Verona", "Wessex"
}

OTHER_P1 = {
    "Arevalo", "Kyushu", "Lothian", "New Iberia", "Oaktier", "Velaines", "York5"
}

P2_WORLDS = {
    "Anacona", "Anshun", "Bilma", "Boongate", "Gralmond", "Hanko",
    "Merredin", "Saville", "Tanyata"
}

P3_WORLDS = {
    "Anagaska", "Clonclurry", "Elan", "Far Away", "Valvida"
}

def normalize_id(name: str) -> str:
    cleaned = re.sub(r"[^a-zA-Z0-9_]+", "_", name.strip().lower()).strip("_")
    if not cleaned.startswith("world_") and not cleaned.startswith("node_") and not cleaned.startswith("sys_"):
        return f"world_{cleaned}"
    return cleaned

def map_phase(phase_str: str, world_name: str) -> str:
    phase_str = phase_str.strip()
    if world_name in BIG15 or world_name in OTHER_P1:
        return "Phase1_Core"
    if phase_str == "P1":
        return "Phase1_Core"
    if phase_str == "P2" or world_name in P2_WORLDS:
        return "Phase2_Developed"
    if phase_str == "P3" or world_name in P3_WORLDS:
        return "Phase3_Frontier"
    if phase_str == "P4":
        return "Phase4_Expansion"
    if phase_str == "NA":
        return "NotApplicable"
    return "Phase3_Frontier" if phase_str == "?" else "Unassigned"

def map_biome(world_name: str, phase: str) -> str:
    # Lore-specific biomes
    arid = {"Merredin", "Anshun", "Damaran", "Dampier", "Calyx", "Berkak", "Kozani", "Austin", "Abadan"}
    boreal = {"Far Away", "Hanko", "Nattavaara", "Vyborg", "Ice Citadel World", "Gaczyna", "Tandil", "Halifax"}
    volcanic = {"Hardrock", "Chelva", "Sligo", "Whalton", "Martaban"}
    lush = {"Earth", "Augusta", "EdenBurg", "Silvergalde", "Verona", "Los Vada", "Shayoni", "DRNG", "Orleans"}
    if world_name in arid:
        return "AridDesert"
    if world_name in boreal:
        return "BorealSnow"
    if world_name in volcanic:
        return "VolcanicBasalt"
    if world_name in lush:
        return "Temperate"
    if phase == "Phase1_Core":
        return "Temperate"
    if phase == "Phase2_Developed":
        return "AridDesert"
    return "Temperate"

def make_economic_profile(world_name: str, phase: str, role_hint: str = ""):
    if world_name == "Earth":
        return {
            "role": "sol_homeworld_capital",
            "primary_imports": ["SUPERALLOYS", "HIGH_TECH_ELECTRONICS", "QUANTUM_CRYSTALS", "CONSUMER_CRYSTALS", "LUXURY_GOODS"],
            "primary_exports": ["FABRICATION_PARTS", "MACHINE_MODULES", "PASSENGERS", "INVESTMENT_CAPITAL"],
            "base_population": 8500000000,
            "megacity_eligible": True,
            "tariff_multiplier": 1.5
        }
    if world_name in BIG15:
        return {
            "role": "core_metropolitan_hub",
            "primary_imports": ["STRUCTURAL_STEEL", "HIGH_TECH_ELECTRONICS", "CONSUMER_CRYSTALS", "FOOD"],
            "primary_exports": ["QUANTUM_CRYSTALS", "MACHINE_MODULES", "LUXURY_GOODS"],
            "base_population": 85000000,
            "megacity_eligible": True,
            "tariff_multiplier": 1.25
        }
    if phase == "Phase1_Core":
        return {
            "role": "metropolitan_center",
            "primary_imports": ["SUPERALLOYS", "WIRING", "CONSUMER_CRYSTALS", "FOOD"],
            "primary_exports": ["HIGH_TECH_ELECTRONICS", "DATA_CRYSTALS"],
            "base_population": 25000000,
            "megacity_eligible": True,
            "tariff_multiplier": 1.15
        }
    if phase == "Phase2_Developed":
        return {
            "role": "heavy_refining_and_fabrication",
            "primary_imports": ["IRON_ORE", "COPPER_ORE", "SILICA_SAND", "HYDROCARBONS", "FOOD"],
            "primary_exports": ["STRUCTURAL_STEEL", "SUPERALLOYS", "SIGNALLING_LOGIC", "MACHINE_MODULES"],
            "base_population": 4500000,
            "megacity_eligible": False,
            "tariff_multiplier": 1.0
        }
    if phase == "Phase3_Frontier":
        return {
            "role": "primary_resource_extraction",
            "primary_imports": ["STRUCTURAL_STEEL", "MACHINE_MODULES", "FOOD", "CONSUMER_GOODS"],
            "primary_exports": ["IRON_ORE", "COPPER_ORE", "RARE_EARTHS", "BLANK_CRYSTALS", "BIOMASS"],
            "base_population": 250000,
            "megacity_eligible": False,
            "tariff_multiplier": 0.85
        }
    return {
        "role": "wilderness_and_outpost",
        "primary_imports": ["BUILDING_MATERIALS", "LIFE_SUPPORT", "FABRICATION_PARTS"],
        "primary_exports": ["UNREFINED_ORE", "EXOTIC_SPECIMENS"],
        "base_population": 1500,
        "megacity_eligible": False,
        "tariff_multiplier": 0.70
    }

def main():
    repo_root = Path(__file__).resolve().parent.parent
    roadmap_path = repo_root / "docs" / "world_roadmap.txt"
    if not roadmap_path.exists():
        print(f"Error: {roadmap_path} not found!", file=sys.stderr)
        sys.exit(1)

    with open(roadmap_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    # Parse 108 Main Catalogue (Section 4)
    # WORLD | PHASE | CONNECTS_TO | EVIDENCE | MODE | SOURCE/RULE
    in_section_4 = False
    in_section_5 = False
    in_section_6 = False

    raw_108 = []
    raw_section_5 = []
    raw_section_6 = []

    for line in lines:
        s = line.strip()
        if "4. COMPLETE MAIN REFERENCE CATALOGUE" in s:
            in_section_4 = True
            in_section_5 = False
            in_section_6 = False
            continue
        elif "5. ADDITIONAL WORLDS / PLANETARY BODIES" in s:
            in_section_4 = False
            in_section_5 = True
            in_section_6 = False
            continue
        elif "6. NON-PLANET AND DESCRIPTIVELY NAMED NODES" in s:
            in_section_4 = False
            in_section_5 = False
            in_section_6 = True
            continue
        elif "7. TERMINAL-LEVEL CONNECTIONS" in s:
            in_section_4 = False
            in_section_5 = False
            in_section_6 = False
            break

        if in_section_4:
            if "|" in s and not s.startswith("WORLD"):
                parts = [p.strip() for p in s.split("|")]
                if len(parts) >= 6:
                    raw_108.append(parts)
        elif in_section_5:
            if "|" in s and not s.startswith("WORLD"):
                parts = [p.strip() for p in s.split("|")]
                if len(parts) >= 6:
                    raw_section_5.append(parts)

    print(f"Parsed {len(raw_108)} entries from Section 4 (expected 108)")
    print(f"Parsed {len(raw_section_5)} entries from Section 5 (expected 8)")

    if len(raw_108) != 108:
        print(f"Error: expected 108 entries in section 4, got {len(raw_108)}", file=sys.stderr)
        sys.exit(1)

    nodes = []
    connections = []

    for parts in raw_108:
        world_name, raw_phase, connects_to, evidence, mode, source_rule = parts[:6]
        world_id = normalize_id(world_name)
        parent_id = normalize_id(connects_to) if connects_to not in ("ROOT", "UNASSIGNED") else ""
        phase = map_phase(raw_phase, world_name)
        biome = map_biome(world_name, phase)

        # Economic profile
        econ = make_economic_profile(world_name, phase)

        node = {
            "world_id": world_id,
            "canonical_name": world_name,
            "display_name": world_name,
            "phase": phase,
            "phase_evidence": raw_phase if raw_phase in ("P1", "P2", "P3", "P4") else evidence,
            "biome": biome,
            "location_type": "homeworld" if world_name == "Earth" else ("core_world" if phase == "Phase1_Core" else "colony_world"),
            "system": "Sol" if world_name == "Earth" else world_name,
            "era": "pre_invasion",
            "connects_to": parent_id,
            "connection_evidence": evidence,
            "connection_mode": mode,
            "source_rule": source_rule,
            "source_ids": re.findall(r"S\d+", source_rule) or ["S01"],
            "economic_profile": econ
        }
        nodes.append(node)

        # Create connection edge if connected
        if parent_id and connects_to not in ("ROOT", "UNASSIGNED"):
            permits_train = (mode == "RAIL")
            is_enabled = (mode in ("RAIL", "GATE", "SCHEDULED", "PRIVATE"))
            conn = {
                "connection_id": f"conn_{world_id}_{parent_id}",
                "endpoint_a": world_id,
                "endpoint_b": parent_id,
                "connection_type": mode,
                "evidence_class": evidence,
                "source_ids": re.findall(r"S\d+", source_rule) or ["S01"],
                "inference_or_assumption_id": re.findall(r"[RIA]\d+", source_rule)[0] if re.findall(r"[RIA]\d+", source_rule) else "NONE",
                "availability": "SCHEDULED" if mode == "SCHEDULED" else ("PRIVATE" if mode == "PRIVATE" else "PERMANENT"),
                "access_policy": "PRIVATE" if mode == "PRIVATE" else "PUBLIC",
                "permits_through_running_train": permits_train,
                "virtual_length_tiles": 32,
                "enabled_in_game": is_enabled
            }
            connections.append(conn)

    # 16 Special bodies:
    # 8 from Section 5: Chelva, Gaczyna, Hardrock, Icalanise, Jaruva, Mars, Merioneth, Tandil
    # 8 from Section 6: OZZIE_ASTEROID, HIGH_ANGEL, HIGH_ANGEL_ORBITAL_GATEWAYS, ICE_CITADEL_WORLD, PRIME_HOMEWORLD, DYSON_ALPHA, DYSON_BETA, ALPHA_LEONIS
    special_bodies = []

    for parts in raw_section_5:
        world_name, raw_phase, connects_to, evidence, mode, source_rule = parts[:6]
        world_id = normalize_id(world_name)
        parent_id = normalize_id(connects_to) if connects_to not in ("ROOT", "UNASSIGNED", "NO SURFACE CONNECTION", "Silfen path network") else ""
        phase = map_phase(raw_phase, world_name)
        biome = map_biome(world_name, phase)
        econ = make_economic_profile(world_name, phase)

        body = {
            "world_id": world_id,
            "canonical_name": world_name,
            "display_name": world_name,
            "phase": phase,
            "phase_evidence": evidence,
            "biome": biome,
            "location_type": "special_body",
            "system": "Sol" if world_name == "Mars" else world_name,
            "era": "pre_invasion",
            "connects_to": parent_id,
            "connection_evidence": evidence,
            "connection_mode": mode,
            "source_rule": source_rule,
            "source_ids": re.findall(r"S\d+", source_rule) or ["S01"],
            "economic_profile": econ
        }
        special_bodies.append(body)

        if parent_id:
            permits_train = (mode == "RAIL")
            is_enabled = (mode in ("RAIL", "GATE", "EXPLORATION", "PRIVATE"))
            conn = {
                "connection_id": f"conn_{world_id}_{parent_id}",
                "endpoint_a": world_id,
                "endpoint_b": parent_id,
                "connection_type": mode,
                "evidence_class": evidence,
                "source_ids": re.findall(r"S\d+", source_rule) or ["S01"],
                "inference_or_assumption_id": re.findall(r"[RIA]\d+", source_rule)[0] if re.findall(r"[RIA]\d+", source_rule) else "NONE",
                "availability": "PERMANENT",
                "access_policy": "PRIVATE" if mode == "PRIVATE" else ("RESTRICTED" if mode == "EXPLORATION" else "PUBLIC"),
                "permits_through_running_train": permits_train,
                "virtual_length_tiles": 48,
                "enabled_in_game": is_enabled
            }
            connections.append(conn)

    # 8 from Section 6
    sec_6_definitions = [
        {
            "id": "node_ozzie_asteroid",
            "name": "Ozzie's Private Asteroid",
            "display_name": "Ozzie's Asteroid",
            "type": "private_asteroid_habitat",
            "system": "Alpha Leonis",
            "connects_to": "world_augusta",
            "evidence": "E",
            "mode": "PRIVATE",
            "source": "S10, S18",
            "permits_train": False
        },
        {
            "id": "node_high_angel",
            "name": "High Angel",
            "display_name": "High Angel (Starship Habitat)",
            "type": "alien_starship_habitat",
            "system": "Icalanise",
            "connects_to": "node_high_angel_orbital_gateways",
            "evidence": "E",
            "mode": "SHUTTLE",
            "source": "S01, S09",
            "permits_train": False
        },
        {
            "id": "node_high_angel_orbital_gateways",
            "name": "High Angel Orbital Gateways",
            "display_name": "High Angel Orbital Gateways",
            "type": "orbital_gateway_port",
            "system": "Icalanise",
            "connects_to": "world_kerensk",
            "evidence": "E",
            "mode": "ORBITAL",
            "source": "S09",
            "permits_train": False
        },
        {
            "id": "node_ice_citadel_world",
            "name": "Ice Citadel World",
            "display_name": "Unnamed Ice World (Ice Citadel)",
            "type": "silfen_lodge_world",
            "system": "Unknown",
            "connects_to": "world_silvergalde",
            "evidence": "R",
            "mode": "SILFEN_ROUTE",
            "source": "S01, S08",
            "permits_train": False
        },
        {
            "id": "node_prime_homeworld",
            "name": "Prime Homeworld",
            "display_name": "Prime Homeworld (MorningLightMountain)",
            "type": "hostile_alien_world",
            "system": "Dyson Alpha",
            "connects_to": "",
            "evidence": "R",
            "mode": "NONE",
            "source": "S14",
            "permits_train": False
        },
        {
            "id": "node_dyson_alpha",
            "name": "Dyson Alpha",
            "display_name": "Dyson Alpha System",
            "type": "enclosed_star_system",
            "system": "Dyson Alpha",
            "connects_to": "",
            "evidence": "R",
            "mode": "NONE",
            "source": "S15",
            "permits_train": False
        },
        {
            "id": "node_dyson_beta",
            "name": "Dyson Beta",
            "display_name": "Dyson Beta System",
            "type": "enclosed_star_system",
            "system": "Dyson Beta",
            "connects_to": "",
            "evidence": "R",
            "mode": "NONE",
            "source": "S15",
            "permits_train": False
        },
        {
            "id": "sys_alpha_leonis",
            "name": "Alpha Leonis",
            "display_name": "Alpha Leonis System",
            "type": "binary_star_system",
            "system": "Alpha Leonis",
            "connects_to": "world_augusta",
            "evidence": "R",
            "mode": "NONE",
            "source": "S01",
            "permits_train": False
        }
    ]

    for item in sec_6_definitions:
        body = {
            "world_id": item["id"],
            "canonical_name": item["name"],
            "display_name": item["display_name"],
            "phase": "NotApplicable",
            "phase_evidence": item["evidence"],
            "biome": "Temperate",
            "location_type": item["type"],
            "system": item["system"],
            "era": "pre_invasion",
            "connects_to": item["connects_to"],
            "connection_evidence": item["evidence"],
            "connection_mode": item["mode"],
            "source_rule": item["source"],
            "source_ids": re.findall(r"S\d+", item["source"]) or ["S01"],
            "economic_profile": {
                "role": item["type"],
                "primary_imports": ["EXOTIC_TECHNOLOGY", "QUANTUM_CRYSTALS"],
                "primary_exports": ["EXOTIC_BLUEPRINTS", "ALIEN_ARTIFACTS"],
                "base_population": 100000 if "habitat" in item["type"] else 0,
                "megacity_eligible": False,
                "tariff_multiplier": 2.0
            }
        }
        special_bodies.append(body)

        if item["connects_to"]:
            conn = {
                "connection_id": f"conn_{item['id']}_{item['connects_to']}",
                "endpoint_a": item["id"],
                "endpoint_b": item["connects_to"],
                "connection_type": item["mode"],
                "evidence_class": item["evidence"],
                "source_ids": re.findall(r"S\d+", item["source"]) or ["S01"],
                "inference_or_assumption_id": "NONE",
                "availability": "PRIVATE" if item["mode"] == "PRIVATE" else "PERMANENT",
                "access_policy": "PRIVATE" if item["mode"] == "PRIVATE" else "RESTRICTED",
                "permits_through_running_train": item["permits_train"],
                "virtual_length_tiles": 64,
                "enabled_in_game": (item["mode"] in ("PRIVATE", "ORBITAL", "SHUTTLE", "SILFEN_ROUTE"))
            }
            connections.append(conn)

    print(f"Total special bodies created: {len(special_bodies)} (expected 16)")
    print(f"Total connections created: {len(connections)}")

    universe_data = {
        "metadata": {
            "schema_version": "1.0",
            "title": "Commonwealth Saga 108-World Universe Topology",
            "source": "docs/world_roadmap.txt",
            "governing_lore": "Peter F. Hamilton Commonwealth Saga (Pandora's Star / Judas Unchained)",
            "node_count": len(nodes),
            "special_body_count": len(special_bodies),
            "total_location_count": len(nodes) + len(special_bodies),
            "connection_count": len(connections),
            "root_node_id": "world_earth"
        },
        "nodes": nodes,
        "special_bodies": special_bodies,
        "connections": connections
    }

    out_paths = [
        repo_root / "assets" / "data" / "commonwealth_universe.json",
        repo_root / "bin" / "data" / "commonwealth_universe.json"
    ]

    for p in out_paths:
        p.parent.mkdir(parents=True, exist_ok=True)
        with open(p, "w", encoding="utf-8") as f:
            json.dump(universe_data, f, indent=2)
        print(f"Wrote {p} ({p.stat().st_size} bytes)")

if __name__ == "__main__":
    main()
