#!/usr/bin/env python3
"""
OpenSpaceTTD Universe Authority Daemon (Phase F3)
Centralized transaction coordinator, player authentication, corporate registry,
dynamic world discovery directory, and commodity conservation ledger.
"""

import argparse
import base64
import hashlib
import json
import secrets
import sys
import time
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

class UniverseAuthority:
    def __init__(self):
        self.reset()

    def reset(self):
        self.next_transfer_seq = 1
        self.next_player_seq = 1
        self.next_charter_seq = 1
        self.worlds = {}
        self.routes = {}
        self.transfers = {}
        self.players = {}        # player_id -> dict
        self.auth_tokens = {}     # token -> player_id
        self.charters = {}       # charter_id -> dict
        # Commodity ledger tracking: cargo_type -> amount
        self.commodity_initiated = {}
        self.commodity_in_transit = {}
        self.commodity_completed = {}
        # World trade balances: (source, dest) -> {cargo_type: count, credits: int}
        self.world_trade = {}    # f"{w1}->{w2}" -> {cargo_counts: {}, total_credits: 0}
        # Megacities (town_id -> dict)
        self.megacities = {}
        # Empire supply chain matrix
        self.supply_chain_matrix = {
            "frontier_to_refinery_cargo": 0,
            "refinery_to_core_cargo": 0,
            "frontier_to_core_cargo": 0,
            "core_export_cargo": 0,
            "total_interplanetary_cargo": 0,
            "total_tariffs_generated": 0
        }

    # -------------------------------------------------------------------------
    # Authentication & Accounts
    # -------------------------------------------------------------------------
    def register_player(self, data):
        username = data.get("username", "").strip()
        display_name = data.get("display_name", username).strip()
        pwd_hash = data.get("password_hash", "")
        if not username:
            return False, "username is required"

        for p in self.players.values():
            if p["username"].lower() == username.lower():
                return False, f"Username '{username}' already registered"

        pid = f"USRP-{self.next_player_seq:08d}"
        self.next_player_seq += 1
        token = secrets.token_hex(24)

        rec = {
            "player_id": pid,
            "username": username,
            "display_name": display_name or username,
            "password_hash": pwd_hash,
            "created_at": time.time(),
            "last_login": time.time(),
            "is_admin": data.get("is_admin", False)
        }
        self.players[pid] = rec
        self.auth_tokens[token] = pid
        return True, {"player_id": pid, "token": token, "display_name": rec["display_name"]}

    def login_player(self, data):
        username = data.get("username", "").strip()
        pwd_hash = data.get("password_hash", "")
        for p in self.players.values():
            if p["username"].lower() == username.lower():
                if p["password_hash"] and pwd_hash and p["password_hash"] != pwd_hash:
                    return False, "Invalid credentials"
                token = secrets.token_hex(24)
                self.auth_tokens[token] = p["player_id"]
                p["last_login"] = time.time()
                return True, {"player_id": p["player_id"], "token": token, "display_name": p["display_name"]}
        return False, f"User '{username}' not found"

    def get_player_by_token(self, token):
        pid = self.auth_tokens.get(token)
        if pid and pid in self.players:
            return self.players[pid]
        return None

    # -------------------------------------------------------------------------
    # Corporate Charters
    # -------------------------------------------------------------------------
    def register_company(self, data):
        owner_id = data.get("owner_player_id")
        name = data.get("company_name", "").strip()
        home_world = int(data.get("home_world_id", 0))
        if not owner_id or not name:
            return False, "owner_player_id and company_name are required"
        if owner_id not in self.players:
            return False, f"Owner player '{owner_id}' not found"

        cid = f"CORP-{self.next_charter_seq:08d}"
        self.next_charter_seq += 1

        rec = {
            "charter_id": cid,
            "company_name": name,
            "owner_player_id": owner_id,
            "home_world_id": home_world,
            "world_presences": [home_world],
            "delegates": data.get("delegates", []),
            "treasury_balance": int(data.get("treasury_balance", 1000000)),
            "created_at": time.time()
        }
        self.charters[cid] = rec
        return True, rec

    def list_companies(self, owner_filter=None, world_filter=None):
        results = []
        for c in self.charters.values():
            if owner_filter and c["owner_player_id"] != owner_filter:
                continue
            if world_filter is not None and world_filter not in c["world_presences"]:
                continue
            results.append(c)
        return results

    def add_company_presence(self, data):
        cid = data.get("charter_id")
        wid = data.get("world_id")
        if cid not in self.charters:
            return False, "charter not found"
        if wid is None:
            return False, "world_id required"
        wid = int(wid)
        if wid not in self.charters[cid]["world_presences"]:
            self.charters[cid]["world_presences"].append(wid)
        return True, self.charters[cid]

    # -------------------------------------------------------------------------
    # Dynamic World Directory & Heartbeats
    # -------------------------------------------------------------------------
    def register_world(self, data):
        world_id = data.get("world_id")
        if world_id is None:
            return False, "world_id required"
        self.worlds[world_id] = {
            "world_id": world_id,
            "phase": data.get("phase", 3),
            "name": data.get("name", f"World {world_id}"),
            "address": data.get("address", f"127.0.0.1:{3979 + world_id}"),
            "description": data.get("description", "Federated Railway System"),
            "manifest_token": data.get("manifest_token", ""),
            "active_clients": int(data.get("active_clients", 0)),
            "max_clients": int(data.get("max_clients", 16)),
            "active_trains": int(data.get("active_trains", 0)),
            "status": data.get("status", "online"),
            "last_heartbeat": time.time()
        }
        return True, self.worlds[world_id]

    def update_heartbeat(self, data):
        world_id = data.get("world_id")
        if world_id is None:
            return False, "world_id required"
        world_id = int(world_id)
        if world_id not in self.worlds:
            # Auto-register on heartbeat
            self.register_world(data)
        rec = self.worlds[world_id]
        rec["last_heartbeat"] = time.time()
        if "address" in data:
            rec["address"] = data["address"]
        if "description" in data:
            rec["description"] = data["description"]
        if "active_clients" in data:
            rec["active_clients"] = int(data["active_clients"])
        if "max_clients" in data:
            rec["max_clients"] = int(data["max_clients"])
        if "active_trains" in data:
            rec["active_trains"] = int(data["active_trains"])
        if "status" in data:
            rec["status"] = data["status"]
        else:
            rec["status"] = "online"
        return True, rec

    def get_world_directory(self, min_phase=0, prune_stale=False, stale_threshold=60.0):
        now = time.time()
        worlds_to_return = []
        stale_ids = []
        for wid, w in self.worlds.items():
            age = now - w["last_heartbeat"]
            if age > stale_threshold:
                if prune_stale:
                    stale_ids.append(wid)
                    continue
                else:
                    w["status"] = "unreachable"
            if w["phase"] >= min_phase:
                worlds_to_return.append(w)

        for wid in stale_ids:
            del self.worlds[wid]

        return worlds_to_return

    def register_route(self, data):
        route_id = data.get("route_id")
        if not route_id:
            return False, "route_id required"
        self.routes[route_id] = {
            "route_id": route_id,
            "source_world": data.get("source_world"),
            "source_gate": data.get("source_gate"),
            "dest_world": data.get("dest_world"),
            "dest_gate": data.get("dest_gate"),
            "transit_delay_sec": data.get("transit_delay_sec", 5.0),
            "max_bandwidth_trains_per_min": int(data.get("max_bandwidth_trains_per_min", data.get("max_bandwidth", 10))),
            "max_active_in_transit": int(data.get("max_active_in_transit", 8)),
            "current_in_transit_count": 0,
            "total_trains_dispatched": 0,
            "congestion_level": "CLEAR"
        }
        return True, self.routes[route_id]

    def update_corridor_limits(self, data):
        route_id = data.get("route_id")
        route = self.routes.get(route_id)
        if not route:
            return False, f"Route {route_id} not found"
        if "max_bandwidth" in data:
            route["max_bandwidth_trains_per_min"] = int(data["max_bandwidth"])
        if "max_bandwidth_trains_per_min" in data:
            route["max_bandwidth_trains_per_min"] = int(data["max_bandwidth_trains_per_min"])
        if "max_active_in_transit" in data:
            route["max_active_in_transit"] = int(data["max_active_in_transit"])
        if "transit_delay_sec" in data:
            route["transit_delay_sec"] = float(data["transit_delay_sec"])
        self.evaluate_corridor_congestion(route_id)
        return True, route

    def evaluate_corridor_congestion(self, route_id):
        route = self.routes.get(route_id)
        if not route:
            return "CLEAR"
        active = route.get("current_in_transit_count", 0)
        cap = route.get("max_active_in_transit", 8)
        if cap <= 0:
            cap = 1
        util = active / cap
        if util < 0.5:
            level = "CLEAR"
        elif util < 0.8:
            level = "MODERATE"
        elif util <= 1.0:
            level = "CONGESTED"
        else:
            level = "SATURATED"
        route["congestion_level"] = level
        return level

    # -------------------------------------------------------------------------
    # Consist Transfers & Conservation Accounting
    # -------------------------------------------------------------------------
    def initiate_transfer(self, data):
        source_world = data.get("source_world")
        dest_world = data.get("dest_world")
        snapshot_b64 = data.get("snapshot_base64", "")
        total_cargo = int(data.get("total_cargo", 0))
        cargo_breakdown = data.get("cargo_breakdown", {})
        valuation_credits = int(data.get("valuation_credits", 0))

        if source_world is None or dest_world is None:
            return False, "source_world and dest_world required"

        tx_id = f"TRANSFER-X{self.next_transfer_seq:09d}"
        self.next_transfer_seq += 1

        transit_delay = data.get("transit_delay_sec", 5.0)
        source_gate = data.get("source_gate", 0)
        priority = data.get("priority", "STANDARD")
        route_id = data.get("route_id")

        # Route lookup
        route = None
        if route_id and route_id in self.routes:
            route = self.routes[route_id]
        else:
            for r in self.routes.values():
                if r["source_world"] == source_world and r["source_gate"] == source_gate:
                    route = r
                    break

        if route:
            transit_delay = route["transit_delay_sec"]
            rec_route_id = route["route_id"]
            route["current_in_transit_count"] += 1
            route["total_trains_dispatched"] += 1
            cong = self.evaluate_corridor_congestion(rec_route_id)
            if cong == "CLEAR":
                mult = 1.0
            elif cong == "MODERATE":
                mult = 1.2
            elif cong == "CONGESTED":
                mult = 1.5
            else: # SATURATED
                mult = 2.0

            # Express/PriorityUrgent 50% penalty relief
            is_priority = False
            if isinstance(priority, str) and priority.upper() in ("EXPRESS", "PRIORITY_URGENT", "URGENT"):
                is_priority = True
            elif isinstance(priority, (int, float)) and int(priority) in (2, 3):
                is_priority = True

            if is_priority:
                penalty = mult - 1.0
                mult = 1.0 + (penalty * 0.5)

            transit_delay *= mult

        # Normalize breakdown
        normalized_breakdown = {}
        if cargo_breakdown:
            for k, v in cargo_breakdown.items():
                normalized_breakdown[int(k)] = int(v)
        elif total_cargo > 0:
            normalized_breakdown[0] = total_cargo

        # Accounting: initiate commodity in transit
        for ctype, amount in normalized_breakdown.items():
            self.commodity_initiated[ctype] = self.commodity_initiated.get(ctype, 0) + amount
            self.commodity_in_transit[ctype] = self.commodity_in_transit.get(ctype, 0) + amount

        cargo_units = sum(normalized_breakdown.values()) if normalized_breakdown else total_cargo

        # Track supply chain matrix flows between developmental phases
        sw_rec = self.worlds.get(source_world)
        dw_rec = self.worlds.get(dest_world)
        if sw_rec and dw_rec and cargo_units > 0:
            self.supply_chain_matrix["total_interplanetary_cargo"] += cargo_units
            self.supply_chain_matrix["total_tariffs_generated"] += (cargo_units * 10)
            src_phase = sw_rec.get("phase", 3)
            dst_phase = dw_rec.get("phase", 1)
            if src_phase == 3 and dst_phase == 2:
                self.supply_chain_matrix["frontier_to_refinery_cargo"] += cargo_units
            elif src_phase == 2 and dst_phase == 1:
                self.supply_chain_matrix["refinery_to_core_cargo"] += cargo_units
            elif src_phase == 3 and dst_phase == 1:
                self.supply_chain_matrix["frontier_to_core_cargo"] += cargo_units
            elif src_phase == 1:
                self.supply_chain_matrix["core_export_cargo"] += cargo_units

        rec = {
            "transfer_id": tx_id,
            "source_world": source_world,
            "dest_world": dest_world,
            "source_gate": source_gate,
            "dest_gate": data.get("dest_gate", 0),
            "snapshot_base64": snapshot_b64,
            "state": "LOCKED",
            "departure_time": 0.0,
            "arrival_time": time.time() + transit_delay,
            "transit_delay_sec": transit_delay,
            "total_cargo": cargo_units,
            "cargo_breakdown": {str(k): v for k, v in normalized_breakdown.items()},
            "valuation_credits": valuation_credits,
            "route_id": route["route_id"] if route else 0,
            "priority": str(priority),
            "status_message": "Transfer locked and ready for departure"
        }
        self.transfers[tx_id] = rec
        return True, rec

    def depart_transfer(self, data):
        tx_id = data.get("transfer_id")
        rec = self.transfers.get(tx_id)
        if not rec:
            return False, f"Transfer {tx_id} not found"
        if rec["state"] not in ("PREPARING", "LOCKED"):
            return False, f"Invalid state {rec['state']} for departure"

        now = time.time()
        rec["departure_time"] = now
        rec["arrival_time"] = now + rec["transit_delay_sec"]
        rec["state"] = "IN_TRANSIT"
        rec["status_message"] = "Consist in transit through portal wormhole"
        return True, rec

    def query_pending(self, dest_world):
        now = time.time()
        ready = []
        for tx_id, rec in self.transfers.items():
            if rec["dest_world"] == dest_world and rec["state"] == "IN_TRANSIT" and now >= rec["arrival_time"]:
                ready.append(tx_id)
        return ready

    def claim_transfer(self, data):
        tx_id = data.get("transfer_id")
        dest_world = data.get("dest_world")
        rec = self.transfers.get(tx_id)
        if not rec:
            return False, f"Transfer {tx_id} not found"
        if rec["dest_world"] != dest_world or rec["state"] != "IN_TRANSIT":
            return False, f"Transfer {tx_id} not available for claim by world {dest_world}"

        rec["state"] = "ARRIVAL_PENDING"
        rec["status_message"] = "Claimed by destination world; awaiting emergence clearance"
        return True, rec

    def confirm_arrival(self, data):
        tx_id = data.get("transfer_id")
        dest_world = data.get("dest_world")
        success = data.get("success", True)
        reason = data.get("reason", "")

        rec = self.transfers.get(tx_id)
        if not rec:
            return False, f"Transfer {tx_id} not found"
        if rec["dest_world"] != dest_world or rec["state"] != "ARRIVAL_PENDING":
            return False, f"Transfer {tx_id} not pending arrival for world {dest_world}"

        if success:
            rec["state"] = "COMPLETED"
            rec["status_message"] = "Consist emerged and materialized successfully"

            # Accounting: move commodity from transit to completed
            breakdown = rec.get("cargo_breakdown", {})
            for cstr, amt in breakdown.items():
                ctype = int(cstr)
                self.commodity_in_transit[ctype] = max(0, self.commodity_in_transit.get(ctype, 0) - amt)
                self.commodity_completed[ctype] = self.commodity_completed.get(ctype, 0) + amt

            # Update world trade balance
            sw = rec["source_world"]
            dw = rec["dest_world"]
            pair_key = f"{sw}->{dw}"
            if pair_key not in self.world_trade:
                self.world_trade[pair_key] = {"cargo_counts": {}, "total_credits": 0, "completed_transfers": 0}
            trade_entry = self.world_trade[pair_key]
            trade_entry["completed_transfers"] += 1
            trade_entry["total_credits"] += rec.get("valuation_credits", 0)
            for cstr, amt in breakdown.items():
                trade_entry["cargo_counts"][cstr] = trade_entry["cargo_counts"].get(cstr, 0) + amt

            # Relieve corridor active transit count
            rt_id = rec.get("route_id")
            if rt_id and rt_id in self.routes:
                r = self.routes[rt_id]
                r["current_in_transit_count"] = max(0, r["current_in_transit_count"] - 1)
                self.evaluate_corridor_congestion(rt_id)
        else:
            rec["state"] = "RECOVERY_REQUIRED"
            rec["status_message"] = reason or "Emergence failed on destination server"
        return True, rec

    # -------------------------------------------------------------------------
    # Ledger & Conservation Audit
    # -------------------------------------------------------------------------
    def get_audit(self):
        total_initiated = len(self.transfers)
        total_cargo_initiated = sum(r["total_cargo"] for r in self.transfers.values())
        total_completed = sum(1 for r in self.transfers.values() if r["state"] == "COMPLETED")
        total_cargo_completed = sum(r["total_cargo"] for r in self.transfers.values() if r["state"] == "COMPLETED")
        total_in_transit = sum(1 for r in self.transfers.values() if r["state"] in ("IN_TRANSIT", "ARRIVAL_PENDING", "LOCKED", "PREPARING", "RECOVERY_REQUIRED"))
        total_cargo_in_transit = sum(r["total_cargo"] for r in self.transfers.values() if r["state"] in ("IN_TRANSIT", "ARRIVAL_PENDING", "LOCKED", "PREPARING", "RECOVERY_REQUIRED"))

        is_conserved = total_cargo_initiated == (total_cargo_completed + total_cargo_in_transit)
        return {
            "total_transfers_initiated": total_initiated,
            "total_transfers_completed": total_completed,
            "total_transfers_in_transit": total_in_transit,
            "total_cargo_initiated": total_cargo_initiated,
            "total_cargo_completed": total_cargo_completed,
            "total_cargo_in_transit": total_cargo_in_transit,
            "is_conserved": is_conserved,
        }

    def get_detailed_audit(self):
        # Per cargo type conservation audit
        all_types = set(self.commodity_initiated.keys()) | set(self.commodity_in_transit.keys()) | set(self.commodity_completed.keys())
        commodity_reports = []
        all_conserved = True

        for ctype in sorted(all_types):
            init = self.commodity_initiated.get(ctype, 0)
            transit = self.commodity_in_transit.get(ctype, 0)
            comp = self.commodity_completed.get(ctype, 0)
            conserved = (init == (transit + comp))
            if not conserved:
                all_conserved = False
            commodity_reports.append({
                "cargo_type": ctype,
                "initiated": init,
                "in_transit": transit,
                "completed": comp,
                "conserved": conserved
            })

        return {
            "all_conserved": all_conserved,
            "commodity_reports": commodity_reports,
            "total_commodities_tracked": len(all_types)
        }

    def get_trade_balances(self):
        # Summarize exports/imports per world
        summaries = {}
        for pair_key, trade in self.world_trade.items():
            sw_str, dw_str = pair_key.split("->")
            sw = int(sw_str)
            dw = int(dw_str)

            if sw not in summaries:
                summaries[sw] = {"world_id": sw, "total_exported": 0, "total_imported": 0, "net_credits": 0, "cargo_exported": {}, "cargo_imported": {}}
            if dw not in summaries:
                summaries[dw] = {"world_id": dw, "total_exported": 0, "total_imported": 0, "net_credits": 0, "cargo_exported": {}, "cargo_imported": {}}

            credits = trade.get("total_credits", 0)
            summaries[sw]["net_credits"] += credits
            summaries[dw]["net_credits"] -= credits

            for cstr, amt in trade.get("cargo_counts", {}).items():
                summaries[sw]["total_exported"] += amt
                summaries[sw]["cargo_exported"][cstr] = summaries[sw]["cargo_exported"].get(cstr, 0) + amt

                summaries[dw]["total_imported"] += amt
                summaries[dw]["cargo_imported"][cstr] = summaries[dw]["cargo_imported"].get(cstr, 0) + amt

        return {
            "world_balances": list(summaries.values()),
            "route_flows": self.world_trade
        }

    # -------------------------------------------------------------------------
    # Megacity & Empire Economy
    # -------------------------------------------------------------------------
    def register_megacity(self, data):
        town_id = data.get("town_id")
        if town_id is None:
            return False, "town_id required"
        town_id = int(town_id)
        world_id = int(data.get("world_id", 0))
        name = data.get("name", f"Megacity {town_id}")
        population = int(data.get("population", 1000))

        quota = [
            max(50, population // 20),
            max(30, population // 40),
            max(10, population // 100)
        ]
        if "custom_quota" in data:
            quota = [int(x) for x in data["custom_quota"]]

        rec = {
            "town_id": town_id,
            "world_id": world_id,
            "name": name,
            "population": population,
            "monthly_quota": quota,
            "delivered_current": [0, 0, 0],
            "delivered_last": [0, 0, 0],
            "satisfaction_pct": [0.0, 0.0, 0.0],
            "overall_supply_index": 0.0,
            "growth_state": "SUBSISTENCE",
            "growth_multiplier": 1.0,
            "passenger_multiplier": 1.0
        }
        self.megacities[town_id] = rec
        return True, rec

    def record_megacity_delivery(self, data):
        town_id = data.get("town_id")
        if town_id is None:
            return False, "town_id required"
        town_id = int(town_id)
        rec = self.megacities.get(town_id)
        if not rec:
            return False, f"Megacity {town_id} not found"

        amount = int(data.get("amount", 0))
        if amount <= 0:
            return True, rec

        tier = data.get("tier")
        if tier is not None:
            tier = int(tier)
        elif "cargo_type" in data:
            cargo_type = int(data["cargo_type"])
            if cargo_type in (4, 6, 11, 12):
                tier = 0
            elif cargo_type in (5, 7, 8, 9):
                tier = 1
            elif cargo_type in (10, 3):
                tier = 2
            else:
                tier = cargo_type % 3
        else:
            tier = 0

        if 0 <= tier < 3:
            rec["delivered_current"][tier] += amount

        return True, rec

    def evaluate_megacity_supply(self, data=None):
        target_town_id = data.get("town_id") if data else None
        evaluated = []
        for tid, rec in self.megacities.items():
            if target_town_id is not None and int(target_town_id) != tid:
                continue

            rec["delivered_last"] = list(rec["delivered_current"])
            rec["delivered_current"] = [0, 0, 0]

            total_sat = 0.0
            for i in range(3):
                quota = rec["monthly_quota"][i]
                if quota > 0:
                    rec["satisfaction_pct"][i] = round(rec["delivered_last"][i] / quota, 3)
                else:
                    rec["satisfaction_pct"][i] = 1.0
                total_sat += rec["satisfaction_pct"][i]

            rec["overall_supply_index"] = round(total_sat / 3.0, 3)

            if rec["satisfaction_pct"][0] < 0.5:
                rec["growth_state"] = "STARVATION"
                rec["growth_multiplier"] = 0.0
                rec["passenger_multiplier"] = 0.5
            elif rec["satisfaction_pct"][0] >= 1.0 and rec["satisfaction_pct"][1] >= 1.0 and rec["satisfaction_pct"][2] >= 1.0:
                rec["growth_state"] = "HYPER_GROWTH"
                rec["growth_multiplier"] = 2.0
                rec["passenger_multiplier"] = 1.5
            elif rec["satisfaction_pct"][0] >= 1.0 and rec["satisfaction_pct"][1] >= 1.0:
                rec["growth_state"] = "METROPOLITAN_BOOM"
                rec["growth_multiplier"] = 1.5
                rec["passenger_multiplier"] = 1.25
            else:
                rec["growth_state"] = "SUBSISTENCE"
                rec["growth_multiplier"] = 1.0
                rec["passenger_multiplier"] = 1.0

            evaluated.append(rec)

        return True, evaluated

    def get_megacity_status(self, town_id=None):
        if town_id is not None:
            tid = int(town_id)
            if tid in self.megacities:
                return [self.megacities[tid]]
            return []
        return list(self.megacities.values())

    def get_corridors(self):
        return list(self.routes.values())

    def get_supply_chain_matrix(self):
        return self.supply_chain_matrix

AUTHORITY = UniverseAuthority()

class AuthorityHandler(BaseHTTPRequestHandler):
    def _send_json(self, status_code, obj):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _read_json(self):
        content_len = int(self.headers.get("Content-Length", 0))
        if content_len == 0:
            return {}
        raw = self.rfile.read(content_len).decode("utf-8")
        return json.loads(raw)

    def do_GET(self):
        url = urlparse(self.path)
        qs = parse_qs(url.query)

        if url.path in ("/worlds", "/directory/worlds"):
            min_phase = int(qs.get("min_phase", [0])[0])
            prune = qs.get("prune", ["false"])[0].lower() in ("true", "1")
            self._send_json(200, AUTHORITY.get_world_directory(min_phase=min_phase, prune_stale=prune))
        elif url.path == "/companies/list":
            owner = qs.get("owner", [None])[0]
            wid = int(qs.get("world", [-1])[0])
            self._send_json(200, AUTHORITY.list_companies(owner_filter=owner, world_filter=wid if wid >= 0 else None))
        elif url.path == "/transfers/pending":
            dest_world = int(qs.get("dest_world", [0])[0])
            pending = AUTHORITY.query_pending(dest_world)
            self._send_json(200, {"pending_transfers": pending})
        elif url.path == "/ledger/status":
            self._send_json(200, AUTHORITY.get_audit())
        elif url.path == "/ledger/audit_detailed":
            self._send_json(200, AUTHORITY.get_detailed_audit())
        elif url.path == "/ledger/trade_balance":
            self._send_json(200, AUTHORITY.get_trade_balances())
        elif url.path == "/corridors/list":
            self._send_json(200, AUTHORITY.get_corridors())
        elif url.path == "/megacity/status":
            tid = qs.get("town_id", [None])[0]
            self._send_json(200, AUTHORITY.get_megacity_status(town_id=tid))
        elif url.path == "/economy/matrix":
            self._send_json(200, AUTHORITY.get_supply_chain_matrix())
        elif url.path.startswith("/transfers/"):
            tx_id = url.path.split("/")[-1]
            rec = AUTHORITY.transfers.get(tx_id)
            if rec:
                self._send_json(200, rec)
            else:
                self._send_json(404, {"error": "Transfer not found"})
        else:
            self._send_json(404, {"error": "Endpoint not found"})

    def do_POST(self):
        url = urlparse(self.path)
        data = self._read_json()

        # Auth endpoints
        if url.path == "/auth/register":
            ok, res = AUTHORITY.register_player(data)
            self._send_json(200 if ok else 400, res if ok else {"error": res})
        elif url.path == "/auth/login":
            ok, res = AUTHORITY.login_player(data)
            self._send_json(200 if ok else 401, res if ok else {"error": res})

        # Company endpoints
        elif url.path == "/companies/register":
            ok, res = AUTHORITY.register_company(data)
            self._send_json(200 if ok else 400, res if ok else {"error": res})
        elif url.path == "/companies/presence":
            ok, res = AUTHORITY.add_company_presence(data)
            self._send_json(200 if ok else 400, res if ok else {"error": res})

        # Directory & World endpoints
        elif url.path in ("/worlds/register", "/directory/register"):
            ok, res = AUTHORITY.register_world(data)
            self._send_json(200 if ok else 400, res if ok else {"error": res})
        elif url.path == "/directory/heartbeat":
            ok, res = AUTHORITY.update_heartbeat(data)
            self._send_json(200 if ok else 400, res if ok else {"error": res})

        # Routing & Transfer endpoints
        elif url.path in ("/portal_links", "/corridors/register"):
            ok, res = AUTHORITY.register_route(data)
            self._send_json(200 if ok else 400, res if ok else {"error": res})
        elif url.path == "/corridors/update":
            ok, res = AUTHORITY.update_corridor_limits(data)
            self._send_json(200 if ok else 400, res if ok else {"error": res})
        elif url.path == "/transfers/initiate":
            ok, res = AUTHORITY.initiate_transfer(data)
            self._send_json(200 if ok else 400, res if ok else {"error": res})
        elif url.path == "/transfers/depart":
            ok, res = AUTHORITY.depart_transfer(data)
            self._send_json(200 if ok else 400, res if ok else {"error": res})
        elif url.path == "/transfers/claim":
            ok, res = AUTHORITY.claim_transfer(data)
            self._send_json(200 if ok else 400, res if ok else {"error": res})
        elif url.path == "/transfers/confirm":
            ok, res = AUTHORITY.confirm_arrival(data)
            self._send_json(200 if ok else 400, res if ok else {"error": res})

        # Megacity endpoints
        elif url.path == "/megacity/register":
            ok, res = AUTHORITY.register_megacity(data)
            self._send_json(200 if ok else 400, res if ok else {"error": res})
        elif url.path in ("/megacity/demand", "/megacity/deliver"):
            ok, res = AUTHORITY.record_megacity_delivery(data)
            self._send_json(200 if ok else 400, res if ok else {"error": res})
        elif url.path == "/megacity/eval":
            ok, res = AUTHORITY.evaluate_megacity_supply(data)
            self._send_json(200 if ok else 400, res if ok else {"error": res})

        elif url.path == "/reset":
            AUTHORITY.reset()
            self._send_json(200, {"status": "reset"})
        else:
            self._send_json(404, {"error": "Endpoint not found"})

    def log_message(self, format, *args):
        # Silence default stderr logging unless in debug mode
        pass

def main():
    parser = argparse.ArgumentParser(description="OpenSpaceTTD Universe Authority Daemon")
    parser.add_argument("--host", default="127.0.0.1", help="Host interface to bind")
    parser.add_argument("--port", type=int, default=8080, help="Port to listen on")
    args = parser.parse_args()

    server = HTTPServer((args.host, args.port), AuthorityHandler)
    print(f"[UniverseAuthority] Running on http://{args.host}:{args.port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[UniverseAuthority] Shutting down.")
        server.server_close()

if __name__ == "__main__":
    main()
