import json
import os
import urllib.request

COUNTRIES = {
    "USA": "USA",
    "EU": "EUU",   # World Bank aggregate for European Union
    "Brazil": "BRA",
    "Russia": "RUS",
    "India": "IND",
    "China": "CHN",
    "UAE": "ARE",
}

# Map your JSON keys -> World Bank indicator codes
WB_INDICATORS = {
    "gdp_nominal_usd_trillion": "NY.GDP.MKTP.CD",
    "gdp_growth_pct": "NY.GDP.MKTP.KD.ZG",
    "inflation_pct": "FP.CPI.TOTL.ZG",
    "population_total_million": "SP.POP.TOTL",
    "urbanization_pct": "SP.URB.TOTL.IN.ZS",
    "trade_volume_usd_trillion": "NE.TRD.GNFS.CD"
}

def wb_latest(country_code: str, indicator: str):
    # Pull latest non-null value
    url = f"https://api.worldbank.org/v2/country/{country_code}/indicator/{indicator}?format=json&per_page=200"
    with urllib.request.urlopen(url) as r:
        data = json.loads(r.read().decode("utf-8"))

    # data[1] is the list of yearly observations
    for row in data[1]:
        if row.get("value") is not None:
            return float(row["value"]), int(row["date"])
    return None, None

def main():
    out = {}

    for name, wb_code in COUNTRIES.items():
        out[name] = {}

        for key, ind in WB_INDICATORS.items():
            val, year = wb_latest(wb_code, ind)
            if val is None:
                continue

            # unit conversions for your schema
            if key == "gdp_nominal_usd_trillion":
                out[name][key] = round(val / 1e12, 3)
            elif key == "population_total_million":
                out[name][key] = round(val / 1e6, 3)
            elif key == "trade_volume_usd_trillion":
                out[name][key] = round(val / 1e12, 3)
            else:
                out[name][key] = round(val, 3)

        # store metadata
        out[name]["_source"] = "World Bank"
        out[name]["_last_updated_year_guess"] = max(
            [out[name].get("_last_updated_year_guess", 0)] +
            [0]  # keep simple
        )

    os.makedirs("data", exist_ok=True)
    with open("data/latest.json", "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2, ensure_ascii=False)

if __name__ == "__main__":
    main()
