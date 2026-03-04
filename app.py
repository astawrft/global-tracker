import streamlit as st
import json
import os
import pandas as pd

st.set_page_config(page_title="Global Conflict Tracker", page_icon="🌍", layout="wide")
st.title("🌍 Global Conflict Tracker")
st.caption("Higher score = more influence")

DATA_FOLDER = "data"

# ---------- helpers ----------
def pick_score(value):
    """
    Tries to extract a numeric score from:
    - number
    - string number
    - dict that contains a score-like field
    """
    if value is None:
        return None

    # direct number
    if isinstance(value, (int, float)):
        return float(value)

    # numeric string like "8.2"
    if isinstance(value, str):
        try:
            return float(value.strip())
        except Exception:
            return None

    # dict with a score field
    if isinstance(value, dict):
        # common field names (you can add more)
        for k in ["score", "influence", "power", "rank_score", "value", "total", "index"]:
            if k in value:
                return pick_score(value[k])

        # if dict is like {"USA": 9} (nested weirdness), try any numeric inside
        for v in value.values():
            s = pick_score(v)
            if s is not None:
                return s

    return None


def normalize_to_table(data):
    """
    Returns a DataFrame with columns: Country, Score
    Supports:
    - dict: {"USA": 9, "EU": 8}
    - dict: {"USA": {"influence": 9}, ...}
    - list: [{"country":"USA","score":9}, ...]
    - list: ["USA","EU"] (no scores -> None)
    """
    rows = []

    # dict mapping
    if isinstance(data, dict):
        for country, val in data.items():
            rows.append({"Country": str(country), "Score": pick_score(val)})

    # list formats
    elif isinstance(data, list):
        for item in data:
            if isinstance(item, str):
                rows.append({"Country": item, "Score": None})
            elif isinstance(item, dict):
                # guess country field
                country = (
                    item.get("country")
                    or item.get("name")
                    or item.get("actor")
                    or item.get("state")
                    or item.get("nation")
                )
                # score can be in many places
                score = pick_score(item.get("score")) or pick_score(item.get("influence")) or pick_score(item)
                rows.append({"Country": str(country) if country is not None else "Unknown", "Score": score})

    return pd.DataFrame(rows)


def load_json_files(folder: str):
    out = {}
    for fn in sorted(os.listdir(folder)):
        if fn.lower().endswith(".json"):
            with open(os.path.join(folder, fn), "r", encoding="utf-8") as f:
                out[fn] = json.load(f)
    return out


# ---------- load ----------
if not os.path.exists(DATA_FOLDER):
    st.error("No `data/` folder found in your repo.")
    st.stop()

files_data = load_json_files(DATA_FOLDER)
if not files_data:
    st.warning("No .json files found in `data/`.")
    st.stop()

selected_file = st.selectbox("Choose a data file", list(files_data.keys()))
data = files_data[selected_file]

# Debug toggle (super useful)
with st.expander("🔧 Debug: show raw JSON"):
    st.json(data)

df = normalize_to_table(data)

# If still empty, show raw
if df.empty:
    st.warning("Could not convert this JSON to a ranking table. See raw JSON in Debug.")
    st.stop()

# Sort ranking (highest score first; None last)
df["Score"] = pd.to_numeric(df["Score"], errors="coerce")
df = df.sort_values("Score", ascending=False, na_position="last").reset_index(drop=True)
df.insert(0, "Rank", range(1, len(df) + 1))

# Metrics
col1, col2, col3 = st.columns(3)
col1.metric("Countries", len(df))

if df["Score"].notna().any():
    top = df.dropna(subset=["Score"]).iloc[0]
    col2.metric("Top country", str(top["Country"]))
    col3.metric("Top score", float(top["Score"]))
else:
    col2.metric("Top country", "—")
    col3.metric("Top score", "—")

st.divider()

st.subheader("🏆 Ranking")
st.dataframe(df, use_container_width=True, hide_index=True)

st.subheader("📊 Score chart")
chart_df = df.dropna(subset=["Score"]).set_index("Country")["Score"]
if len(chart_df) == 0:
    st.info("No numeric scores found in the JSON yet. Check the Debug view above to see your structure.")
else:
    st.bar_chart(chart_df)
