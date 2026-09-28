#!/usr/bin/env python3
"""Persistent, host-authenticated advertisements for one registered CST universe."""

from __future__ import annotations

import copy
import secrets
import re
import time


class StellarDirectory:
    """Advertisements contain public metadata, never login credentials or cargo stock."""

    def __init__(self, host_token: str = ""):
        self.host_token = host_token
        self.hosts: dict[str, dict] = {}

    def publish(self, payload: dict) -> tuple[bool, object]:
        if not self.host_token or not secrets.compare_digest(str(payload.get("token", "")), self.host_token):
            return False, "Universe host credential required"
        try:
            host = copy.deepcopy(payload["host"])
            identity = host["namespace"]
            if not isinstance(identity, str) or not re.fullmatch(r"[0-9a-f]{1,16}:[0-9a-f]{1,16}", identity) or identity == "0:0":
                raise ValueError("Invalid host identity")
            if not isinstance(host["address"], str) or len(host["address"]) > 255 or any(c in host["address"] for c in "\r\n/@"):
                raise ValueError("Invalid game address")
            if not isinstance(host["manifest"], str) or not re.fullmatch(r"[0-9a-f]{64}", host["manifest"]):
                raise ValueError("Invalid content manifest")
            if len(host["worlds"]) > 16 or len(host["stations"]) > 4096 or len(host["gates"]) > 4096 or len(host["companies"]) > 15:
                raise ValueError("Advertisement exceeds universe limits")
            previous = self.hosts.get(identity)
            if previous and previous["address"] != host["address"] and time.time() - previous.get("updated", 0) < 30:
                raise ValueError("Host identity is already online at another address")
            world_ids = {int(w["id"]) for w in host["worlds"]}
            if any(w < 0 or w >= 2**32-1 for w in world_ids):
                raise ValueError("Invalid world identity")
            if len(world_ids) != len(host["worlds"]):
                raise ValueError("Duplicate world identity")
            for other_id, other in self.hosts.items():
                if other_id != identity and world_ids.intersection(int(w["id"]) for w in other["worlds"]):
                    raise ValueError("World IDs must be unique within the registered universe")
            for row in host["stations"]:
                if int(row["world"]) not in world_ids or row["namespace"] != identity or int(row["sequence"]) <= 0:
                    raise ValueError("Invalid advertised station identity")
                if not isinstance(row["name"], str) or len(row["name"]) > 256:
                    raise ValueError("Invalid station name")
            for row in host["gates"]:
                if int(row["world"]) not in world_ids or int(row.get("toll", 0)) < 0:
                    raise ValueError("Invalid gate")
            for kind in ("zones", "gateprojects", "gatereplys", "trains"):
                rows = host.setdefault(kind, [])
                if not isinstance(rows, list) or len(rows) > 4096:
                    raise ValueError("Invalid project directory")
                for row in rows:
                    if len(str(row)) > 1050:
                        raise ValueError("Project record exceeds command limit")
                    if kind == "zones":
                        if int(row["world"]) not in world_ids or not 0 < int(row["id"]) < 2**32 or abs(int(row["x"])) > 100000 or abs(int(row["y"])) > 100000:
                            raise ValueError("Invalid landing zone")
                    elif kind == "gateprojects":
                        if row["namespace"] != identity or int(row["source_world"]) not in world_ids or not row["id"].startswith(identity+":") or row["state"] not in ("supplying", "ready", "committed", "active", "cancelled"):
                            raise ValueError("Invalid source commitment")
                        if not 1 <= int(row["bands"]) <= 10 or not 0 <= int(row["steel"]) <= 200*int(row["bands"]) or not 0 <= int(row["machines"]) <= 40*int(row["bands"]):
                            raise ValueError("Invalid equipment inventory")
                    elif kind == "trains":
                        if int(row["world"]) not in world_ids or not 0 < int(row["sequence"]) < 2**64 or not isinstance(row["name"], str):
                            raise ValueError("Invalid train location")
                    elif row["state"] not in ("reserved", "built", "active", "cancelled") or not 0 <= int(row["price"]) < 2**63:
                        raise ValueError("Invalid landing reservation")
            host["updated"] = time.time()
            self.hosts[identity] = host
            return True, {"accepted": identity}
        except (KeyError, ValueError, TypeError, OverflowError) as exc:
            return False, str(exc)

    def snapshot(self) -> dict:
        now = time.time()
        hosts = copy.deepcopy(list(self.hosts.values()))
        for host in hosts:
            host["online"] = now - host.get("updated", 0) < 30
        return {"version": 1, "hosts": sorted(hosts, key=lambda h: h["namespace"])}

    def export_state(self) -> dict:
        return copy.deepcopy(self.hosts)

    def import_state(self, data: dict):
        self.hosts = copy.deepcopy(data)
        # A restored registration does not establish that a game process is alive.
        for host in self.hosts.values():
            host["updated"] = 0
