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
# Streamlit Community Cloud -> password goes in Secrets:
# ADMIN_PASSWORD="your_password"
ADMIN_PASSWORD = st.secrets.get("ADMIN_PASSWORD", "")

def check_admin(pw: str) -> bool:
    return bool(ADMIN_PASSWORD) and pw == ADMIN_PASSWORD

if "is_admin" not in st.session_state:
    st.session_state.is_admin = False

# =========================
# Dramatic console (fake logs)
# =========================
if "console_lines" not in st.session_state:
    st.session_state.console_lines = []

def console_log(msg: str):
    ts = datetime.now().strftime("%H:%M:%S")
    st.session_state.console_lines.append(f"[{ts}] {msg}")
    # keep it short
    st.session_state.console_lines = st.session_state.console_lines[-14:]

def render_console():
    with st.sidebar.expander("📟 Live console (dramatic)", expanded=True):
        st.code("\n".join(st.session_state.console_lines) or "idle...", language="bash")

st.sidebar.header("🔐 Access")

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
                console_log("admin auth: OK")
            else:
                st.sidebar.error("Wrong password ❌")
                console_log("admin auth: FAIL")
    with colB:
        if st.button("Logout"):
            st.session_state.is_admin = False
            st.sidebar.info("Logged out.")
            console_log("admin auth: logout")

is_admin = st.session_state.is_admin

render_console()

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

console_log("boot: starting app")

if not os.path.exists(DATA_FOLDER):
    st.error("No `data/` folder found in your repo.")
    console_log("error: data folder missing")
    st.stop()

files_data = load_json_files(DATA_FOLDER)
if not files_data:
    st.warning("No .json files found in `data/`.")
    console_log("warning: no json files")
    st.stop()

selected_file = st.selectbox("Choose a data file", list(files_data.keys()))
raw = files_data[selected_file]
console_log(f"load: {selected_file}")

with st.expander("🔧 Debug: show raw JSON"):
    st.json(raw)

if not isinstance(raw, dict):
    st.error("This app expects a JSON dict like { 'USA': { ... }, 'EU': { ... } }")
    console_log("error: invalid json structure")
    st.stop()

# working copy (safe)
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
                st.success(f"Saved ✅ → {selected_file}")
                console_log("save: OK")
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

# ---------- Scoring ----------
GOOD_WHEN_HIGHER = {
    "gdp_growth_pct",
    "defense_spend_usd_bn",
    "overseas_bases_count",
    "energy_net_exporter",
    "oil_production_mbd",
    "rd_gdp_pct",
}
BAD_WHEN_HIGHER = {
    "inflation_pct",
    "semiconductor_import_dependence",
    "sanctions_intensity_index",
}

available_good = [m for m in GOOD_WHEN_HIGHER if m in df.columns]
available_bad = [m for m in BAD_WHEN_HIGHER if m in df.columns]
all_metrics = available_good + available_bad

st.sidebar.header("🧮 Influence score weights")
default_weights = {
    "gdp_growth_pct": 1.0,
    "defense_spend_usd_bn": 2.0,
    "overseas_bases_count": 2.0,
    "energy_net_exporter": 1.0,
    "oil_production_mbd": 1.0,
    "rd_gdp_pct": 1.5,
    "inflation_pct": 1.0,
    "semiconductor_import_dependence": 1.5,
    "sanctions_intensity_index": 2.0,
}

weights = {}
for m in all_metrics:
    weights[m] = st.sidebar.slider(
        m,
        min_value=0.0,
        max_value=3.0,
        value=float(default_weights.get(m, 1.0)),
        step=0.1,
    )

st.sidebar.markdown("---")
scale_to_100 = st.sidebar.checkbox("Scale score to 0–100", value=True)

def minmax_norm(series: pd.Series) -> pd.Series:
    s = series.astype(float)
    mn, mx = s.min(skipna=True), s.max(skipna=True)
    if pd.isna(mn) or pd.isna(mx) or mn == mx:
        return pd.Series([0.5] * len(s), index=s.index)
    return (s - mn) / (mx - mn)

console_log("compute: normalizing metrics")
norm = pd.DataFrame()
norm["Country"] = df["Country"]

for m in available_good:
    norm[m] = minmax_norm(df[m])

for m in available_bad:
    norm[m] = 1.0 - minmax_norm(df[m])

console_log("compute: scoring")
score = pd.Series(0.0, index=df.index)
total_w = 0.0
for m, w in weights.items():
    if m in norm.columns and w > 0:
        score += norm[m] * w
        total_w += w

if total_w == 0:
    df["Score"] = None
else:
    df["Score"] = score / total_w
    if scale_to_100:
        df["Score"] = df["Score"] * 100

# Ranking
df_rank = df[["Country", "Score"]].copy()
df_rank = df_rank.sort_values("Score", ascending=False, na_position="last").reset_index(drop=True)
df_rank.insert(0, "Rank", range(1, len(df_rank) + 1))

# ---------- Output ----------
col1, col2, col3 = st.columns(3)
col1.metric("Countries", len(df_rank))
col2.metric("Top country", df_rank.loc[0, "Country"])
col3.metric("Top score", float(df_rank.loc[0, "Score"]) if pd.notna(df_rank.loc[0, "Score"]) else 0.0)

st.divider()

# =========================
# WORLD MAP (NEW)
# =========================
st.subheader("🗺️ World map (Influence Score)")
console_log("viz: building world map")

# Plotly needs ISO-3 codes for countries; EU isn't a country code for the map.
iso3 = {
    "USA": "USA",
    "China": "CHN",
    "India": "IND",
    "Brazil": "BRA",
    "Russia": "RUS",
    "UAE": "ARE",
    # "EU": (not plotted; shown separately)
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

    st.markdown("### 🎬 Dramatic mode")
    st.caption("This console is fake on purpose — just for effect 😄")

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
            color_continuous_scale="Viridis",
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
