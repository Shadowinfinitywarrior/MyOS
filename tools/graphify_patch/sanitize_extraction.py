"""Extraction post-pass that cleans edge-integrity issues graphify's auditor flags.

Run after graphify merges AST + semantic extraction into
``graphify-out/.graphify_extract.json`` and before the graph is built.

  * Dangling import endpoints (a ``#include "foo.h"`` whose target id is the bare
    stem ``foo_h`` rather than the header's file-node id) are retargeted to the
    real corpus header node; imports that point at host/tooling headers outside
    the corpus are dropped.
  * Self-import edges are dropped.  Self-``calls`` edges are genuine recursion, so
    they are folded into a ``recursive: true`` node attribute instead of an edge.
  * Parallel edges between the same pair collapse to one (the simple graph build
    would silently drop the rest), keeping the strongest confidence and recording
    ``collapsed_edges``.

Idempotent: safe to run more than once.
"""

from __future__ import annotations

import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXTRACT = ROOT / "graphify-out" / ".graphify_extract.json"

STRENGTH = {"EXTRACTED": 3, "INFERRED": 2, "AMBIGUOUS": 1}
IMPORT_RELATIONS = {"imports", "imports_from", "includes"}


def _is_host_tool(source_file: str) -> bool:
    # tools/ is compiled against the host libc, so its <...> includes are
    # external system headers, never in-corpus kernel headers.
    return source_file.replace("\\", "/").startswith("tools/")


def sanitize(extraction: dict) -> tuple[dict, dict]:
    nodes = extraction["nodes"]
    edges = extraction["edges"]
    node_by_id = {n["id"]: n for n in nodes}
    node_ids = set(node_by_id)

    header_map: dict[str, str] = {}
    for n in nodes:
        sf = n.get("source_file") or ""
        if sf.lower().endswith(".h"):
            stem = os.path.splitext(os.path.basename(sf))[0]
            header_map.setdefault(stem, n["id"])

    stats = {
        "dangling_before": 0,
        "retargeted": 0,
        "dropped_unresolved": 0,
        "dropped_self_import": 0,
        "recursion_marked": 0,
        "collapsed": 0,
    }

    fixed = []
    for e in edges:
        s, t = e["source"], e["target"]
        if s not in node_ids or t not in node_ids:
            stats["dangling_before"] += 1
            if e.get("relation") in IMPORT_RELATIONS:
                bad = s if s not in node_ids else t
                stem = bad[:-2] if bad.endswith("_h") else bad
                if stem in header_map and not _is_host_tool(e.get("source_file", "")):
                    e = dict(e)
                    if t == bad:
                        e["target"] = header_map[stem]
                    else:
                        e["source"] = header_map[stem]
                    e["resolved_from"] = bad
                    stats["retargeted"] += 1
                    fixed.append(e)
                    continue
            stats["dropped_unresolved"] += 1
            continue
        if s == t:
            if e.get("relation") == "calls":
                node_by_id[s]["recursive"] = True
                stats["recursion_marked"] += 1
                continue
            stats["dropped_self_import"] += 1
            continue
        fixed.append(e)

    dedup: dict[frozenset, dict] = {}
    order: list[frozenset] = []
    for e in fixed:
        key = frozenset((e["source"], e["target"]))
        if key not in dedup:
            row = dict(e)
            row["collapsed_edges"] = 1
            dedup[key] = row
            order.append(key)
        else:
            cur = dedup[key]
            cur["collapsed_edges"] += 1
            locs = cur.setdefault("collapsed_locations", [])
            if e.get("source_location"):
                locs.append(e["source_location"])
            if STRENGTH.get(e.get("confidence"), 0) > STRENGTH.get(cur.get("confidence"), 0):
                count = cur["collapsed_edges"]
                locs = cur.get("collapsed_locations", [])
                cur.clear()
                cur.update(e)
                cur["collapsed_edges"] = count
                cur["collapsed_locations"] = locs
            stats["collapsed"] += 1

    extraction["edges"] = [dedup[k] for k in order]
    return extraction, stats


def main() -> int:
    extraction = json.loads(EXTRACT.read_text(encoding="utf-8"))
    extraction, stats = sanitize(extraction)
    EXTRACT.write_text(
        json.dumps(extraction, indent=2, ensure_ascii=False), encoding="utf-8"
    )

    ids = {n["id"] for n in extraction["nodes"]}
    dangling = [
        e for e in extraction["edges"] if e["source"] not in ids or e["target"] not in ids
    ]
    self_loops = [e for e in extraction["edges"] if e["source"] == e["target"]]
    print(
        f"sanitized: {len(extraction['nodes'])} nodes, {len(extraction['edges'])} edges | "
        f"dangling {stats['dangling_before']}->{len(dangling)} "
        f"(retargeted {stats['retargeted']}, dropped {stats['dropped_unresolved']}) | "
        f"self-loops {stats['dropped_self_import']} dropped, "
        f"{stats['recursion_marked']} recursion-marked | "
        f"collapsed {stats['collapsed']}"
    )
    return 0 if not dangling and not self_loops else 1


if __name__ == "__main__":
    raise SystemExit(main())
