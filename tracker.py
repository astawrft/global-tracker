import json
import math
import os
from dataclasses import dataclass
from typing import Any, Dict, List, Optional

ENTITIES = ["USA", "EU", "Brazil", "Russia", "India", "China", "UAE"]


@dataclass
class Metric:
    key: str
    name: str
    category: str
    weight: float
    mode: str  # "max_norm" | "threshold"
    direction: str  # "higher_better" | "lower_better"
    max_points: float = 5.0  # used for max_norm
    thresholds: Optional[List[Dict[str, Any]]] = None  # used for threshold


def load_json(path: str) -> Dict[str, Any]:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def safe_float(x: Any) -> Optional[float]:
    if x is None:
        return None
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def clamp01(x: float) -> float:
    return max(0.0, min(1.0, x))


def threshold_points(value: float, thresholds: List[Dict[str, Any]]) -> float:
    # thresholds evaluated in order; first match wins
    for t in thresholds:
        if value <= float(t["max"]):
            return float(t["points"])
    return float(thresholds[-1]["points"]) if thresholds else 0.0


def parse_metrics(cfg: Dict[str, Any]) -> List[Metric]:
    metrics: List[Metric] = []
    for m in cfg.get("metrics", []):
        metrics.append(
            Metric(
                key=m["key"],
                name=m.get("name", m["key"]),
                category=m.get("category", "Other"),
                weight=float(m.get("weight", 1.0)),
                mode=m.get("mode", "max_norm"),
                direction=m.get("direction", "higher_better"),
                max_points=float(m.get("max_points", 5.0)),
                thresholds=m.get("thresholds"),
            )
        )
    return metrics


def compute_metric_max(metrics: List[Metric], current: Dict[str, Dict[str, Any]]) -> Dict[str, float]:
    """
    For each max_norm metric, compute max(country_values) across ENTITIES (ignoring missing).
    If no data exists for a metric, it won't appear in the returned dict.
    """
    max_map: Dict[str, float] = {}

    for m in metrics:
        if m.mode != "max_norm":
            continue

        vals: List[float] = []
        for e in ENTITIES:
            v = safe_float(current.get(e, {}).get(m.key))
            if v is not None:
                vals.append(v)

        if vals:
            max_map[m.key] = max(vals)

    return max_map


def score_snapshot(cfg: Dict[str, Any], metrics: List[Metric], current: Dict[str, Dict[str, Any]]) -> Dict[str, Any]:
    scoring_cfg = cfg.get("scoring", {})
    clamp_enabled = bool(scoring_cfg.get("clamp_0_1", True))
    round_to = int(scoring_cfg.get("round_points_to", 2))
    missing_policy = scoring_cfg.get("missing_value_policy", "zero")  # currently supports "zero"

    max_map = compute_metric_max(metrics, current)

    results: Dict[str, Any] = {e: {"total": 0.0, "breakdown": []} for e in ENTITIES}

    for e in ENTITIES:
        total = 0.0

        for m in metrics:
            cur_val = safe_float(current.get(e, {}).get(m.key))
            base_pts = 0.0
            note = ""

            if cur_val is None:
                base_pts = 0.0
                note = "missing" if missing_policy == "zero" else "missing"
            else:
                if m.mode == "threshold":
                    base_pts = threshold_points(cur_val, m.thresholds or [])
                elif m.mode == "max_norm":
                    vmax = max_map.get(m.key, None)

                    # avoid division by zero / absent max
                    if vmax is None or math.isclose(vmax, 0.0):
                        base_pts = 0.0
                        note = "max unavailable"
                    else:
                        if m.direction == "higher_better":
                            norm = cur_val / vmax
                        else:
                            norm = 1.0 - (cur_val / vmax)

                        if clamp_enabled:
                            norm = clamp01(norm)

                        base_pts = norm * m.max_points
                else:
                    base_pts = 0.0
                    note = "unknown mode"

            weighted = base_pts * m.weight
            total += weighted

            results[e]["breakdown"].append(
                {
                    "metric": m.key,
                    "name": m.name,
                    "category": m.category,
                    "value": cur_val,
                    "mode": m.mode,
                    "direction": m.direction,
                    "max_points": m.max_points if m.mode == "max_norm" else None,
                    "base_points": round(base_pts, round_to),
                    "weight": m.weight,
                    "weighted_points": round(weighted, round_to),
                    "note": note,
                }
            )

        results[e]["total"] = round(total, round_to)

    return results


def print_leaderboard(results: Dict[str, Any], top_n: Optional[int] = None) -> None:
    rows = sorted(((e, results[e]["total"]) for e in ENTITIES), key=lambda x: x[1], reverse=True)
    if top_n:
        rows = rows[:top_n]

    print("\n=== GLOBAL TRACKER LEADERBOARD ===")
    for i, (e, pts) in enumerate(rows, start=1):
        print(f"{i:>2}. {e:<7} {pts:>8.2f} pts")


def print_country_breakdown(results: Dict[str, Any], entity: str) -> None:
    r = results[entity]
    print(f"\n--- {entity} breakdown (total {r['total']:.2f}) ---")
    for b in r["breakdown"]:
        val = "NA" if b["value"] is None else f"{b['value']}"
        note = f" [{b['note']}]" if b["note"] else ""
        print(
            f"- {b['category']}: {b['name']} ({b['mode']}, {b['direction']}) | val={val} "
            f"=> {b['weighted_points']:.2f} (base {b['base_points']:.2f} * w {b['weight']}){note}"
        )


if __name__ == "__main__":
    cfg_path = "config.json"
    current_path = os.environ.get("CURRENT_DATA", "data/2026_global_data.json")

    cfg = load_json(cfg_path)
    metrics = parse_metrics(cfg)
    current = load_json(current_path)

    results = score_snapshot(cfg, metrics, current)

    print_leaderboard(results)

    for e in ["USA", "China", "EU"]:
        if e in ENTITIES:
            print_country_breakdown(results, e)
