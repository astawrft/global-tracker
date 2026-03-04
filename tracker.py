import json
import math
import os
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Tuple

ENTITIES = ["USA", "EU", "Brazil", "Russia", "India", "China", "UAE"]


@dataclass
class Metric:
    key: str
    name: str
    category: str
    weight: float
    mode: str  # "threshold" | "rank" | "delta"
    direction: str  # "higher_better" | "lower_better"
    # threshold config
    thresholds: Optional[List[Dict[str, Any]]] = None  # [{"max": 3, "points": 3}, ...] (for lower_better usually)
    # rank config
    max_points: Optional[float] = None
    # delta config
    delta_step: Optional[float] = None  # how much change counts as 1 "step"
    delta_points_per_step: Optional[float] = None  # points per step
    delta_cap: Optional[float] = None  # cap absolute delta points


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


def threshold_points(value: float, thresholds: List[Dict[str, Any]]) -> float:
    # thresholds are evaluated in order; first match wins
    for t in thresholds:
        if value <= float(t["max"]):
            return float(t["points"])
    return float(thresholds[-1]["points"]) if thresholds else 0.0


def normalize_rank(values: Dict[str, float], direction: str, max_points: float) -> Dict[str, float]:
    # linear scaling between min and max
    items = list(values.items())
    vals = [v for _, v in items]
    vmin, vmax = min(vals), max(vals)

    if math.isclose(vmin, vmax):
        # everyone equal -> give half of max_points (or max_points, your choice; half is "neutral")
        return {k: max_points / 2.0 for k, _ in items}

    scores = {}
    for k, v in items:
        # base normalized 0..1 where 1 is "better"
        if direction == "higher_better":
            norm = (v - vmin) / (vmax - vmin)
        else:
            norm = (vmax - v) / (vmax - vmin)
        scores[k] = norm * max_points
    return scores


def delta_points(current: float, previous: float, direction: str,
                 step: float, points_per_step: float, cap: float) -> float:
    # compute "improvement" so positive means good
    change = current - previous
    improvement = change if direction == "higher_better" else -change

    if step <= 0:
        step = 1.0

    steps = improvement / step
    pts = steps * points_per_step

    if cap is not None:
        pts = max(-abs(cap), min(abs(cap), pts))
    return pts


def parse_metrics(cfg: Dict[str, Any]) -> List[Metric]:
    metrics = []
    for m in cfg["metrics"]:
        metrics.append(Metric(
            key=m["key"],
            name=m.get("name", m["key"]),
            category=m.get("category", "Other"),
            weight=float(m.get("weight", 1.0)),
            mode=m["mode"],
            direction=m.get("direction", "higher_better"),
            thresholds=m.get("thresholds"),
            max_points=safe_float(m.get("max_points")),
            delta_step=safe_float(m.get("delta_step")),
            delta_points_per_step=safe_float(m.get("delta_points_per_step")),
            delta_cap=safe_float(m.get("delta_cap")),
        ))
    return metrics


def score_snapshot(metrics: List[Metric],
                   current: Dict[str, Dict[str, Any]],
                   previous: Optional[Dict[str, Dict[str, Any]]] = None) -> Dict[str, Any]:

    # Prepare rank-based metric maps
    rank_metric_values: Dict[str, Dict[str, float]] = {}
    for m in metrics:
        if m.mode == "rank":
            vals = {}
            for e in ENTITIES:
                v = safe_float(current.get(e, {}).get(m.key))
                if v is not None:
                    vals[e] = v
            # Only compute if we have at least 2 values
            if len(vals) >= 2:
                rank_metric_values[m.key] = vals

    rank_metric_points: Dict[str, Dict[str, float]] = {}
    for m in metrics:
        if m.mode == "rank" and m.key in rank_metric_values:
            mp = m.max_points if m.max_points is not None else 5.0
            rank_metric_points[m.key] = normalize_rank(rank_metric_values[m.key], m.direction, mp)

    results: Dict[str, Any] = {e: {"total": 0.0, "breakdown": []} for e in ENTITIES}

    for e in ENTITIES:
        total = 0.0
        for m in metrics:
            cur_val = safe_float(current.get(e, {}).get(m.key))
            prev_val = safe_float(previous.get(e, {}).get(m.key)) if previous else None

            base_pts = 0.0
            note = ""

            if cur_val is None:
                base_pts = 0.0
                note = "missing"
            elif m.mode == "threshold":
                base_pts = threshold_points(cur_val, m.thresholds or [])
            elif m.mode == "rank":
                if m.key in rank_metric_points and e in rank_metric_points[m.key]:
                    base_pts = rank_metric_points[m.key][e]
                else:
                    base_pts = 0.0
                    note = "rank unavailable"
            elif m.mode == "delta":
                if prev_val is None:
                    base_pts = 0.0
                    note = "no previous"
                else:
                    step = m.delta_step if m.delta_step is not None else 1.0
                    pps = m.delta_points_per_step if m.delta_points_per_step is not None else 1.0
                    cap = m.delta_cap if m.delta_cap is not None else 3.0
                    base_pts = delta_points(cur_val, prev_val, m.direction, step, pps, cap)
            else:
                note = "unknown mode"

            weighted = base_pts * m.weight
            total += weighted

            results[e]["breakdown"].append({
                "metric": m.key,
                "name": m.name,
                "category": m.category,
                "value": cur_val,
                "prev": prev_val,
                "mode": m.mode,
                "base_points": round(base_pts, 3),
                "weight": m.weight,
                "weighted_points": round(weighted, 3),
                "note": note
            })

        results[e]["total"] = round(total, 3)

    return results


def print_leaderboard(results: Dict[str, Any], top_n: Optional[int] = None) -> None:
    rows = sorted(((e, results[e]["total"]) for e in ENTITIES), key=lambda x: x[1], reverse=True)
    if top_n:
        rows = rows[:top_n]

    print("\n=== GLOBAL TRACKER LEADERBOARD ===")
    for i, (e, pts) in enumerate(rows, start=1):
        print(f"{i:>2}. {e:<7} {pts:>8.3f} pts")


def print_country_breakdown(results: Dict[str, Any], entity: str) -> None:
    r = results[entity]
    print(f"\n--- {entity} breakdown (total {r['total']:.3f}) ---")
    for b in r["breakdown"]:
        val = "NA" if b["value"] is None else f"{b['value']}"
        prev = "NA" if b["prev"] is None else f"{b['prev']}"
        note = f" [{b['note']}]" if b["note"] else ""
        print(f"- {b['category']}: {b['name']} ({b['mode']}) | val={val} prev={prev} "
              f"=> {b['weighted_points']:.3f} (base {b['base_points']:.3f} * w {b['weight']}){note}")


if __name__ == "__main__":
    # paths
    cfg_path = "config.json"
    current_path = os.environ.get("CURRENT_DATA", "data/2026-03-02.json")
    previous_path = os.environ.get("PREVIOUS_DATA", "data/2026-02-01.json")

    cfg = load_json(cfg_path)
    metrics = parse_metrics(cfg)

    current = load_json(current_path)
    previous = load_json(previous_path) if os.path.exists(previous_path) else None

    results = score_snapshot(metrics, current, previous)

    print_leaderboard(results)

    # Print a few breakdowns (edit as you like)
    for e in ["USA", "China", "EU"]:
        if e in ENTITIES:
            print_country_breakdown(results, e)