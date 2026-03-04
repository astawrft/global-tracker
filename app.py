import streamlit as st
import json
import os
import pandas as pd
from copy import deepcopy
from datetime import datetime

# Optional: Plotly map
try:
    import plotly.express as px
    PLOTLY_OK = True
except Exception:
    PLOTLY_OK = False

st.set_page_config(page_title="Global Influence Tracker", page_icon="🌍", layout="wide")
st.title("🌍 Global Influence Tracker")
st.caption("Influence Score is computed from your indicators (higher = more influence).")

DATA_FOLDER = "data"

# =========================
# Admin / View-only (Option 2)
# =========================
ADMIN_PASSWORD = st.secrets.get("ADMIN_PASSWORD", "")

def check_admin(pw: str) -> bool:
    return bool(ADMIN_PASSWORD) and pw == ADMIN_PASSWORD

if "is_admin" not in st.session_state:
    st.session_state.is_admin = False

# =========================
# Processing log (REAL)
# =========================
if "console_lines" not in st.session_state:
    st.session_state.console_lines = []

def console_log(msg: str):
    ts = datetime.now().strftime("%H:%M:%S")
    st.session_state.console_lines.append(f"[{ts}] {msg}")
    st.session_state.console_lines = st.session_state.console_lines[-20:]

# Sidebar
st.sidebar.header("🔐 Access")

# Toggle log visibility
show_log = st.sidebar.checkbox("Show processing log", value=True)
log_box = st.sidebar.empty()

def render_console():
    if show_log:
        log_box.code("\n".join(st.session_state.console_lines) or "ready", language="text")

if not ADMIN_PASSWORD:
    st.sidebar.info("Admin password is NOT set → app is view-only.")
else:
    pw = st.sidebar.text_input("Admin password", type="password", placeholder="Enter to enable edit mode")
    colA, colB = st.sidebar.columns(2)
    with colA:
        if st.button("Login"):
            st.session_state.is_admin = check_admin(pw)
            if st.session_state.is_admin:
                st.sidebar.success("Edit mode enabled ✅")
                console_log("auth: admin login OK")
            else:
                st.sidebar.error("Wrong password ❌")
                console_log("auth: admin login FAIL")
    with colB:
        if st.button("Logout"):
            st.session_state.is_admin = False
            st.sidebar.info("Logged out.")
            console_log("auth: logout")

is_admin = st.session_state.is_admin

# ---------- Load ----------
def load_json_files(folder: str):
    out = {}
    for fn in sorted(os.listdir(folder)):
        if fn.lower().endswith(".json"):
            with open(os.path.join(folder, fn), "r", encoding="utf-8") as f:
                out[fn] = json.load(f)
    return out

def save_json_file(folder: str, filename: str, data: dict):
    if not is_admin:
        raise PermissionError("View-only: saving is disabled.")
    path = os.path.join(folder, filename)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)

console_log("boot: app started")

if not os.path.exists(DATA_FOLDER):
    st.error("No `data/` folder found in your repo.")
    console_log("error: data folder missing")
    render_console()
    st.stop()

files_data = load_json_files(DATA_FOLDER)
if not files_data:
    st.warning("No .json files found in `data/`.")
    console_log("warning: no json files")
    render_console()
    st.stop()

selected_file = st.selectbox("Choose a data file", list(files_data.keys()))
raw = files_data[selected_file]
console_log(f"load: selected file = {selected_file}")
console_log(f"data: countries loaded = {len(raw) if isinstance(raw, dict) else 'n/a'}")

with st.expander("🔧 Debug: show raw JSON"):
    st.json(raw)

if not isinstance(raw, dict):
    st.error("This app expects a JSON dict like { 'USA': { ... }, 'EU': { ... } }")
    console_log("error: invalid json structure")
    render_console()
    st.stop()

working = deepcopy(raw)

