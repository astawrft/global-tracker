import streamlit as st
import json
import os
import pandas as pd

st.set_page_config(page_title="Global Conflict Tracker", page_icon="🌍", layout="wide")
st.title("🌍 Global Conflict Tracker")

DATA_FOLDER = "data"

def load_json_files(folder: str):
    all_data = {}
    for fn in sorted(os.listdir(folder)):
        if fn.lower().endswith(".json"):
            path = os.path.join(folder, fn)
            with open(path, "r", encoding="utf-8") as f:
                all_data[fn] = json.load(f)
    return all_data

if not os.path.exists(DATA_FOLDER):
    st.error("No `data/` folder found. Make sure your repo has a folder called `data`.")
    st.stop()

files_data = load_json_files(DATA_FOLDER)
if not files_data:
    st.warning("No .json files found in `data/`.")
    st.stop()

# Pick file
selected_file = st.selectbox("Choose a data file", list(files_data.keys()))
data = files_data[selected_file]

st.subheader(selected_file)

# --- Case 1: data is a dict like {"USA": 7, "China": 9}
if isinstance(data, dict):
    # Convert to table
    df = pd.DataFrame(list(data.items()), columns=["Country", "Score"])

    # Try to make Score numeric if possible
    df["Score"] = pd.to_numeric(df["Score"], errors="coerce")

    # Ranking: highest score = rank 1
    df = df.sort_values("Score", ascending=False, na_position="last").reset_index(drop=True)
    df.insert(0, "Rank", range(1, len(df) + 1))

    # Nice top metrics
    col1, col2, col3 = st.columns(3)
    col1.metric("Countries", len(df))
    if df["Score"].notna().any():
        col2.metric("Top country", str(df.loc[0, "Country"]))
        col3.metric("Top score", float(df.loc[0, "Score"]))

    st.divider()

    st.subheader("🏆 Ranking")
    st.dataframe(df, use_container_width=True, hide_index=True)

    st.subheader("📊 Score chart")
    chart_df = df.dropna(subset=["Score"]).set_index("Country")["Score"]
    st.bar_chart(chart_df)

# --- Case 2: data is a list (events). We show it as-is.
elif isinstance(data, list):
    st.info("This file is a list. Showing raw items (events).")
    st.write(data)

# --- Other formats
else:
    st.warning("Unknown JSON structure. Showing raw data:")
    st.write(data)
