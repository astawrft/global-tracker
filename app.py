import streamlit as st
import json
import os

st.title("Global Conflict Tracker")

data_folder = "data"

files = os.listdir(data_folder)

for file in files:
    with open(os.path.join(data_folder, file)) as f:
        data = json.load(f)

    st.subheader(file)

    for event in data:
        st.write(event)
