#!/usr/bin/env python3
"""
Test Commonwealth Universe Graph topology, connectivity, filter modes,
and economic profiles for Sprint 49.
"""

import json
import os
import sys
from pathlib import Path
from collections import deque

def load_graph(filepath: Path):
    with open(filepath, "r", encoding="utf-8") as f:
        return json.load(f)

def build_adjacency(data, filter_mode="PLAYABLE_COMPLETION"):
    """
    filter_mode:
      - SOURCE_ONLY: evidence in (E, R)
      - RECONSTRUCTED: evidence in (E, R, I)
      - PLAYABLE_COMPLETION: evidence in (E, R, I, A)
    """
    allowed_evidence = {"E", "R"}
    if filter_mode in ("RECONSTRUCTED", "PLAYABLE_COMPLETION"):
        allowed_evidence.add("I")
    if filter_mode == "PLAYABLE_COMPLETION":
        allowed_evidence.add("A")

    adj = {}
    for n in data["nodes"]:
        adj[n["world_id"]] = set()
    for s in data["special_bodies"]:
        adj[s["world_id"]] = set()

    for conn in data["connections"]:
        ev = conn["evidence_class"]
        if ev in allowed_evidence:
            u = conn["endpoint_a"]
            v = conn["endpoint_b"]
            if u in adj and v in adj:
                adj[u].add(v)
                adj[v].add(u)

    return adj

def bfs_reachable(adj, root_id):
    if root_id not in adj:
        return set()
    visited = set([root_id])
    queue = deque([root_id])
    while queue:
        curr = queue.popleft()
        for neighbor in adj[curr]:
            if neighbor not in visited:
                visited.add(neighbor)
                queue.append(neighbor)
    return visited

def test_counts(data):
    nodes = data["nodes"]
    special_bodies = data["special_bodies"]
    assert len(nodes) == 108, f"Expected 108 nodes, got {len(nodes)}"
    assert len(special_bodies) == 16, f"Expected 16 special bodies, got {len(special_bodies)}"
    assert data["metadata"]["node_count"] == 108
    assert data["metadata"]["special_body_count"] == 16
    print(f"PASS: Node counts verified (108 nodes, 16 special bodies)")

def test_sol_root_and_big15(data):
    node_map = {n["world_id"]: n for n in data["nodes"]}
    assert "world_earth" in node_map, "world_earth missing"
    assert node_map["world_earth"]["location_type"] == "homeworld"

    # Big 15 check
    big15_names = [
        "Augusta", "Bayovar", "Buta", "Democratic Republic of New Germany",
        "EdenBurg", "Granada", "Kerensk", "Los Vada", "Mito", "Orleans",
        "Piura", "Shayoni", "StLincoln", "Verona", "Wessex"
    ]
    for b in big15_names:
        found = any(n["canonical_name"] == b for n in data["nodes"])
        assert found, f"Big15 world {b} not found"
        # Check direct link to earth
        earth_id = "world_earth"
        b_node = next(n for n in data["nodes"] if n["canonical_name"] == b)
        assert b_node["connects_to"] == earth_id, f"{b} does not connect to Earth"
        assert b_node["economic_profile"]["megacity_eligible"] is True
    print(f"PASS: Sol/Earth root and all Big15 hubs verified with R0 Earth links")

def test_filter_modes(data):
    # 1. PLAYABLE_COMPLETION: maximum connectivity from Earth
    adj_playable = build_adjacency(data, "PLAYABLE_COMPLETION")
    reach_playable = bfs_reachable(adj_playable, "world_earth")
    print(f"PLAYABLE_COMPLETION reachable from Earth: {len(reach_playable)} nodes")
    assert len(reach_playable) >= 100, f"Expected >= 100 reachable nodes, got {len(reach_playable)}"

    # 2. RECONSTRUCTED: includes E, R, I
    adj_reconstructed = build_adjacency(data, "RECONSTRUCTED")
    reach_reconstructed = bfs_reachable(adj_reconstructed, "world_earth")
    print(f"RECONSTRUCTED reachable from Earth: {len(reach_reconstructed)} nodes")
    assert len(reach_reconstructed) < len(reach_playable)
    assert len(reach_reconstructed) >= 30, f"Expected >= 30 reconstructed nodes, got {len(reach_reconstructed)}"

    # 3. SOURCE_ONLY: includes E, R only
    adj_source = build_adjacency(data, "SOURCE_ONLY")
    reach_source = bfs_reachable(adj_source, "world_earth")
    print(f"SOURCE_ONLY reachable from Earth: {len(reach_source)} nodes")
    assert len(reach_source) < len(reach_reconstructed)
    assert len(reach_source) >= 20, f"Expected >= 20 source-only nodes, got {len(reach_source)}"

    print(f"PASS: Graph filter modes validated (SOURCE_ONLY < RECONSTRUCTED < PLAYABLE_COMPLETION)")

def test_modal_rules(data):
    node_map = {n["world_id"]: n for n in data["nodes"]}
    vinmar = node_map.get("world_vinmar")
    assert vinmar is not None
    assert vinmar["connection_mode"] == "RAIL"

    vinmar_conn = next(c for c in data["connections"] if "vinmar" in c["connection_id"])
    assert vinmar_conn["permits_through_running_train"] is True
    assert vinmar_conn["connection_type"] == "RAIL"

    # Far Away universal rail freight route (WP-50.3)
    far_away = node_map.get("world_far_away")
    assert far_away is not None
    assert far_away["connection_mode"] == "RAIL"

    far_away_conn = next(c for c in data["connections"] if "far_away" in c["connection_id"])
    assert far_away_conn["connection_type"] == "RAIL"
    assert far_away_conn["availability"] == "PERMANENT"
    assert far_away_conn["permits_through_running_train"] is True

    # Private world verification (WP-50.4)
    cressat = node_map.get("world_cressat")
    assert cressat is not None
    assert cressat["connection_mode"] == "PRIVATE"

    print(f"PASS: Modal rules verified (Vinmar & Far Away are universal RAIL, Cressat is PRIVATE)")

def main():
    repo_root = Path(__file__).resolve().parent.parent
    json_path = repo_root / "assets" / "data" / "commonwealth_universe.json"
    if not json_path.exists():
        json_path = repo_root / "bin" / "data" / "commonwealth_universe.json"

    data = load_graph(json_path)
    test_counts(data)
    test_sol_root_and_big15(data)
    test_filter_modes(data)
    test_modal_rules(data)
    print("ALL PYTHON UNIVERSE GRAPH TESTS PASSED!")

if __name__ == "__main__":
    main()
