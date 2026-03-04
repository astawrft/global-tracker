import streamlit as st
import json
import os
import pandas as pd

st.set_page_config(page_title="Global Influence Tracker", page_icon="🌍", layout="wide")
st.title("🌍 Global Influence Tracker")
st.caption("Influence Score is computed from your indicators (higher = more influence).")

DATA_FOLDER = "data"

# ---------- Load ----------
def load_json_files(folder: str):
    out = {}
    for fn in sorted(os.listdir(folder)):
        if fn.lower().endswith(".json"):
            with open(os.path.join(folder, fn), "r", encoding="utf-8") as f:
                out[fn] = json.load(f)
    return out

if not os.path.exists(DATA_FOLDER):
    st.error("No `data/` folder found in your repo.")
    st.stop()

files_data = load_json_files(DATA_FOLDER)
if not files_data:
    st.warning("No .json files found in `data/`.")
    st.stop()

selected_file = st.selectbox("Choose a data file", list(files_data.keys()))
raw = files_data[selected_file]

with st.expander("🔧 Debug: show raw JSON"):
    st.json(raw)

if not isinstance(raw, dict):
    st.error("This app expects a JSON dict like { 'USA': { ...metrics... }, 'EU': { ... } }")
    st.stop()

# Convert to DataFrame (rows=countries, cols=metrics)
df = pd.DataFrame.from_dict(raw, orient="index")
df.index.name = "Country"
df = df.reset_index()

# Ensure numeric columns
for c in df.columns:
    if c != "Country":
        df[c] = pd.to_numeric(df[c], errors="coerce")

# ---------- Scoring ----------
# We normalize each metric to 0..1, then combine using weights.
# For "good when higher": normalize normally.
# For "bad when higher": invert (1 - normalized).

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

# Default weights (you can change these)
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
        return pd.Series([0.5] * len(s), index=s.index)  # neutral if no spread
    return (s - mn) / (mx - mn)

# Build normalized columns
norm = pd.DataFrame()
norm["Country"] = df["Country"]

for m in available_good:
    norm[m] = minmax_norm(df[m])

for m in available_bad:
    norm[m] = 1.0 - minmax_norm(df[m])  # invert

# Weighted sum
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

st.subheader("🏆 Ranking")
st.dataframe(df_rank, use_container_width=True, hide_index=True)

st.subheader("📊 Score chart")
chart = df_rank.dropna(subset=["Score"]).set_index("Country")["Score"]
st.bar_chart(chart)

st.divider()

st.subheader("📋 Indicators (raw data)")
st.dataframe(df.set_index("Country"), use_container_width=True)