# =========================
# EDIT MODE UI (admins only)
# =========================
st.sidebar.markdown("---")
if is_admin:
    st.sidebar.success("🛠 Edit mode: ON")
    st.subheader("🛠 Edit indicators (admin only)")

    countries = list(working.keys())
    country = st.selectbox("Country to edit", countries)

    metrics = list(working[country].keys())
    metric = st.selectbox("Metric to edit", metrics)

    current_val = working[country].get(metric, None)
    new_val = st.number_input(
        f"New value for {country} → {metric}",
        value=float(current_val) if current_val is not None else 0.0,
        step=0.1
    )

    col1, col2 = st.columns(2)
    with col1:
        if st.button("Apply change (not saved yet)"):
            working[country][metric] = float(new_val)
            st.success("Change applied ✅ (press Save to write to file)")
            console_log(f"edit: {country}.{metric}={float(new_val)} (pending)")
    with col2:
        if st.button("Save to JSON file"):
            try:
                console_log("save: attempting write...")
                save_json_file(DATA_FOLDER, selected_file, working)
                console_log("save: OK")
                st.success(f"Saved ✅ → {selected_file}")
                st.rerun()
            except Exception as e:
                st.error(f"Save failed: {e}")
                console_log(f"save: FAIL ({e})")
else:
    st.sidebar.info("👀 View-only mode: ON")
    console_log("mode: view-only")

# Convert to DataFrame
df = pd.DataFrame.from_dict(working, orient="index")
df.index.name = "Country"
df = df.reset_index()

# Ensure numeric columns
for c in df.columns:
    if c != "Country":
        df[c] = pd.to_numeric(df[c], errors="coerce")

# ---------- Scoring (CONFIG-DRIVEN, MAX-NORM) ----------
st.sidebar.header("🧮 Influence score weights (from config.json)")

# Load config
try:
    with open("config.json", "r", encoding="utf-8") as f:
        cfg = json.load(f)
    cfg_metrics = cfg.get("metrics", [])
    scoring_cfg = cfg.get("scoring", {})
    console_log(f"config: loaded {len(cfg_metrics)} metrics")
except Exception as e:
    st.error(f"Could not load config.json: {e}")
    console_log(f"error: config load failed ({e})")
    render_console()
    st.stop()

# Only support modes used in your new system
SUPPORTED_MODES = {"max_norm", "threshold"}

# Build metric config list (filter to keys that exist in df)
metric_rows = []
for m in cfg_metrics:
    key = m.get("key")
    if not key or key not in df.columns:
        continue
    mode = m.get("mode", "max_norm")
    if mode not in SUPPORTED_MODES:
        continue
    metric_rows.append(m)

if not metric_rows:
    st.warning("No metrics from config.json match the columns in your data file.")
    console_log("warning: no matching metrics in config")
    render_console()
    st.stop()

# Let user adjust weights (default to config weights)
weights = {}
for m in metric_rows:
    key = m["key"]
    default_w = float(m.get("weight", 1.0))
    weights[key] = st.sidebar.slider(
        key,
        min_value=0.0,
        max_value=3.0,
        value=float(default_w),
        step=0.1,
        help=m.get("name", key),
    )

st.sidebar.markdown("---")
scale_to_100 = st.sidebar.checkbox("Scale score to 0–100", value=True)

clamp_0_1 = bool(scoring_cfg.get("clamp_0_1", True))

def clamp01(x: float) -> float:
    return max(0.0, min(1.0, x))

def max_norm(series: pd.Series) -> pd.Series:
    s = series.astype(float)
    mx = s.max(skipna=True)
    if pd.isna(mx) or mx == 0:
        return pd.Series([0.0] * len(s), index=s.index)
    return s / mx

def threshold_apply(series: pd.Series, thresholds: list) -> pd.Series:
    # thresholds like [{"max": 0, "points": 0}, {"max": 1, "points": 4}]
    # returns points (NOT normalized) so we will later divide by max_points to put into 0..1
    def pts(v):
        if pd.isna(v):
            return 0.0
        for t in thresholds:
            if float(v) <= float(t["max"]):
                return float(t["points"])
        return float(thresholds[-1]["points"]) if thresholds else 0.0
    return series.apply(pts)

console_log("compute: max-normalizing metrics from config")
norm = pd.DataFrame(index=df.index)

for m in metric_rows:
    key = m["key"]
    mode = m.get("mode", "max_norm")
    direction = m.get("direction", "higher_better")
    max_points = float(m.get("max_points", 1.0))  # used for max_norm; also to normalize threshold to 0..1

    if mode == "max_norm":
        n = max_norm(df[key])
        if direction == "lower_better":
            n = 1.0 - n
        if clamp_0_1:
            n = n.apply(lambda x: clamp01(float(x)) if pd.notna(x) else 0.0)
        norm[key] = n

    elif mode == "threshold":
        thresholds = m.get("thresholds", [])
        pts = threshold_apply(df[key], thresholds)

        # Convert threshold points -> normalized 0..1 using max_points if provided
        # If no max_points in config, fall back to max points found in thresholds.
        if "max_points" in m:
            denom = float(m["max_points"]) if float(m["max_points"]) > 0 else 1.0
        else:
            denom = max([float(t.get("points", 0)) for t in thresholds], default=1.0)
            denom = denom if denom > 0 else 1.0

        n = pts / denom
        if clamp_0_1:
            n = n.apply(lambda x: clamp01(float(x)) if pd.notna(x) else 0.0)
        norm[key] = n

console_log("compute: scoring (weighted mean)")
score = pd.Series(0.0, index=df.index)
total_w = 0.0

for key, w in weights.items():
    if key in norm.columns and w > 0:
        score += norm[key] * w
        total_w += w

if total_w == 0:
    df["Score"] = None
else:
    df["Score"] = score / total_w
    if scale_to_100:
        df["Score"] = df["Score"] * 100

# ---------- Output ----------
col1, col2, col3 = st.columns(3)
col1.metric("Countries", len(df_rank))
col2.metric("Top country", df_rank.loc[0, "Country"])
col3.metric("Top score", float(df_rank.loc[0, "Score"]) if pd.notna(df_rank.loc[0, "Score"]) else 0.0)

st.divider()

# =========================
# WORLD MAP
# =========================
st.subheader("🗺️ World map (Influence Score)")
console_log("viz: building world map")

iso3 = {
    "USA": "USA",
    "China": "CHN",
    "India": "IND",
    "Brazil": "BRA",
    "Russia": "RUS",
    "UAE": "ARE",
}

map_rows = []
eu_row = None
for _, row in df_rank.iterrows():
    c = row["Country"]
    s = row["Score"]
    if c == "EU":
        eu_row = row
        continue
    if c in iso3 and pd.notna(s):
        map_rows.append({"Country": c, "ISO3": iso3[c], "Score": float(s)})

map_df = pd.DataFrame(map_rows)

map_col, side_col = st.columns([2, 1], vertical_alignment="top")

with side_col:
    st.markdown("### ℹ️ Notes")
    st.write("- EU is shown as a bloc (not a single country on the map).")
    if eu_row is not None and pd.notna(eu_row["Score"]):
        st.metric("EU Score (bloc)", f"{float(eu_row['Score']):.1f}")
    else:
        st.caption("EU score not available.")

with map_col:
    if not PLOTLY_OK:
        st.warning("Plotly is not installed. Add `plotly` to requirements.txt to enable the world map.")
    elif map_df.empty:
        st.info("No mappable countries with scores found.")
    else:
        fig = px.choropleth(
            map_df,
            locations="ISO3",
            color="Score",
            hover_name="Country",
            projection="natural earth",
        )
        fig.update_layout(margin=dict(l=0, r=0, t=0, b=0))
        st.plotly_chart(fig, use_container_width=True)

st.divider()

st.subheader("🏆 Ranking")
st.dataframe(df_rank, use_container_width=True, hide_index=True)

st.subheader("📊 Score chart")
chart = df_rank.dropna(subset=["Score"]).set_index("Country")["Score"]
st.bar_chart(chart)

st.divider()

st.subheader("📋 Indicators (raw data)")
st.dataframe(df.set_index("Country"), use_container_width=True)

console_log("done: render complete")
render_console()
