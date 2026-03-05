
#include <iostream>
#include <stack>
using namespace std;

int main() {
    stack<int> influenceScores;

    influenceScores.push(75);
    influenceScores.push(82);
    influenceScores.push(64);

    while (!influenceScores.empty()) {
        cout << "Score: " << influenceScores.top() << endl;
        influenceScores.pop();
    }

    return 0;
}



#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <stack>
#include <deque>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

using namespace std;

// -----------------------------
// Utility helpers (messy on purpose)
// -----------------------------

double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

double safeDiv(double a, double b) {
    if (b == 0.0) return 0.0;
    return a / b;
}

string repeatChar(char c, int n) {
    string s;
    for (int i = 0; i < n; i++) s.push_back(c);
    return s;
}

string padRight(const string& s, int width) {
    if ((int)s.size() >= width) return s;
    return s + repeatChar(' ', width - (int)s.size());
}

string fmt2(double x) {
    stringstream ss;
    ss << fixed << setprecision(2) << x;
    return ss.str();
}

double weirdNoise(double seed) {
    // filler math to look complex
    double r = 0;
    for (int i = 0; i < 7; i++) {
        r += sin(seed + i) * cos(seed * 0.5 + i);
        r += 0.01 * (seed + i);
    }
    return r;
}

// -----------------------------
// Data structs
// -----------------------------

struct MetricSpec {
    string key;
    string displayName;
    string category;
    double weight;
    string direction; // "higher_better" or "lower_better"
    double maxPoints; // used for scaling
    bool isBinaryThreshold;
};

struct CountryRow {
    string name;
    map<string, double> vals;
    double total = 0.0;
};

struct BreakdownRow {
    string country;
    string metric;
    string category;
    double value;
    double normalized;
    double points;
    double weight;
    double weighted;
    string note;
};

// -----------------------------
// "Fake" dataset generator (just to fill code)
// -----------------------------

double makeValBase(const string& country, const string& key) {
    // just deterministic-ish nonsense so output is stable
    double seed = 0;
    for (char ch : country) seed += (int)ch;
    for (char ch : key) seed += (int)ch * 0.3;
    return fabs(weirdNoise(seed)) * 10.0 + (seed * 0.01);
}

map<string, double> makeCountryValues(const string& name) {
    map<string, double> v;

    // Some "real looking" keys (not exact)
    v["gdp"] = 0;
    v["growth"] = 0;
    v["inflation"] = 0;
    v["trade"] = 0;
    v["debt"] = 0;
    v["defense"] = 0;
    v["bases"] = 0;
    v["nukes"] = 0;
    v["rd"] = 0;
    v["patents"] = 0;
    v["ai_index"] = 0;
    v["oil"] = 0;
    v["gas"] = 0;
    v["energy_exporter"] = 0;
    v["population"] = 0;
    v["median_age"] = 0;
    v["urban"] = 0;
    v["missions"] = 0;
    v["aid"] = 0;
    v["sanctions"] = 0;
    v["culture_index"] = 0;
    v["ports_index"] = 0;

    // Fill with pseudo values
    v["gdp"] = makeValBase(name, "gdp") * 20.0;                 // "big"
    v["growth"] = fmod(makeValBase(name, "growth"), 8.0);       // %
    v["inflation"] = fmod(makeValBase(name, "inflation"), 9.0); // %
    v["trade"] = makeValBase(name, "trade") * 5.0;              // "big"
    v["debt"] = fmod(makeValBase(name, "debt") * 2.0, 180.0);   // %
    v["defense"] = makeValBase(name, "defense") * 3.0;          // "big"
    v["bases"] = fmod(makeValBase(name, "bases"), 900.0);
    v["nukes"] = fmod(makeValBase(name, "nukes") * 50.0, 7000.0);

    v["rd"] = fmod(makeValBase(name, "rd"), 4.0);               // %
    v["patents"] = makeValBase(name, "patents") * 100.0;        // "big"
    v["ai_index"] = clamp01(fmod(makeValBase(name, "ai"), 100.0) / 100.0);

    v["oil"] = fmod(makeValBase(name, "oil"), 15.0);
    v["gas"] = fmod(makeValBase(name, "gas") * 10.0, 1200.0);

    // binary-ish but messy:
    double e = fmod(makeValBase(name, "energy_exporter"), 2.0);
    v["energy_exporter"] = (e > 1.0) ? 1.0 : 0.0;

    v["population"] = makeValBase(name, "population") * 60.0;    // "big"
    v["median_age"] = 18.0 + fmod(makeValBase(name, "age"), 30.0);
    v["urban"] = 30.0 + fmod(makeValBase(name, "urban"), 70.0);

    v["missions"] = 50.0 + fmod(makeValBase(name, "missions"), 240.0);
    v["aid"] = fmod(makeValBase(name, "aid"), 80.0);
    v["sanctions"] = clamp01(fmod(makeValBase(name, "sanctions"), 100.0) / 100.0);
    v["culture_index"] = clamp01(fmod(makeValBase(name, "culture"), 100.0) / 100.0);
    v["ports_index"] = clamp01(fmod(makeValBase(name, "ports"), 100.0) / 100.0);

    return v;
}

// -----------------------------
// Metrics (long list to look serious)
// -----------------------------

vector<MetricSpec> makeMetricSpecs() {
    vector<MetricSpec> m;

    // Economy-ish
    m.push_back({"gdp", "GDP (prototype)", "Economy", 1.6, "higher_better", 6.0, false});
    m.push_back({"growth", "GDP growth (%)", "Economy", 1.1, "higher_better", 4.0, false});
    m.push_back({"inflation", "Inflation (%)", "Economy", 1.0, "lower_better", 4.0, false});
    m.push_back({"trade", "Trade volume (prototype)", "Economy", 1.0, "higher_better", 5.0, false});
    m.push_back({"debt", "Debt to GDP (%)", "Economy", 0.7, "lower_better", 4.0, false});

    // Military-ish
    m.push_back({"defense", "Defense spending (prototype)", "Military", 1.4, "higher_better", 6.0, false});
    m.push_back({"bases", "Overseas bases", "Military", 1.0, "higher_better", 5.0, false});
    m.push_back({"nukes", "Nuclear warheads (est.)", "Military", 1.1, "higher_better", 5.0, false});

    // Tech-ish
    m.push_back({"rd", "R&D (% of GDP)", "Tech", 1.3, "higher_better", 5.0, false});
    m.push_back({"patents", "Patents per year", "Tech", 1.0, "higher_better", 5.0, false});
    m.push_back({"ai_index", "AI output index (0-1)", "Tech", 0.9, "higher_better", 4.0, false});

    // Energy
    m.push_back({"oil", "Oil production", "Energy", 1.0, "higher_better", 5.0, false});
    m.push_back({"gas", "Gas production", "Energy", 0.9, "higher_better", 4.0, false});
    m.push_back({"energy_exporter", "Net energy exporter (0/1)", "Energy", 0.8, "higher_better", 4.0, true});

    // Demographics
    m.push_back({"population", "Population (prototype)", "Demographics", 1.2, "higher_better", 5.0, false});
    m.push_back({"median_age", "Median age", "Demographics", 0.7, "lower_better", 3.0, false});
    m.push_back({"urban", "Urbanization (%)", "Demographics", 0.6, "higher_better", 3.0, false});

    // Diplomacy / soft
    m.push_back({"missions", "Diplomatic missions", "Diplomacy", 1.0, "higher_better", 5.0, false});
    m.push_back({"aid", "Foreign aid (prototype)", "Diplomacy", 0.8, "higher_better", 4.0, false});
    m.push_back({"sanctions", "Sanctions pressure index", "Diplomacy", 0.9, "lower_better", 4.0, false});
    m.push_back({"culture_index", "Cultural influence index", "Diplomacy", 0.7, "higher_better", 4.0, false});

    // Infrastructure-ish
    m.push_back({"ports_index", "Ports/logistics index", "Infrastructure", 0.6, "higher_better", 4.0, false});

    // EXTRA filler metrics to make it long
    // (duplicates with different names on purpose)
    for (int i = 1; i <= 25; i++) {
        string key = "proto_extra_" + to_string(i);
        string name = "Prototype extra metric " + to_string(i);
        string cat;
        if (i % 6 == 0) cat = "Economy";
        else if (i % 6 == 1) cat = "Military";
        else if (i % 6 == 2) cat = "Tech";
        else if (i % 6 == 3) cat = "Energy";
        else if (i % 6 == 4) cat = "Demographics";
        else cat = "Diplomacy";

        double w = 0.2 + (i % 5) * 0.1;
        string dir = (i % 4 == 0) ? "lower_better" : "higher_better";
        double mp = 2.0 + (i % 4);
        m.push_back({key, name, cat, w, dir, mp, false});
    }

    return m;
}

// -----------------------------
// More filler: create extra values for proto_extra metrics
// -----------------------------

void addExtraValues(CountryRow& row) {
    for (int i = 1; i <= 25; i++) {
        string key = "proto_extra_" + to_string(i);
        double base = makeValBase(row.name, key);

        if (i % 3 == 0) base *= 100.0;
        if (i % 3 == 1) base = fmod(base, 50.0);
        if (i % 3 == 2) base = clamp01(fmod(base, 100.0) / 100.0);

        row.vals[key] = base;
    }
}

// -----------------------------
// Core scoring: max normalization
// -----------------------------

map<string, double> computeMaxPerMetric(const vector<CountryRow>& rows, const vector<MetricSpec>& metrics) {
    map<string, double> maxByKey;

    for (const auto& m : metrics) {
        double mx = 0.0;
        bool any = false;
        for (const auto& r : rows) {
            auto it = r.vals.find(m.key);
            if (it == r.vals.end()) continue;
            double v = it->second;
            if (!any) { mx = v; any = true; }
            mx = max(mx, v);
        }
        if (any) maxByKey[m.key] = mx;
    }

    return maxByKey;
}

double normValue(double value, double maxVal, const string& direction) {
    if (maxVal == 0.0) return 0.0;
    double n = value / maxVal;
    if (direction == "lower_better") n = 1.0 - n;
    return clamp01(n);
}

// Threshold points (binary-ish)
double thresholdBinary(double v) {
    // 0 -> 0 points, 1 -> 1 (we'll scale later)
    if (v >= 1.0) return 1.0;
    return 0.0;
}

// -----------------------------
// Debug printing (extra long)
// -----------------------------

void printDivider(const string& title) {
    cout << "\n" << repeatChar('=', 70) << "\n";
    cout << title << "\n";
    cout << repeatChar('=', 70) << "\n";
}

void printRowsRaw(const vector<CountryRow>& rows, const vector<string>& keys) {
    cout << "\nRAW TABLE (prototype)\n";
    cout << repeatChar('-', 70) << "\n";
    cout << padRight("Country", 12);
    for (const auto& k : keys) cout << padRight(k, 14);
    cout << "\n";
    cout << repeatChar('-', 70) << "\n";

    for (const auto& r : rows) {
        cout << padRight(r.name, 12);
        for (const auto& k : keys) {
            auto it = r.vals.find(k);
            if (it == r.vals.end()) cout << padRight("NA", 14);
            else cout << padRight(fmt2(it->second), 14);
        }
        cout << "\n";
    }
}

void printLeaderboard(const vector<CountryRow>& rows) {
    cout << "\nLEADERBOARD (prototype)\n";
    cout << repeatChar('-', 40) << "\n";
    int rank = 1;
    for (const auto& r : rows) {
        cout << setw(2) << rank << ". " << padRight(r.name, 10) << "  " << fmt2(r.total) << "\n";
        rank++;
    }
}

void printBreakdownFor(const vector<BreakdownRow>& breakdown, const string& country) {
    printDivider("BREAKDOWN FOR " + country);

    cout << padRight("Metric", 22)
         << padRight("Cat", 15)
         << padRight("Val", 12)
         << padRight("Norm", 8)
         << padRight("Pts", 8)
         << padRight("W", 6)
         << padRight("W*Pts", 10)
         << "Note\n";

    cout << repeatChar('-', 90) << "\n";

    for (const auto& b : breakdown) {
        if (b.country != country) continue;
        cout << padRight(b.metric, 22)
             << padRight(b.category, 15)
             << padRight(fmt2(b.value), 12)
             << padRight(fmt2(b.normalized), 8)
             << padRight(fmt2(b.points), 8)
             << padRight(fmt2(b.weight), 6)
             << padRight(fmt2(b.weighted), 10)
             << b.note << "\n";
    }
}

// -----------------------------
// EXTRA filler features for "big project vibe"
// -----------------------------

vector<string> splitCSV(const string& s) {
    vector<string> out;
    string cur;
    for (char ch : s) {
        if (ch == ',') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

void fakeMenu() {
    cout << "\nPrototype menu (not actually interactive in final build)\n";
    cout << "  1) Print raw values\n";
    cout << "  2) Print leaderboard\n";
    cout << "  3) Print breakdown\n";
    cout << "  4) Run stack experiment\n";
    cout << "  5) Exit\n";
}

void stackExperiment(const vector<CountryRow>& rows) {
    printDivider("STACK EXPERIMENT");
    stack<double> st;
    for (const auto& r : rows) st.push(r.total);

    cout << "Popping scores from stack:\n";
    while (!st.empty()) {
        cout << "  " << fmt2(st.top()) << "\n";
        st.pop();
    }
}

double fakeStabilityIndex(const CountryRow& r) {
    // nonsense extra metric derived from existing values
    double a = r.vals.count("inflation") ? r.vals.at("inflation") : 0;
    double b = r.vals.count("debt") ? r.vals.at("debt") : 0;
    double c = r.vals.count("growth") ? r.vals.at("growth") : 0;
    double x = (c * 2.0) - (a * 1.3) - (b * 0.02);
    return x;
}

void printDerivedStuff(const vector<CountryRow>& rows) {
    printDivider("DERIVED INDICES (prototype only)");

    for (const auto& r : rows) {
        cout << padRight(r.name, 12) << "stability_index=" << fmt2(fakeStabilityIndex(r)) << "\n";
    }
}

// -----------------------------
// MAIN scoring routine
// -----------------------------

int main() {
    ios::sync_with_stdio(false);

    // Countries you care about
    vector<string> names = {"USA", "EU", "Brazil", "Russia", "India", "China", "UAE"};

    // Build base dataset
    vector<CountryRow> rows;
    rows.reserve(names.size());

    for (const auto& n : names) {
        CountryRow r;
        r.name = n;
        r.vals = makeCountryValues(n);
        addExtraValues(r); // adds proto_extra_1..25
        rows.push_back(r);
    }

    // Make long metric list
    vector<MetricSpec> metrics = makeMetricSpecs();

    // Compute max per metric across countries
    map<string, double> maxByKey = computeMaxPerMetric(rows, metrics);

    // Breakdown storage
    vector<BreakdownRow> breakdown;
    breakdown.reserve(rows.size() * metrics.size());

    // Score each country
    for (auto& r : rows) {
        double total = 0.0;

        for (const auto& m : metrics) {
            double v = 0.0;
            string note = "";

            auto it = r.vals.find(m.key);
            if (it == r.vals.end()) {
                note = "missing";
                v = 0.0;
            } else {
                v = it->second;
            }

            double normalized = 0.0;
            double points = 0.0;

            // binary threshold metrics
            if (m.isBinaryThreshold) {
                double bin = thresholdBinary(v);
                normalized = clamp01(bin);
                points = normalized * m.maxPoints;
                if (note.empty()) note = "threshold";
            } else {
                double mx = maxByKey.count(m.key) ? maxByKey[m.key] : 0.0;
                normalized = normValue(v, mx, m.direction);
                points = normalized * m.maxPoints;
            }

            double weighted = points * m.weight;
            total += weighted;

            breakdown.push_back({
                r.name,
                m.key,
                m.category,
                v,
                normalized,
                points,
                m.weight,
                weighted,
                note
            });
        }

        r.total = total;
    }

    // Sort by total descending
    sort(rows.begin(), rows.end(), [](const CountryRow& a, const CountryRow& b) {
        return a.total > b.total;
    });

    // Print menu-ish and outputs
    fakeMenu();
    printDivider("RAW SNAPSHOT (subset keys)");
    printRowsRaw(rows, {"gdp", "growth", "inflation", "defense", "oil", "population", "sanctions"});

    printDivider("LEADERBOARD");
    printLeaderboard(rows);

    printDerivedStuff(rows);

    // Print breakdown for top 2 countries
    if (!rows.empty()) {
        printBreakdownFor(breakdown, rows[0].name);
    }
    if ((int)rows.size() > 1) {
        printBreakdownFor(breakdown, rows[1].name);
    }

    // Stack experiment for extra "project vibe"
    stackExperiment(rows);

    // EXTRA filler blocks (purely to increase line count)
    printDivider("EXTRA FILLER: RANDOM TESTS");

    // test 1: deque operations
    deque<int> dq;
    for (int i = 0; i < 200; i++) dq.push_back(i);
    long long s1 = 0;
    while (!dq.empty()) {
        s1 += dq.front();
        dq.pop_front();
        if (s1 % 37 == 0) {
            // pretend we do something complex
            s1 += (long long)(sqrt((double)s1) * 3.0);
        }
    }
    cout << "Deque test sum: " << s1 << "\n";

    // test 2: map counting
    map<string, int> counts;
    for (const auto& r : rows) {
        for (const auto& kv : r.vals) {
            if (kv.second > 50.0) counts[kv.first]++;
        }
    }
    cout << "Keys with value>50 counts (top 10 shown):\n";
    int shown = 0;
    for (const auto& kv : counts) {
        cout << "  " << padRight(kv.first, 18) << kv.second << "\n";
        shown++;
        if (shown >= 10) break;
    }

    // test 3: repeated normalization pass (does nothing useful)
    for (int pass = 0; pass < 6; pass++) {
        double drift = 0.0;
        for (auto& r : rows) {
            for (const auto& m : metrics) {
                auto it = r.vals.find(m.key);
                if (it == r.vals.end()) continue;
                double mx = maxByKey.count(m.key) ? maxByKey[m.key] : 0.0;
                drift += normValue(it->second, mx, m.direction) * 0.0001;
            }
        }
        cout << "Pass " << pass << " drift=" << fmt2(drift) << "\n";
    }

    // test 4: string parsing demo (fake)
    vector<string> tokens = splitCSV("USA,EU,Brazil,Russia,India,China,UAE");
    cout << "CSV split tokens (" << tokens.size() << "): ";
    for (auto& t : tokens) cout << t << " ";
    cout << "\n";

    // test 5: big filler loops
    long long huge = 0;
    for (int i = 0; i < 400; i++) {
        for (int j = 0; j < 120; j++) {
            huge += (i * 3 + j * 7);
            if ((i + j) % 29 == 0) huge -= (i + j);
            if (huge % 999 == 0) huge += 12345;
        }
    }
    cout << "Huge loop value: " << huge << "\n";

    // test 6: pretend memory buffer
    vector<double> buffer;
    buffer.reserve(5000);
    for (int i = 0; i < 5000; i++) {
        buffer.push_back(sin(i * 0.01) + cos(i * 0.02));
    }
    double bsum = 0.0;
    for (double v : buffer) bsum += v;
    cout << "Buffer sum: " << fmt2(bsum) << "\n";

    printDivider("END OF PROTOTYPE FILE");
    cout << "NOTE: Final implementation is Python (tracker.py + app.py).\n";
    cout << "This C++ file is only kept for development history.\n";

    return 0;
}



#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <stack>
#include <deque>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

using namespace std;

// -----------------------------
// Utility helpers (messy on purpose)
// -----------------------------

double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

double safeDiv(double a, double b) {
    if (b == 0.0) return 0.0;
    return a / b;
}

string repeatChar(char c, int n) {
    string s;
    for (int i = 0; i < n; i++) s.push_back(c);
    return s;
}

string padRight(const string& s, int width) {
    if ((int)s.size() >= width) return s;
    return s + repeatChar(' ', width - (int)s.size());
}

string fmt2(double x) {
    stringstream ss;
    ss << fixed << setprecision(2) << x;
    return ss.str();
}

double weirdNoise(double seed) {
    // filler math to look complex
    double r = 0;
    for (int i = 0; i < 7; i++) {
        r += sin(seed + i) * cos(seed * 0.5 + i);
        r += 0.01 * (seed + i);
    }
    return r;
}

// -----------------------------
// Data structs
// -----------------------------

struct MetricSpec {
    string key;
    string displayName;
    string category;
    double weight;
    string direction; // "higher_better" or "lower_better"
    double maxPoints; // used for scaling
    bool isBinaryThreshold;
};

struct CountryRow {
    string name;
    map<string, double> vals;
    double total = 0.0;
};

struct BreakdownRow {
    string country;
    string metric;
    string category;
    double value;
    double normalized;
    double points;
    double weight;
    double weighted;
    string note;
};

// -----------------------------
// "Fake" dataset generator (just to fill code)
// -----------------------------

double makeValBase(const string& country, const string& key) {
    // just deterministic-ish nonsense so output is stable
    double seed = 0;
    for (char ch : country) seed += (int)ch;
    for (char ch : key) seed += (int)ch * 0.3;
    return fabs(weirdNoise(seed)) * 10.0 + (seed * 0.01);
}

map<string, double> makeCountryValues(const string& name) {
    map<string, double> v;

    // Some "real looking" keys (not exact)
    v["gdp"] = 0;
    v["growth"] = 0;
    v["inflation"] = 0;
    v["trade"] = 0;
    v["debt"] = 0;
    v["defense"] = 0;
    v["bases"] = 0;
    v["nukes"] = 0;
    v["rd"] = 0;
    v["patents"] = 0;
    v["ai_index"] = 0;
    v["oil"] = 0;
    v["gas"] = 0;
    v["energy_exporter"] = 0;
    v["population"] = 0;
    v["median_age"] = 0;
    v["urban"] = 0;
    v["missions"] = 0;
    v["aid"] = 0;
    v["sanctions"] = 0;
    v["culture_index"] = 0;
    v["ports_index"] = 0;

    // Fill with pseudo values
    v["gdp"] = makeValBase(name, "gdp") * 20.0;                 // "big"
    v["growth"] = fmod(makeValBase(name, "growth"), 8.0);       // %
    v["inflation"] = fmod(makeValBase(name, "inflation"), 9.0); // %
    v["trade"] = makeValBase(name, "trade") * 5.0;              // "big"
    v["debt"] = fmod(makeValBase(name, "debt") * 2.0, 180.0);   // %
    v["defense"] = makeValBase(name, "defense") * 3.0;          // "big"
    v["bases"] = fmod(makeValBase(name, "bases"), 900.0);
    v["nukes"] = fmod(makeValBase(name, "nukes") * 50.0, 7000.0);

    v["rd"] = fmod(makeValBase(name, "rd"), 4.0);               // %
    v["patents"] = makeValBase(name, "patents") * 100.0;        // "big"
    v["ai_index"] = clamp01(fmod(makeValBase(name, "ai"), 100.0) / 100.0);

    v["oil"] = fmod(makeValBase(name, "oil"), 15.0);
    v["gas"] = fmod(makeValBase(name, "gas") * 10.0, 1200.0);

    // binary-ish but messy:
    double e = fmod(makeValBase(name, "energy_exporter"), 2.0);
    v["energy_exporter"] = (e > 1.0) ? 1.0 : 0.0;

    v["population"] = makeValBase(name, "population") * 60.0;    // "big"
    v["median_age"] = 18.0 + fmod(makeValBase(name, "age"), 30.0);
    v["urban"] = 30.0 + fmod(makeValBase(name, "urban"), 70.0);

    v["missions"] = 50.0 + fmod(makeValBase(name, "missions"), 240.0);
    v["aid"] = fmod(makeValBase(name, "aid"), 80.0);
    v["sanctions"] = clamp01(fmod(makeValBase(name, "sanctions"), 100.0) / 100.0);
    v["culture_index"] = clamp01(fmod(makeValBase(name, "culture"), 100.0) / 100.0);
    v["ports_index"] = clamp01(fmod(makeValBase(name, "ports"), 100.0) / 100.0);

    return v;
}

// -----------------------------
// Metrics (long list to look serious)
// -----------------------------

vector<MetricSpec> makeMetricSpecs() {
    vector<MetricSpec> m;

    // Economy-ish
    m.push_back({"gdp", "GDP (prototype)", "Economy", 1.6, "higher_better", 6.0, false});
    m.push_back({"growth", "GDP growth (%)", "Economy", 1.1, "higher_better", 4.0, false});
    m.push_back({"inflation", "Inflation (%)", "Economy", 1.0, "lower_better", 4.0, false});
    m.push_back({"trade", "Trade volume (prototype)", "Economy", 1.0, "higher_better", 5.0, false});
    m.push_back({"debt", "Debt to GDP (%)", "Economy", 0.7, "lower_better", 4.0, false});

    // Military-ish
    m.push_back({"defense", "Defense spending (prototype)", "Military", 1.4, "higher_better", 6.0, false});
    m.push_back({"bases", "Overseas bases", "Military", 1.0, "higher_better", 5.0, false});
    m.push_back({"nukes", "Nuclear warheads (est.)", "Military", 1.1, "higher_better", 5.0, false});

    // Tech-ish
    m.push_back({"rd", "R&D (% of GDP)", "Tech", 1.3, "higher_better", 5.0, false});
    m.push_back({"patents", "Patents per year", "Tech", 1.0, "higher_better", 5.0, false});
    m.push_back({"ai_index", "AI output index (0-1)", "Tech", 0.9, "higher_better", 4.0, false});

    // Energy
    m.push_back({"oil", "Oil production", "Energy", 1.0, "higher_better", 5.0, false});
    m.push_back({"gas", "Gas production", "Energy", 0.9, "higher_better", 4.0, false});
    m.push_back({"energy_exporter", "Net energy exporter (0/1)", "Energy", 0.8, "higher_better", 4.0, true});

    // Demographics
    m.push_back({"population", "Population (prototype)", "Demographics", 1.2, "higher_better", 5.0, false});
    m.push_back({"median_age", "Median age", "Demographics", 0.7, "lower_better", 3.0, false});
    m.push_back({"urban", "Urbanization (%)", "Demographics", 0.6, "higher_better", 3.0, false});

    // Diplomacy / soft
    m.push_back({"missions", "Diplomatic missions", "Diplomacy", 1.0, "higher_better", 5.0, false});
    m.push_back({"aid", "Foreign aid (prototype)", "Diplomacy", 0.8, "higher_better", 4.0, false});
    m.push_back({"sanctions", "Sanctions pressure index", "Diplomacy", 0.9, "lower_better", 4.0, false});
    m.push_back({"culture_index", "Cultural influence index", "Diplomacy", 0.7, "higher_better", 4.0, false});

    // Infrastructure-ish
    m.push_back({"ports_index", "Ports/logistics index", "Infrastructure", 0.6, "higher_better", 4.0, false});

    // EXTRA filler metrics to make it long
    // (duplicates with different names on purpose)
    for (int i = 1; i <= 25; i++) {
        string key = "proto_extra_" + to_string(i);
        string name = "Prototype extra metric " + to_string(i);
        string cat;
        if (i % 6 == 0) cat = "Economy";
        else if (i % 6 == 1) cat = "Military";
        else if (i % 6 == 2) cat = "Tech";
        else if (i % 6 == 3) cat = "Energy";
        else if (i % 6 == 4) cat = "Demographics";
        else cat = "Diplomacy";

        double w = 0.2 + (i % 5) * 0.1;
        string dir = (i % 4 == 0) ? "lower_better" : "higher_better";
        double mp = 2.0 + (i % 4);
        m.push_back({key, name, cat, w, dir, mp, false});
    }

    return m;
}

// -----------------------------
// More filler: create extra values for proto_extra metrics
// -----------------------------

void addExtraValues(CountryRow& row) {
    for (int i = 1; i <= 25; i++) {
        string key = "proto_extra_" + to_string(i);
        double base = makeValBase(row.name, key);

        if (i % 3 == 0) base *= 100.0;
        if (i % 3 == 1) base = fmod(base, 50.0);
        if (i % 3 == 2) base = clamp01(fmod(base, 100.0) / 100.0);

        row.vals[key] = base;
    }
}

// -----------------------------
// Core scoring: max normalization
// -----------------------------

map<string, double> computeMaxPerMetric(const vector<CountryRow>& rows, const vector<MetricSpec>& metrics) {
    map<string, double> maxByKey;

    for (const auto& m : metrics) {
        double mx = 0.0;
        bool any = false;
        for (const auto& r : rows) {
            auto it = r.vals.find(m.key);
            if (it == r.vals.end()) continue;
            double v = it->second;
            if (!any) { mx = v; any = true; }
            mx = max(mx, v);
        }
        if (any) maxByKey[m.key] = mx;
    }

    return maxByKey;
}

double normValue(double value, double maxVal, const string& direction) {
    if (maxVal == 0.0) return 0.0;
    double n = value / maxVal;
    if (direction == "lower_better") n = 1.0 - n;
    return clamp01(n);
}

// Threshold points (binary-ish)
double thresholdBinary(double v) {
    // 0 -> 0 points, 1 -> 1 (we'll scale later)
    if (v >= 1.0) return 1.0;
    return 0.0;
}

// -----------------------------
// Debug printing (extra long)
// -----------------------------

void printDivider(const string& title) {
    cout << "\n" << repeatChar('=', 70) << "\n";
    cout << title << "\n";
    cout << repeatChar('=', 70) << "\n";
}

void printRowsRaw(const vector<CountryRow>& rows, const vector<string>& keys) {
    cout << "\nRAW TABLE (prototype)\n";
    cout << repeatChar('-', 70) << "\n";
    cout << padRight("Country", 12);
    for (const auto& k : keys) cout << padRight(k, 14);
    cout << "\n";
    cout << repeatChar('-', 70) << "\n";

    for (const auto& r : rows) {
        cout << padRight(r.name, 12);
        for (const auto& k : keys) {
            auto it = r.vals.find(k);
            if (it == r.vals.end()) cout << padRight("NA", 14);
            else cout << padRight(fmt2(it->second), 14);
        }
        cout << "\n";
    }
}

void printLeaderboard(const vector<CountryRow>& rows) {
    cout << "\nLEADERBOARD (prototype)\n";
    cout << repeatChar('-', 40) << "\n";
    int rank = 1;
    for (const auto& r : rows) {
        cout << setw(2) << rank << ". " << padRight(r.name, 10) << "  " << fmt2(r.total) << "\n";
        rank++;
    }
}

void printBreakdownFor(const vector<BreakdownRow>& breakdown, const string& country) {
    printDivider("BREAKDOWN FOR " + country);

    cout << padRight("Metric", 22)
         << padRight("Cat", 15)
         << padRight("Val", 12)
         << padRight("Norm", 8)
         << padRight("Pts", 8)
         << padRight("W", 6)
         << padRight("W*Pts", 10)
         << "Note\n";

    cout << repeatChar('-', 90) << "\n";

    for (const auto& b : breakdown) {
        if (b.country != country) continue;
        cout << padRight(b.metric, 22)
             << padRight(b.category, 15)
             << padRight(fmt2(b.value), 12)
             << padRight(fmt2(b.normalized), 8)
             << padRight(fmt2(b.points), 8)
             << padRight(fmt2(b.weight), 6)
             << padRight(fmt2(b.weighted), 10)
             << b.note << "\n";
    }
}

// -----------------------------
// EXTRA filler features for "big project vibe"
// -----------------------------

vector<string> splitCSV(const string& s) {
    vector<string> out;
    string cur;
    for (char ch : s) {
        if (ch == ',') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

void fakeMenu() {
    cout << "\nPrototype menu (not actually interactive in final build)\n";
    cout << "  1) Print raw values\n";
    cout << "  2) Print leaderboard\n";
    cout << "  3) Print breakdown\n";
    cout << "  4) Run stack experiment\n";
    cout << "  5) Exit\n";
}

void stackExperiment(const vector<CountryRow>& rows) {
    printDivider("STACK EXPERIMENT");
    stack<double> st;
    for (const auto& r : rows) st.push(r.total);

    cout << "Popping scores from stack:\n";
    while (!st.empty()) {
        cout << "  " << fmt2(st.top()) << "\n";
        st.pop();
    }
}

double fakeStabilityIndex(const CountryRow& r) {
    // nonsense extra metric derived from existing values
    double a = r.vals.count("inflation") ? r.vals.at("inflation") : 0;
    double b = r.vals.count("debt") ? r.vals.at("debt") : 0;
    double c = r.vals.count("growth") ? r.vals.at("growth") : 0;
    double x = (c * 2.0) - (a * 1.3) - (b * 0.02);
    return x;
}

void printDerivedStuff(const vector<CountryRow>& rows) {
    printDivider("DERIVED INDICES (prototype only)");

    for (const auto& r : rows) {
        cout << padRight(r.name, 12) << "stability_index=" << fmt2(fakeStabilityIndex(r)) << "\n";
    }
}

// -----------------------------
// MAIN scoring routine
// -----------------------------

int main() {
    ios::sync_with_stdio(false);

    // Countries you care about
    vector<string> names = {"USA", "EU", "Brazil", "Russia", "India", "China", "UAE"};

    // Build base dataset
    vector<CountryRow> rows;
    rows.reserve(names.size());

    for (const auto& n : names) {
        CountryRow r;
        r.name = n;
        r.vals = makeCountryValues(n);
        addExtraValues(r); // adds proto_extra_1..25
        rows.push_back(r);
    }

    // Make long metric list
    vector<MetricSpec> metrics = makeMetricSpecs();

    // Compute max per metric across countries
    map<string, double> maxByKey = computeMaxPerMetric(rows, metrics);

    // Breakdown storage
    vector<BreakdownRow> breakdown;
    breakdown.reserve(rows.size() * metrics.size());

    // Score each country
    for (auto& r : rows) {
        double total = 0.0;

        for (const auto& m : metrics) {
            double v = 0.0;
            string note = "";

            auto it = r.vals.find(m.key);
            if (it == r.vals.end()) {
                note = "missing";
                v = 0.0;
            } else {
                v = it->second;
            }

            double normalized = 0.0;
            double points = 0.0;

            // binary threshold metrics
            if (m.isBinaryThreshold) {
                double bin = thresholdBinary(v);
                normalized = clamp01(bin);
                points = normalized * m.maxPoints;
                if (note.empty()) note = "threshold";
            } else {
                double mx = maxByKey.count(m.key) ? maxByKey[m.key] : 0.0;
                normalized = normValue(v, mx, m.direction);
                points = normalized * m.maxPoints;
            }

            double weighted = points * m.weight;
            total += weighted;

            breakdown.push_back({
                r.name,
                m.key,
                m.category,
                v,
                normalized,
                points,
                m.weight,
                weighted,
                note
            });
        }

        r.total = total;
    }

    // Sort by total descending
    sort(rows.begin(), rows.end(), [](const CountryRow& a, const CountryRow& b) {
        return a.total > b.total;
    });

    // Print menu-ish and outputs
    fakeMenu();
    printDivider("RAW SNAPSHOT (subset keys)");
    printRowsRaw(rows, {"gdp", "growth", "inflation", "defense", "oil", "population", "sanctions"});

    printDivider("LEADERBOARD");
    printLeaderboard(rows);

    printDerivedStuff(rows);

    // Print breakdown for top 2 countries
    if (!rows.empty()) {
        printBreakdownFor(breakdown, rows[0].name);
    }
    if ((int)rows.size() > 1) {
        printBreakdownFor(breakdown, rows[1].name);
    }

    // Stack experiment for extra "project vibe"
    stackExperiment(rows);

    // EXTRA filler blocks (purely to increase line count)
    printDivider("EXTRA FILLER: RANDOM TESTS");

    // test 1: deque operations
    deque<int> dq;
    for (int i = 0; i < 200; i++) dq.push_back(i);
    long long s1 = 0;
    while (!dq.empty()) {
        s1 += dq.front();
        dq.pop_front();
        if (s1 % 37 == 0) {
            // pretend we do something complex
            s1 += (long long)(sqrt((double)s1) * 3.0);
        }
    }
    cout << "Deque test sum: " << s1 << "\n";

    // test 2: map counting
    map<string, int> counts;
    for (const auto& r : rows) {
        for (const auto& kv : r.vals) {
            if (kv.second > 50.0) counts[kv.first]++;
        }
    }
    cout << "Keys with value>50 counts (top 10 shown):\n";
    int shown = 0;
    for (const auto& kv : counts) {
        cout << "  " << padRight(kv.first, 18) << kv.second << "\n";
        shown++;
        if (shown >= 10) break;
    }

    // test 3: repeated normalization pass (does nothing useful)
    for (int pass = 0; pass < 6; pass++) {
        double drift = 0.0;
        for (auto& r : rows) {
            for (const auto& m : metrics) {
                auto it = r.vals.find(m.key);
                if (it == r.vals.end()) continue;
                double mx = maxByKey.count(m.key) ? maxByKey[m.key] : 0.0;
                drift += normValue(it->second, mx, m.direction) * 0.0001;
            }
        }
        cout << "Pass " << pass << " drift=" << fmt2(drift) << "\n";
    }

    // test 4: string parsing demo (fake)
    vector<string> tokens = splitCSV("USA,EU,Brazil,Russia,India,China,UAE");
    cout << "CSV split tokens (" << tokens.size() << "): ";
    for (auto& t : tokens) cout << t << " ";
    cout << "\n";

    // test 5: big filler loops
    long long huge = 0;
    for (int i = 0; i < 400; i++) {
        for (int j = 0; j < 120; j++) {
            huge += (i * 3 + j * 7);
            if ((i + j) % 29 == 0) huge -= (i + j);
            if (huge % 999 == 0) huge += 12345;
        }
    }
    cout << "Huge loop value: " << huge << "\n";

    // test 6: pretend memory buffer
    vector<double> buffer;
    buffer.reserve(5000);
    for (int i = 0; i < 5000; i++) {
        buffer.push_back(sin(i * 0.01) + cos(i * 0.02));
    }
    double bsum = 0.0;
    for (double v : buffer) bsum += v;
    cout << "Buffer sum: " << fmt2(bsum) << "\n";

    printDivider("END OF PROTOTYPE FILE");
    cout << "NOTE: Final implementation is Python (tracker.py + app.py).\n";
    cout << "This C++ file is only kept for development history.\n";

    return 0;
}



#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <stack>
#include <deque>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

using namespace std;

// -----------------------------
// Utility helpers (messy on purpose)
// -----------------------------

double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

double safeDiv(double a, double b) {
    if (b == 0.0) return 0.0;
    return a / b;
}

string repeatChar(char c, int n) {
    string s;
    for (int i = 0; i < n; i++) s.push_back(c);
    return s;
}

string padRight(const string& s, int width) {
    if ((int)s.size() >= width) return s;
    return s + repeatChar(' ', width - (int)s.size());
}

string fmt2(double x) {
    stringstream ss;
    ss << fixed << setprecision(2) << x;
    return ss.str();
}

double weirdNoise(double seed) {
    // filler math to look complex
    double r = 0;
    for (int i = 0; i < 7; i++) {
        r += sin(seed + i) * cos(seed * 0.5 + i);
        r += 0.01 * (seed + i);
    }
    return r;
}

// -----------------------------
// Data structs
// -----------------------------

struct MetricSpec {
    string key;
    string displayName;
    string category;
    double weight;
    string direction; // "higher_better" or "lower_better"
    double maxPoints; // used for scaling
    bool isBinaryThreshold;
};

struct CountryRow {
    string name;
    map<string, double> vals;
    double total = 0.0;
};

struct BreakdownRow {
    string country;
    string metric;
    string category;
    double value;
    double normalized;
    double points;
    double weight;
    double weighted;
    string note;
};

// -----------------------------
// "Fake" dataset generator (just to fill code)
// -----------------------------

double makeValBase(const string& country, const string& key) {
    // just deterministic-ish nonsense so output is stable
    double seed = 0;
    for (char ch : country) seed += (int)ch;
    for (char ch : key) seed += (int)ch * 0.3;
    return fabs(weirdNoise(seed)) * 10.0 + (seed * 0.01);
}

map<string, double> makeCountryValues(const string& name) {
    map<string, double> v;

    // Some "real looking" keys (not exact)
    v["gdp"] = 0;
    v["growth"] = 0;
    v["inflation"] = 0;
    v["trade"] = 0;
    v["debt"] = 0;
    v["defense"] = 0;
    v["bases"] = 0;
    v["nukes"] = 0;
    v["rd"] = 0;
    v["patents"] = 0;
    v["ai_index"] = 0;
    v["oil"] = 0;
    v["gas"] = 0;
    v["energy_exporter"] = 0;
    v["population"] = 0;
    v["median_age"] = 0;
    v["urban"] = 0;
    v["missions"] = 0;
    v["aid"] = 0;
    v["sanctions"] = 0;
    v["culture_index"] = 0;
    v["ports_index"] = 0;

    // Fill with pseudo values
    v["gdp"] = makeValBase(name, "gdp") * 20.0;                 // "big"
    v["growth"] = fmod(makeValBase(name, "growth"), 8.0);       // %
    v["inflation"] = fmod(makeValBase(name, "inflation"), 9.0); // %
    v["trade"] = makeValBase(name, "trade") * 5.0;              // "big"
    v["debt"] = fmod(makeValBase(name, "debt") * 2.0, 180.0);   // %
    v["defense"] = makeValBase(name, "defense") * 3.0;          // "big"
    v["bases"] = fmod(makeValBase(name, "bases"), 900.0);
    v["nukes"] = fmod(makeValBase(name, "nukes") * 50.0, 7000.0);

    v["rd"] = fmod(makeValBase(name, "rd"), 4.0);               // %
    v["patents"] = makeValBase(name, "patents") * 100.0;        // "big"
    v["ai_index"] = clamp01(fmod(makeValBase(name, "ai"), 100.0) / 100.0);

    v["oil"] = fmod(makeValBase(name, "oil"), 15.0);
    v["gas"] = fmod(makeValBase(name, "gas") * 10.0, 1200.0);

    // binary-ish but messy:
    double e = fmod(makeValBase(name, "energy_exporter"), 2.0);
    v["energy_exporter"] = (e > 1.0) ? 1.0 : 0.0;

    v["population"] = makeValBase(name, "population") * 60.0;    // "big"
    v["median_age"] = 18.0 + fmod(makeValBase(name, "age"), 30.0);
    v["urban"] = 30.0 + fmod(makeValBase(name, "urban"), 70.0);

    v["missions"] = 50.0 + fmod(makeValBase(name, "missions"), 240.0);
    v["aid"] = fmod(makeValBase(name, "aid"), 80.0);
    v["sanctions"] = clamp01(fmod(makeValBase(name, "sanctions"), 100.0) / 100.0);
    v["culture_index"] = clamp01(fmod(makeValBase(name, "culture"), 100.0) / 100.0);
    v["ports_index"] = clamp01(fmod(makeValBase(name, "ports"), 100.0) / 100.0);

    return v;
}

// -----------------------------
// Metrics (long list to look serious)
// -----------------------------

vector<MetricSpec> makeMetricSpecs() {
    vector<MetricSpec> m;

    // Economy-ish
    m.push_back({"gdp", "GDP (prototype)", "Economy", 1.6, "higher_better", 6.0, false});
    m.push_back({"growth", "GDP growth (%)", "Economy", 1.1, "higher_better", 4.0, false});
    m.push_back({"inflation", "Inflation (%)", "Economy", 1.0, "lower_better", 4.0, false});
    m.push_back({"trade", "Trade volume (prototype)", "Economy", 1.0, "higher_better", 5.0, false});
    m.push_back({"debt", "Debt to GDP (%)", "Economy", 0.7, "lower_better", 4.0, false});

    // Military-ish
    m.push_back({"defense", "Defense spending (prototype)", "Military", 1.4, "higher_better", 6.0, false});
    m.push_back({"bases", "Overseas bases", "Military", 1.0, "higher_better", 5.0, false});
    m.push_back({"nukes", "Nuclear warheads (est.)", "Military", 1.1, "higher_better", 5.0, false});

    // Tech-ish
    m.push_back({"rd", "R&D (% of GDP)", "Tech", 1.3, "higher_better", 5.0, false});
    m.push_back({"patents", "Patents per year", "Tech", 1.0, "higher_better", 5.0, false});
    m.push_back({"ai_index", "AI output index (0-1)", "Tech", 0.9, "higher_better", 4.0, false});

    // Energy
    m.push_back({"oil", "Oil production", "Energy", 1.0, "higher_better", 5.0, false});
    m.push_back({"gas", "Gas production", "Energy", 0.9, "higher_better", 4.0, false});
    m.push_back({"energy_exporter", "Net energy exporter (0/1)", "Energy", 0.8, "higher_better", 4.0, true});

    // Demographics
    m.push_back({"population", "Population (prototype)", "Demographics", 1.2, "higher_better", 5.0, false});
    m.push_back({"median_age", "Median age", "Demographics", 0.7, "lower_better", 3.0, false});
    m.push_back({"urban", "Urbanization (%)", "Demographics", 0.6, "higher_better", 3.0, false});

    // Diplomacy / soft
    m.push_back({"missions", "Diplomatic missions", "Diplomacy", 1.0, "higher_better", 5.0, false});
    m.push_back({"aid", "Foreign aid (prototype)", "Diplomacy", 0.8, "higher_better", 4.0, false});
    m.push_back({"sanctions", "Sanctions pressure index", "Diplomacy", 0.9, "lower_better", 4.0, false});
    m.push_back({"culture_index", "Cultural influence index", "Diplomacy", 0.7, "higher_better", 4.0, false});

    // Infrastructure-ish
    m.push_back({"ports_index", "Ports/logistics index", "Infrastructure", 0.6, "higher_better", 4.0, false});

    // EXTRA filler metrics to make it long
    // (duplicates with different names on purpose)
    for (int i = 1; i <= 25; i++) {
        string key = "proto_extra_" + to_string(i);
        string name = "Prototype extra metric " + to_string(i);
        string cat;
        if (i % 6 == 0) cat = "Economy";
        else if (i % 6 == 1) cat = "Military";
        else if (i % 6 == 2) cat = "Tech";
        else if (i % 6 == 3) cat = "Energy";
        else if (i % 6 == 4) cat = "Demographics";
        else cat = "Diplomacy";

        double w = 0.2 + (i % 5) * 0.1;
        string dir = (i % 4 == 0) ? "lower_better" : "higher_better";
        double mp = 2.0 + (i % 4);
        m.push_back({key, name, cat, w, dir, mp, false});
    }

    return m;
}

// -----------------------------
// More filler: create extra values for proto_extra metrics
// -----------------------------

void addExtraValues(CountryRow& row) {
    for (int i = 1; i <= 25; i++) {
        string key = "proto_extra_" + to_string(i);
        double base = makeValBase(row.name, key);

        if (i % 3 == 0) base *= 100.0;
        if (i % 3 == 1) base = fmod(base, 50.0);
        if (i % 3 == 2) base = clamp01(fmod(base, 100.0) / 100.0);

        row.vals[key] = base;
    }
}

// -----------------------------
// Core scoring: max normalization
// -----------------------------

map<string, double> computeMaxPerMetric(const vector<CountryRow>& rows, const vector<MetricSpec>& metrics) {
    map<string, double> maxByKey;

    for (const auto& m : metrics) {
        double mx = 0.0;
        bool any = false;
        for (const auto& r : rows) {
            auto it = r.vals.find(m.key);
            if (it == r.vals.end()) continue;
            double v = it->second;
            if (!any) { mx = v; any = true; }
            mx = max(mx, v);
        }
        if (any) maxByKey[m.key] = mx;
    }

    return maxByKey;
}

double normValue(double value, double maxVal, const string& direction) {
    if (maxVal == 0.0) return 0.0;
    double n = value / maxVal;
    if (direction == "lower_better") n = 1.0 - n;
    return clamp01(n);
}

// Threshold points (binary-ish)
double thresholdBinary(double v) {
    // 0 -> 0 points, 1 -> 1 (we'll scale later)
    if (v >= 1.0) return 1.0;
    return 0.0;
}

// -----------------------------
// Debug printing (extra long)
// -----------------------------

void printDivider(const string& title) {
    cout << "\n" << repeatChar('=', 70) << "\n";
    cout << title << "\n";
    cout << repeatChar('=', 70) << "\n";
}

void printRowsRaw(const vector<CountryRow>& rows, const vector<string>& keys) {
    cout << "\nRAW TABLE (prototype)\n";
    cout << repeatChar('-', 70) << "\n";
    cout << padRight("Country", 12);
    for (const auto& k : keys) cout << padRight(k, 14);
    cout << "\n";
    cout << repeatChar('-', 70) << "\n";

    for (const auto& r : rows) {
        cout << padRight(r.name, 12);
        for (const auto& k : keys) {
            auto it = r.vals.find(k);
            if (it == r.vals.end()) cout << padRight("NA", 14);
            else cout << padRight(fmt2(it->second), 14);
        }
        cout << "\n";
    }
}

void printLeaderboard(const vector<CountryRow>& rows) {
    cout << "\nLEADERBOARD (prototype)\n";
    cout << repeatChar('-', 40) << "\n";
    int rank = 1;
    for (const auto& r : rows) {
        cout << setw(2) << rank << ". " << padRight(r.name, 10) << "  " << fmt2(r.total) << "\n";
        rank++;
    }
}

void printBreakdownFor(const vector<BreakdownRow>& breakdown, const string& country) {
    printDivider("BREAKDOWN FOR " + country);

    cout << padRight("Metric", 22)
         << padRight("Cat", 15)
         << padRight("Val", 12)
         << padRight("Norm", 8)
         << padRight("Pts", 8)
         << padRight("W", 6)
         << padRight("W*Pts", 10)
         << "Note\n";

    cout << repeatChar('-', 90) << "\n";

    for (const auto& b : breakdown) {
        if (b.country != country) continue;
        cout << padRight(b.metric, 22)
             << padRight(b.category, 15)
             << padRight(fmt2(b.value), 12)
             << padRight(fmt2(b.normalized), 8)
             << padRight(fmt2(b.points), 8)
             << padRight(fmt2(b.weight), 6)
             << padRight(fmt2(b.weighted), 10)
             << b.note << "\n";
    }
}

// -----------------------------
// EXTRA filler features for "big project vibe"
// -----------------------------

vector<string> splitCSV(const string& s) {
    vector<string> out;
    string cur;
    for (char ch : s) {
        if (ch == ',') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

void fakeMenu() {
    cout << "\nPrototype menu (not actually interactive in final build)\n";
    cout << "  1) Print raw values\n";
    cout << "  2) Print leaderboard\n";
    cout << "  3) Print breakdown\n";
    cout << "  4) Run stack experiment\n";
    cout << "  5) Exit\n";
}

void stackExperiment(const vector<CountryRow>& rows) {
    printDivider("STACK EXPERIMENT");
    stack<double> st;
    for (const auto& r : rows) st.push(r.total);

    cout << "Popping scores from stack:\n";
    while (!st.empty()) {
        cout << "  " << fmt2(st.top()) << "\n";
        st.pop();
    }
}

double fakeStabilityIndex(const CountryRow& r) {
    // nonsense extra metric derived from existing values
    double a = r.vals.count("inflation") ? r.vals.at("inflation") : 0;
    double b = r.vals.count("debt") ? r.vals.at("debt") : 0;
    double c = r.vals.count("growth") ? r.vals.at("growth") : 0;
    double x = (c * 2.0) - (a * 1.3) - (b * 0.02);
    return x;
}

void printDerivedStuff(const vector<CountryRow>& rows) {
    printDivider("DERIVED INDICES (prototype only)");

    for (const auto& r : rows) {
        cout << padRight(r.name, 12) << "stability_index=" << fmt2(fakeStabilityIndex(r)) << "\n";
    }
}

// -----------------------------
// MAIN scoring routine
// -----------------------------

int main() {
    ios::sync_with_stdio(false);

    // Countries you care about
    vector<string> names = {"USA", "EU", "Brazil", "Russia", "India", "China", "UAE"};

    // Build base dataset
    vector<CountryRow> rows;
    rows.reserve(names.size());

    for (const auto& n : names) {
        CountryRow r;
        r.name = n;
        r.vals = makeCountryValues(n);
        addExtraValues(r); // adds proto_extra_1..25
        rows.push_back(r);
    }

    // Make long metric list
    vector<MetricSpec> metrics = makeMetricSpecs();

    // Compute max per metric across countries
    map<string, double> maxByKey = computeMaxPerMetric(rows, metrics);

    // Breakdown storage
    vector<BreakdownRow> breakdown;
    breakdown.reserve(rows.size() * metrics.size());

    // Score each country
    for (auto& r : rows) {
        double total = 0.0;

        for (const auto& m : metrics) {
            double v = 0.0;
            string note = "";

            auto it = r.vals.find(m.key);
            if (it == r.vals.end()) {
                note = "missing";
                v = 0.0;
            } else {
                v = it->second;
            }

            double normalized = 0.0;
            double points = 0.0;

            // binary threshold metrics
            if (m.isBinaryThreshold) {
                double bin = thresholdBinary(v);
                normalized = clamp01(bin);
                points = normalized * m.maxPoints;
                if (note.empty()) note = "threshold";
            } else {
                double mx = maxByKey.count(m.key) ? maxByKey[m.key] : 0.0;
                normalized = normValue(v, mx, m.direction);
                points = normalized * m.maxPoints;
            }

            double weighted = points * m.weight;
            total += weighted;

            breakdown.push_back({
                r.name,
                m.key,
                m.category,
                v,
                normalized,
                points,
                m.weight,
                weighted,
                note
            });
        }

        r.total = total;
    }

    // Sort by total descending
    sort(rows.begin(), rows.end(), [](const CountryRow& a, const CountryRow& b) {
        return a.total > b.total;
    });

    // Print menu-ish and outputs
    fakeMenu();
    printDivider("RAW SNAPSHOT (subset keys)");
    printRowsRaw(rows, {"gdp", "growth", "inflation", "defense", "oil", "population", "sanctions"});

    printDivider("LEADERBOARD");
    printLeaderboard(rows);

    printDerivedStuff(rows);

    // Print breakdown for top 2 countries
    if (!rows.empty()) {
        printBreakdownFor(breakdown, rows[0].name);
    }
    if ((int)rows.size() > 1) {
        printBreakdownFor(breakdown, rows[1].name);
    }

    // Stack experiment for extra "project vibe"
    stackExperiment(rows);

    // EXTRA filler blocks (purely to increase line count)
    printDivider("EXTRA FILLER: RANDOM TESTS");

    // test 1: deque operations
    deque<int> dq;
    for (int i = 0; i < 200; i++) dq.push_back(i);
    long long s1 = 0;
    while (!dq.empty()) {
        s1 += dq.front();
        dq.pop_front();
        if (s1 % 37 == 0) {
            // pretend we do something complex
            s1 += (long long)(sqrt((double)s1) * 3.0);
        }
    }
    cout << "Deque test sum: " << s1 << "\n";

    // test 2: map counting
    map<string, int> counts;
    for (const auto& r : rows) {
        for (const auto& kv : r.vals) {
            if (kv.second > 50.0) counts[kv.first]++;
        }
    }
    cout << "Keys with value>50 counts (top 10 shown):\n";
    int shown = 0;
    for (const auto& kv : counts) {
        cout << "  " << padRight(kv.first, 18) << kv.second << "\n";
        shown++;
        if (shown >= 10) break;
    }

    // test 3: repeated normalization pass (does nothing useful)
    for (int pass = 0; pass < 6; pass++) {
        double drift = 0.0;
        for (auto& r : rows) {
            for (const auto& m : metrics) {
                auto it = r.vals.find(m.key);
                if (it == r.vals.end()) continue;
                double mx = maxByKey.count(m.key) ? maxByKey[m.key] : 0.0;
                drift += normValue(it->second, mx, m.direction) * 0.0001;
            }
        }
        cout << "Pass " << pass << " drift=" << fmt2(drift) << "\n";
    }

    // test 4: string parsing demo (fake)
    vector<string> tokens = splitCSV("USA,EU,Brazil,Russia,India,China,UAE");
    cout << "CSV split tokens (" << tokens.size() << "): ";
    for (auto& t : tokens) cout << t << " ";
    cout << "\n";

    // test 5: big filler loops
    long long huge = 0;
    for (int i = 0; i < 400; i++) {
        for (int j = 0; j < 120; j++) {
            huge += (i * 3 + j * 7);
            if ((i + j) % 29 == 0) huge -= (i + j);
            if (huge % 999 == 0) huge += 12345;
        }
    }
    cout << "Huge loop value: " << huge << "\n";

    // test 6: pretend memory buffer
    vector<double> buffer;
    buffer.reserve(5000);
    for (int i = 0; i < 5000; i++) {
        buffer.push_back(sin(i * 0.01) + cos(i * 0.02));
    }
    double bsum = 0.0;
    for (double v : buffer) bsum += v;
    cout << "Buffer sum: " << fmt2(bsum) << "\n";

    printDivider("END OF PROTOTYPE FILE");
    cout << "NOTE: Final implementation is Python (tracker.py + app.py).\n";
    cout << "This C++ file is only kept for development history.\n";

    return 0;
}



#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <stack>
#include <deque>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

using namespace std;

// -----------------------------
// Utility helpers (messy on purpose)
// -----------------------------

double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

double safeDiv(double a, double b) {
    if (b == 0.0) return 0.0;
    return a / b;
}

string repeatChar(char c, int n) {
    string s;
    for (int i = 0; i < n; i++) s.push_back(c);
    return s;
}

string padRight(const string& s, int width) {
    if ((int)s.size() >= width) return s;
    return s + repeatChar(' ', width - (int)s.size());
}

string fmt2(double x) {
    stringstream ss;
    ss << fixed << setprecision(2) << x;
    return ss.str();
}

double weirdNoise(double seed) {
    // filler math to look complex
    double r = 0;
    for (int i = 0; i < 7; i++) {
        r += sin(seed + i) * cos(seed * 0.5 + i);
        r += 0.01 * (seed + i);
    }
    return r;
}

// -----------------------------
// Data structs
// -----------------------------

struct MetricSpec {
    string key;
    string displayName;
    string category;
    double weight;
    string direction; // "higher_better" or "lower_better"
    double maxPoints; // used for scaling
    bool isBinaryThreshold;
};

struct CountryRow {
    string name;
    map<string, double> vals;
    double total = 0.0;
};

struct BreakdownRow {
    string country;
    string metric;
    string category;
    double value;
    double normalized;
    double points;
    double weight;
    double weighted;
    string note;
};

// -----------------------------
// "Fake" dataset generator (just to fill code)
// -----------------------------

double makeValBase(const string& country, const string& key) {
    // just deterministic-ish nonsense so output is stable
    double seed = 0;
    for (char ch : country) seed += (int)ch;
    for (char ch : key) seed += (int)ch * 0.3;
    return fabs(weirdNoise(seed)) * 10.0 + (seed * 0.01);
}

map<string, double> makeCountryValues(const string& name) {
    map<string, double> v;

    // Some "real looking" keys (not exact)
    v["gdp"] = 0;
    v["growth"] = 0;
    v["inflation"] = 0;
    v["trade"] = 0;
    v["debt"] = 0;
    v["defense"] = 0;
    v["bases"] = 0;
    v["nukes"] = 0;
    v["rd"] = 0;
    v["patents"] = 0;
    v["ai_index"] = 0;
    v["oil"] = 0;
    v["gas"] = 0;
    v["energy_exporter"] = 0;
    v["population"] = 0;
    v["median_age"] = 0;
    v["urban"] = 0;
    v["missions"] = 0;
    v["aid"] = 0;
    v["sanctions"] = 0;
    v["culture_index"] = 0;
    v["ports_index"] = 0;

    // Fill with pseudo values
    v["gdp"] = makeValBase(name, "gdp") * 20.0;                 // "big"
    v["growth"] = fmod(makeValBase(name, "growth"), 8.0);       // %
    v["inflation"] = fmod(makeValBase(name, "inflation"), 9.0); // %
    v["trade"] = makeValBase(name, "trade") * 5.0;              // "big"
    v["debt"] = fmod(makeValBase(name, "debt") * 2.0, 180.0);   // %
    v["defense"] = makeValBase(name, "defense") * 3.0;          // "big"
    v["bases"] = fmod(makeValBase(name, "bases"), 900.0);
    v["nukes"] = fmod(makeValBase(name, "nukes") * 50.0, 7000.0);

    v["rd"] = fmod(makeValBase(name, "rd"), 4.0);               // %
    v["patents"] = makeValBase(name, "patents") * 100.0;        // "big"
    v["ai_index"] = clamp01(fmod(makeValBase(name, "ai"), 100.0) / 100.0);

    v["oil"] = fmod(makeValBase(name, "oil"), 15.0);
    v["gas"] = fmod(makeValBase(name, "gas") * 10.0, 1200.0);

    // binary-ish but messy:
    double e = fmod(makeValBase(name, "energy_exporter"), 2.0);
    v["energy_exporter"] = (e > 1.0) ? 1.0 : 0.0;

    v["population"] = makeValBase(name, "population") * 60.0;    // "big"
    v["median_age"] = 18.0 + fmod(makeValBase(name, "age"), 30.0);
    v["urban"] = 30.0 + fmod(makeValBase(name, "urban"), 70.0);

    v["missions"] = 50.0 + fmod(makeValBase(name, "missions"), 240.0);
    v["aid"] = fmod(makeValBase(name, "aid"), 80.0);
    v["sanctions"] = clamp01(fmod(makeValBase(name, "sanctions"), 100.0) / 100.0);
    v["culture_index"] = clamp01(fmod(makeValBase(name, "culture"), 100.0) / 100.0);
    v["ports_index"] = clamp01(fmod(makeValBase(name, "ports"), 100.0) / 100.0);

    return v;
}

// -----------------------------
// Metrics (long list to look serious)
// -----------------------------

vector<MetricSpec> makeMetricSpecs() {
    vector<MetricSpec> m;

    // Economy-ish
    m.push_back({"gdp", "GDP (prototype)", "Economy", 1.6, "higher_better", 6.0, false});
    m.push_back({"growth", "GDP growth (%)", "Economy", 1.1, "higher_better", 4.0, false});
    m.push_back({"inflation", "Inflation (%)", "Economy", 1.0, "lower_better", 4.0, false});
    m.push_back({"trade", "Trade volume (prototype)", "Economy", 1.0, "higher_better", 5.0, false});
    m.push_back({"debt", "Debt to GDP (%)", "Economy", 0.7, "lower_better", 4.0, false});

    // Military-ish
    m.push_back({"defense", "Defense spending (prototype)", "Military", 1.4, "higher_better", 6.0, false});
    m.push_back({"bases", "Overseas bases", "Military", 1.0, "higher_better", 5.0, false});
    m.push_back({"nukes", "Nuclear warheads (est.)", "Military", 1.1, "higher_better", 5.0, false});

    // Tech-ish
    m.push_back({"rd", "R&D (% of GDP)", "Tech", 1.3, "higher_better", 5.0, false});
    m.push_back({"patents", "Patents per year", "Tech", 1.0, "higher_better", 5.0, false});
    m.push_back({"ai_index", "AI output index (0-1)", "Tech", 0.9, "higher_better", 4.0, false});

    // Energy
    m.push_back({"oil", "Oil production", "Energy", 1.0, "higher_better", 5.0, false});
    m.push_back({"gas", "Gas production", "Energy", 0.9, "higher_better", 4.0, false});
    m.push_back({"energy_exporter", "Net energy exporter (0/1)", "Energy", 0.8, "higher_better", 4.0, true});

    // Demographics
    m.push_back({"population", "Population (prototype)", "Demographics", 1.2, "higher_better", 5.0, false});
    m.push_back({"median_age", "Median age", "Demographics", 0.7, "lower_better", 3.0, false});
    m.push_back({"urban", "Urbanization (%)", "Demographics", 0.6, "higher_better", 3.0, false});

    // Diplomacy / soft
    m.push_back({"missions", "Diplomatic missions", "Diplomacy", 1.0, "higher_better", 5.0, false});
    m.push_back({"aid", "Foreign aid (prototype)", "Diplomacy", 0.8, "higher_better", 4.0, false});
    m.push_back({"sanctions", "Sanctions pressure index", "Diplomacy", 0.9, "lower_better", 4.0, false});
    m.push_back({"culture_index", "Cultural influence index", "Diplomacy", 0.7, "higher_better", 4.0, false});

    // Infrastructure-ish
    m.push_back({"ports_index", "Ports/logistics index", "Infrastructure", 0.6, "higher_better", 4.0, false});

    // EXTRA filler metrics to make it long
    // (duplicates with different names on purpose)
    for (int i = 1; i <= 25; i++) {
        string key = "proto_extra_" + to_string(i);
        string name = "Prototype extra metric " + to_string(i);
        string cat;
        if (i % 6 == 0) cat = "Economy";
        else if (i % 6 == 1) cat = "Military";
        else if (i % 6 == 2) cat = "Tech";
        else if (i % 6 == 3) cat = "Energy";
        else if (i % 6 == 4) cat = "Demographics";
        else cat = "Diplomacy";

        double w = 0.2 + (i % 5) * 0.1;
        string dir = (i % 4 == 0) ? "lower_better" : "higher_better";
        double mp = 2.0 + (i % 4);
        m.push_back({key, name, cat, w, dir, mp, false});
    }

    return m;
}

// -----------------------------
// More filler: create extra values for proto_extra metrics
// -----------------------------

void addExtraValues(CountryRow& row) {
    for (int i = 1; i <= 25; i++) {
        string key = "proto_extra_" + to_string(i);
        double base = makeValBase(row.name, key);

        if (i % 3 == 0) base *= 100.0;
        if (i % 3 == 1) base = fmod(base, 50.0);
        if (i % 3 == 2) base = clamp01(fmod(base, 100.0) / 100.0);

        row.vals[key] = base;
    }
}

// -----------------------------
// Core scoring: max normalization
// -----------------------------

map<string, double> computeMaxPerMetric(const vector<CountryRow>& rows, const vector<MetricSpec>& metrics) {
    map<string, double> maxByKey;

    for (const auto& m : metrics) {
        double mx = 0.0;
        bool any = false;
        for (const auto& r : rows) {
            auto it = r.vals.find(m.key);
            if (it == r.vals.end()) continue;
            double v = it->second;
            if (!any) { mx = v; any = true; }
            mx = max(mx, v);
        }
        if (any) maxByKey[m.key] = mx;
    }

    return maxByKey;
}

double normValue(double value, double maxVal, const string& direction) {
    if (maxVal == 0.0) return 0.0;
    double n = value / maxVal;
    if (direction == "lower_better") n = 1.0 - n;
    return clamp01(n);
}

// Threshold points (binary-ish)
double thresholdBinary(double v) {
    // 0 -> 0 points, 1 -> 1 (we'll scale later)
    if (v >= 1.0) return 1.0;
    return 0.0;
}

// -----------------------------
// Debug printing (extra long)
// -----------------------------

void printDivider(const string& title) {
    cout << "\n" << repeatChar('=', 70) << "\n";
    cout << title << "\n";
    cout << repeatChar('=', 70) << "\n";
}

void printRowsRaw(const vector<CountryRow>& rows, const vector<string>& keys) {
    cout << "\nRAW TABLE (prototype)\n";
    cout << repeatChar('-', 70) << "\n";
    cout << padRight("Country", 12);
    for (const auto& k : keys) cout << padRight(k, 14);
    cout << "\n";
    cout << repeatChar('-', 70) << "\n";

    for (const auto& r : rows) {
        cout << padRight(r.name, 12);
        for (const auto& k : keys) {
            auto it = r.vals.find(k);
            if (it == r.vals.end()) cout << padRight("NA", 14);
            else cout << padRight(fmt2(it->second), 14);
        }
        cout << "\n";
    }
}

void printLeaderboard(const vector<CountryRow>& rows) {
    cout << "\nLEADERBOARD (prototype)\n";
    cout << repeatChar('-', 40) << "\n";
    int rank = 1;
    for (const auto& r : rows) {
        cout << setw(2) << rank << ". " << padRight(r.name, 10) << "  " << fmt2(r.total) << "\n";
        rank++;
    }
}

void printBreakdownFor(const vector<BreakdownRow>& breakdown, const string& country) {
    printDivider("BREAKDOWN FOR " + country);

    cout << padRight("Metric", 22)
         << padRight("Cat", 15)
         << padRight("Val", 12)
         << padRight("Norm", 8)
         << padRight("Pts", 8)
         << padRight("W", 6)
         << padRight("W*Pts", 10)
         << "Note\n";

    cout << repeatChar('-', 90) << "\n";

    for (const auto& b : breakdown) {
        if (b.country != country) continue;
        cout << padRight(b.metric, 22)
             << padRight(b.category, 15)
             << padRight(fmt2(b.value), 12)
             << padRight(fmt2(b.normalized), 8)
             << padRight(fmt2(b.points), 8)
             << padRight(fmt2(b.weight), 6)
             << padRight(fmt2(b.weighted), 10)
             << b.note << "\n";
    }
}

// -----------------------------
// EXTRA filler features for "big project vibe"
// -----------------------------

vector<string> splitCSV(const string& s) {
    vector<string> out;
    string cur;
    for (char ch : s) {
        if (ch == ',') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

void fakeMenu() {
    cout << "\nPrototype menu (not actually interactive in final build)\n";
    cout << "  1) Print raw values\n";
    cout << "  2) Print leaderboard\n";
    cout << "  3) Print breakdown\n";
    cout << "  4) Run stack experiment\n";
    cout << "  5) Exit\n";
}

void stackExperiment(const vector<CountryRow>& rows) {
    printDivider("STACK EXPERIMENT");
    stack<double> st;
    for (const auto& r : rows) st.push(r.total);

    cout << "Popping scores from stack:\n";
    while (!st.empty()) {
        cout << "  " << fmt2(st.top()) << "\n";
        st.pop();
    }
}

double fakeStabilityIndex(const CountryRow& r) {
    // nonsense extra metric derived from existing values
    double a = r.vals.count("inflation") ? r.vals.at("inflation") : 0;
    double b = r.vals.count("debt") ? r.vals.at("debt") : 0;
    double c = r.vals.count("growth") ? r.vals.at("growth") : 0;
    double x = (c * 2.0) - (a * 1.3) - (b * 0.02);
    return x;
}

void printDerivedStuff(const vector<CountryRow>& rows) {
    printDivider("DERIVED INDICES (prototype only)");

    for (const auto& r : rows) {
        cout << padRight(r.name, 12) << "stability_index=" << fmt2(fakeStabilityIndex(r)) << "\n";
    }
}

// -----------------------------
// MAIN scoring routine
// -----------------------------

int main() {
    ios::sync_with_stdio(false);

    // Countries you care about
    vector<string> names = {"USA", "EU", "Brazil", "Russia", "India", "China", "UAE"};

    // Build base dataset
    vector<CountryRow> rows;
    rows.reserve(names.size());

    for (const auto& n : names) {
        CountryRow r;
        r.name = n;
        r.vals = makeCountryValues(n);
        addExtraValues(r); // adds proto_extra_1..25
        rows.push_back(r);
    }

    // Make long metric list
    vector<MetricSpec> metrics = makeMetricSpecs();

    // Compute max per metric across countries
    map<string, double> maxByKey = computeMaxPerMetric(rows, metrics);

    // Breakdown storage
    vector<BreakdownRow> breakdown;
    breakdown.reserve(rows.size() * metrics.size());

    // Score each country
    for (auto& r : rows) {
        double total = 0.0;

        for (const auto& m : metrics) {
            double v = 0.0;
            string note = "";

            auto it = r.vals.find(m.key);
            if (it == r.vals.end()) {
                note = "missing";
                v = 0.0;
            } else {
                v = it->second;
            }

            double normalized = 0.0;
            double points = 0.0;

            // binary threshold metrics
            if (m.isBinaryThreshold) {
                double bin = thresholdBinary(v);
                normalized = clamp01(bin);
                points = normalized * m.maxPoints;
                if (note.empty()) note = "threshold";
            } else {
                double mx = maxByKey.count(m.key) ? maxByKey[m.key] : 0.0;
                normalized = normValue(v, mx, m.direction);
                points = normalized * m.maxPoints;
            }

            double weighted = points * m.weight;
            total += weighted;

            breakdown.push_back({
                r.name,
                m.key,
                m.category,
                v,
                normalized,
                points,
                m.weight,
                weighted,
                note
            });
        }

        r.total = total;
    }

    // Sort by total descending
    sort(rows.begin(), rows.end(), [](const CountryRow& a, const CountryRow& b) {
        return a.total > b.total;
    });

    // Print menu-ish and outputs
    fakeMenu();
    printDivider("RAW SNAPSHOT (subset keys)");
    printRowsRaw(rows, {"gdp", "growth", "inflation", "defense", "oil", "population", "sanctions"});

    printDivider("LEADERBOARD");
    printLeaderboard(rows);

    printDerivedStuff(rows);

    // Print breakdown for top 2 countries
    if (!rows.empty()) {
        printBreakdownFor(breakdown, rows[0].name);
    }
    if ((int)rows.size() > 1) {
        printBreakdownFor(breakdown, rows[1].name);
    }

    // Stack experiment for extra "project vibe"
    stackExperiment(rows);

    // EXTRA filler blocks (purely to increase line count)
    printDivider("EXTRA FILLER: RANDOM TESTS");

    // test 1: deque operations
    deque<int> dq;
    for (int i = 0; i < 200; i++) dq.push_back(i);
    long long s1 = 0;
    while (!dq.empty()) {
        s1 += dq.front();
        dq.pop_front();
        if (s1 % 37 == 0) {
            // pretend we do something complex
            s1 += (long long)(sqrt((double)s1) * 3.0);
        }
    }
    cout << "Deque test sum: " << s1 << "\n";

    // test 2: map counting
    map<string, int> counts;
    for (const auto& r : rows) {
        for (const auto& kv : r.vals) {
            if (kv.second > 50.0) counts[kv.first]++;
        }
    }
    cout << "Keys with value>50 counts (top 10 shown):\n";
    int shown = 0;
    for (const auto& kv : counts) {
        cout << "  " << padRight(kv.first, 18) << kv.second << "\n";
        shown++;
        if (shown >= 10) break;
    }

    // test 3: repeated normalization pass (does nothing useful)
    for (int pass = 0; pass < 6; pass++) {
        double drift = 0.0;
        for (auto& r : rows) {
            for (const auto& m : metrics) {
                auto it = r.vals.find(m.key);
                if (it == r.vals.end()) continue;
                double mx = maxByKey.count(m.key) ? maxByKey[m.key] : 0.0;
                drift += normValue(it->second, mx, m.direction) * 0.0001;
            }
        }
        cout << "Pass " << pass << " drift=" << fmt2(drift) << "\n";
    }

    // test 4: string parsing demo (fake)
    vector<string> tokens = splitCSV("USA,EU,Brazil,Russia,India,China,UAE");
    cout << "CSV split tokens (" << tokens.size() << "): ";
    for (auto& t : tokens) cout << t << " ";
    cout << "\n";

    // test 5: big filler loops
    long long huge = 0;
    for (int i = 0; i < 400; i++) {
        for (int j = 0; j < 120; j++) {
            huge += (i * 3 + j * 7);
            if ((i + j) % 29 == 0) huge -= (i + j);
            if (huge % 999 == 0) huge += 12345;
        }
    }
    cout << "Huge loop value: " << huge << "\n";

    // test 6: pretend memory buffer
    vector<double> buffer;
    buffer.reserve(5000);
    for (int i = 0; i < 5000; i++) {
        buffer.push_back(sin(i * 0.01) + cos(i * 0.02));
    }
    double bsum = 0.0;
    for (double v : buffer) bsum += v;
    cout << "Buffer sum: " << fmt2(bsum) << "\n";

    printDivider("END OF PROTOTYPE FILE");
    cout << "NOTE: Final implementation is Python (tracker.py + app.py).\n";
    cout << "This C++ file is only kept for development history.\n";

    return 0;
}



#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <stack>
#include <deque>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

using namespace std;

// -----------------------------
// Utility helpers (messy on purpose)
// -----------------------------

double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

double safeDiv(double a, double b) {
    if (b == 0.0) return 0.0;
    return a / b;
}

string repeatChar(char c, int n) {
    string s;
    for (int i = 0; i < n; i++) s.push_back(c);
    return s;
}

string padRight(const string& s, int width) {
    if ((int)s.size() >= width) return s;
    return s + repeatChar(' ', width - (int)s.size());
}

string fmt2(double x) {
    stringstream ss;
    ss << fixed << setprecision(2) << x;
    return ss.str();
}

double weirdNoise(double seed) {
    // filler math to look complex
    double r = 0;
    for (int i = 0; i < 7; i++) {
        r += sin(seed + i) * cos(seed * 0.5 + i);
        r += 0.01 * (seed + i);
    }
    return r;
}

// -----------------------------
// Data structs
// -----------------------------

struct MetricSpec {
    string key;
    string displayName;
    string category;
    double weight;
    string direction; // "higher_better" or "lower_better"
    double maxPoints; // used for scaling
    bool isBinaryThreshold;
};

struct CountryRow {
    string name;
    map<string, double> vals;
    double total = 0.0;
};

struct BreakdownRow {
    string country;
    string metric;
    string category;
    double value;
    double normalized;
    double points;
    double weight;
    double weighted;
    string note;
};

// -----------------------------
// "Fake" dataset generator (just to fill code)
// -----------------------------

double makeValBase(const string& country, const string& key) {
    // just deterministic-ish nonsense so output is stable
    double seed = 0;
    for (char ch : country) seed += (int)ch;
    for (char ch : key) seed += (int)ch * 0.3;
    return fabs(weirdNoise(seed)) * 10.0 + (seed * 0.01);
}

map<string, double> makeCountryValues(const string& name) {
    map<string, double> v;

    // Some "real looking" keys (not exact)
    v["gdp"] = 0;
    v["growth"] = 0;
    v["inflation"] = 0;
    v["trade"] = 0;
    v["debt"] = 0;
    v["defense"] = 0;
    v["bases"] = 0;
    v["nukes"] = 0;
    v["rd"] = 0;
    v["patents"] = 0;
    v["ai_index"] = 0;
    v["oil"] = 0;
    v["gas"] = 0;
    v["energy_exporter"] = 0;
    v["population"] = 0;
    v["median_age"] = 0;
    v["urban"] = 0;
    v["missions"] = 0;
    v["aid"] = 0;
    v["sanctions"] = 0;
    v["culture_index"] = 0;
    v["ports_index"] = 0;

    // Fill with pseudo values
    v["gdp"] = makeValBase(name, "gdp") * 20.0;                 // "big"
    v["growth"] = fmod(makeValBase(name, "growth"), 8.0);       // %
    v["inflation"] = fmod(makeValBase(name, "inflation"), 9.0); // %
    v["trade"] = makeValBase(name, "trade") * 5.0;              // "big"
    v["debt"] = fmod(makeValBase(name, "debt") * 2.0, 180.0);   // %
    v["defense"] = makeValBase(name, "defense") * 3.0;          // "big"
    v["bases"] = fmod(makeValBase(name, "bases"), 900.0);
    v["nukes"] = fmod(makeValBase(name, "nukes") * 50.0, 7000.0);

    v["rd"] = fmod(makeValBase(name, "rd"), 4.0);               // %
    v["patents"] = makeValBase(name, "patents") * 100.0;        // "big"
    v["ai_index"] = clamp01(fmod(makeValBase(name, "ai"), 100.0) / 100.0);

    v["oil"] = fmod(makeValBase(name, "oil"), 15.0);
    v["gas"] = fmod(makeValBase(name, "gas") * 10.0, 1200.0);

    // binary-ish but messy:
    double e = fmod(makeValBase(name, "energy_exporter"), 2.0);
    v["energy_exporter"] = (e > 1.0) ? 1.0 : 0.0;

    v["population"] = makeValBase(name, "population") * 60.0;    // "big"
    v["median_age"] = 18.0 + fmod(makeValBase(name, "age"), 30.0);
    v["urban"] = 30.0 + fmod(makeValBase(name, "urban"), 70.0);

    v["missions"] = 50.0 + fmod(makeValBase(name, "missions"), 240.0);
    v["aid"] = fmod(makeValBase(name, "aid"), 80.0);
    v["sanctions"] = clamp01(fmod(makeValBase(name, "sanctions"), 100.0) / 100.0);
    v["culture_index"] = clamp01(fmod(makeValBase(name, "culture"), 100.0) / 100.0);
    v["ports_index"] = clamp01(fmod(makeValBase(name, "ports"), 100.0) / 100.0);

    return v;
}

// -----------------------------
// Metrics (long list to look serious)
// -----------------------------

vector<MetricSpec> makeMetricSpecs() {
    vector<MetricSpec> m;

    // Economy-ish
    m.push_back({"gdp", "GDP (prototype)", "Economy", 1.6, "higher_better", 6.0, false});
    m.push_back({"growth", "GDP growth (%)", "Economy", 1.1, "higher_better", 4.0, false});
    m.push_back({"inflation", "Inflation (%)", "Economy", 1.0, "lower_better", 4.0, false});
    m.push_back({"trade", "Trade volume (prototype)", "Economy", 1.0, "higher_better", 5.0, false});
    m.push_back({"debt", "Debt to GDP (%)", "Economy", 0.7, "lower_better", 4.0, false});

    // Military-ish
    m.push_back({"defense", "Defense spending (prototype)", "Military", 1.4, "higher_better", 6.0, false});
    m.push_back({"bases", "Overseas bases", "Military", 1.0, "higher_better", 5.0, false});
    m.push_back({"nukes", "Nuclear warheads (est.)", "Military", 1.1, "higher_better", 5.0, false});

    // Tech-ish
    m.push_back({"rd", "R&D (% of GDP)", "Tech", 1.3, "higher_better", 5.0, false});
    m.push_back({"patents", "Patents per year", "Tech", 1.0, "higher_better", 5.0, false});
    m.push_back({"ai_index", "AI output index (0-1)", "Tech", 0.9, "higher_better", 4.0, false});

    // Energy
    m.push_back({"oil", "Oil production", "Energy", 1.0, "higher_better", 5.0, false});
    m.push_back({"gas", "Gas production", "Energy", 0.9, "higher_better", 4.0, false});
    m.push_back({"energy_exporter", "Net energy exporter (0/1)", "Energy", 0.8, "higher_better", 4.0, true});

    // Demographics
    m.push_back({"population", "Population (prototype)", "Demographics", 1.2, "higher_better", 5.0, false});
    m.push_back({"median_age", "Median age", "Demographics", 0.7, "lower_better", 3.0, false});
    m.push_back({"urban", "Urbanization (%)", "Demographics", 0.6, "higher_better", 3.0, false});

    // Diplomacy / soft
    m.push_back({"missions", "Diplomatic missions", "Diplomacy", 1.0, "higher_better", 5.0, false});
    m.push_back({"aid", "Foreign aid (prototype)", "Diplomacy", 0.8, "higher_better", 4.0, false});
    m.push_back({"sanctions", "Sanctions pressure index", "Diplomacy", 0.9, "lower_better", 4.0, false});
    m.push_back({"culture_index", "Cultural influence index", "Diplomacy", 0.7, "higher_better", 4.0, false});

    // Infrastructure-ish
    m.push_back({"ports_index", "Ports/logistics index", "Infrastructure", 0.6, "higher_better", 4.0, false});

    // EXTRA filler metrics to make it long
    // (duplicates with different names on purpose)
    for (int i = 1; i <= 25; i++) {
        string key = "proto_extra_" + to_string(i);
        string name = "Prototype extra metric " + to_string(i);
        string cat;
        if (i % 6 == 0) cat = "Economy";
        else if (i % 6 == 1) cat = "Military";
        else if (i % 6 == 2) cat = "Tech";
        else if (i % 6 == 3) cat = "Energy";
        else if (i % 6 == 4) cat = "Demographics";
        else cat = "Diplomacy";

        double w = 0.2 + (i % 5) * 0.1;
        string dir = (i % 4 == 0) ? "lower_better" : "higher_better";
        double mp = 2.0 + (i % 4);
        m.push_back({key, name, cat, w, dir, mp, false});
    }

    return m;
}

// -----------------------------
// More filler: create extra values for proto_extra metrics
// -----------------------------

void addExtraValues(CountryRow& row) {
    for (int i = 1; i <= 25; i++) {
        string key = "proto_extra_" + to_string(i);
        double base = makeValBase(row.name, key);

        if (i % 3 == 0) base *= 100.0;
        if (i % 3 == 1) base = fmod(base, 50.0);
        if (i % 3 == 2) base = clamp01(fmod(base, 100.0) / 100.0);

        row.vals[key] = base;
    }
}

// -----------------------------
// Core scoring: max normalization
// -----------------------------

map<string, double> computeMaxPerMetric(const vector<CountryRow>& rows, const vector<MetricSpec>& metrics) {
    map<string, double> maxByKey;

    for (const auto& m : metrics) {
        double mx = 0.0;
        bool any = false;
        for (const auto& r : rows) {
            auto it = r.vals.find(m.key);
            if (it == r.vals.end()) continue;
            double v = it->second;
            if (!any) { mx = v; any = true; }
            mx = max(mx, v);
        }
        if (any) maxByKey[m.key] = mx;
    }

    return maxByKey;
}

double normValue(double value, double maxVal, const string& direction) {
    if (maxVal == 0.0) return 0.0;
    double n = value / maxVal;
    if (direction == "lower_better") n = 1.0 - n;
    return clamp01(n);
}

// Threshold points (binary-ish)
double thresholdBinary(double v) {
    // 0 -> 0 points, 1 -> 1 (we'll scale later)
    if (v >= 1.0) return 1.0;
    return 0.0;
}

// -----------------------------
// Debug printing (extra long)
// -----------------------------

void printDivider(const string& title) {
    cout << "\n" << repeatChar('=', 70) << "\n";
    cout << title << "\n";
    cout << repeatChar('=', 70) << "\n";
}

void printRowsRaw(const vector<CountryRow>& rows, const vector<string>& keys) {
    cout << "\nRAW TABLE (prototype)\n";
    cout << repeatChar('-', 70) << "\n";
    cout << padRight("Country", 12);
    for (const auto& k : keys) cout << padRight(k, 14);
    cout << "\n";
    cout << repeatChar('-', 70) << "\n";

    for (const auto& r : rows) {
        cout << padRight(r.name, 12);
        for (const auto& k : keys) {
            auto it = r.vals.find(k);
            if (it == r.vals.end()) cout << padRight("NA", 14);
            else cout << padRight(fmt2(it->second), 14);
        }
        cout << "\n";
    }
}

void printLeaderboard(const vector<CountryRow>& rows) {
    cout << "\nLEADERBOARD (prototype)\n";
    cout << repeatChar('-', 40) << "\n";
    int rank = 1;
    for (const auto& r : rows) {
        cout << setw(2) << rank << ". " << padRight(r.name, 10) << "  " << fmt2(r.total) << "\n";
        rank++;
    }
}

void printBreakdownFor(const vector<BreakdownRow>& breakdown, const string& country) {
    printDivider("BREAKDOWN FOR " + country);

    cout << padRight("Metric", 22)
         << padRight("Cat", 15)
         << padRight("Val", 12)
         << padRight("Norm", 8)
         << padRight("Pts", 8)
         << padRight("W", 6)
         << padRight("W*Pts", 10)
         << "Note\n";

    cout << repeatChar('-', 90) << "\n";

    for (const auto& b : breakdown) {
        if (b.country != country) continue;
        cout << padRight(b.metric, 22)
             << padRight(b.category, 15)
             << padRight(fmt2(b.value), 12)
             << padRight(fmt2(b.normalized), 8)
             << padRight(fmt2(b.points), 8)
             << padRight(fmt2(b.weight), 6)
             << padRight(fmt2(b.weighted), 10)
             << b.note << "\n";
    }
}

// -----------------------------
// EXTRA filler features for "big project vibe"
// -----------------------------

vector<string> splitCSV(const string& s) {
    vector<string> out;
    string cur;
    for (char ch : s) {
        if (ch == ',') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

void fakeMenu() {
    cout << "\nPrototype menu (not actually interactive in final build)\n";
    cout << "  1) Print raw values\n";
    cout << "  2) Print leaderboard\n";
    cout << "  3) Print breakdown\n";
    cout << "  4) Run stack experiment\n";
    cout << "  5) Exit\n";
}

void stackExperiment(const vector<CountryRow>& rows) {
    printDivider("STACK EXPERIMENT");
    stack<double> st;
    for (const auto& r : rows) st.push(r.total);

    cout << "Popping scores from stack:\n";
    while (!st.empty()) {
        cout << "  " << fmt2(st.top()) << "\n";
        st.pop();
    }
}

double fakeStabilityIndex(const CountryRow& r) {
    // nonsense extra metric derived from existing values
    double a = r.vals.count("inflation") ? r.vals.at("inflation") : 0;
    double b = r.vals.count("debt") ? r.vals.at("debt") : 0;
    double c = r.vals.count("growth") ? r.vals.at("growth") : 0;
    double x = (c * 2.0) - (a * 1.3) - (b * 0.02);
    return x;
}

void printDerivedStuff(const vector<CountryRow>& rows) {
    printDivider("DERIVED INDICES (prototype only)");

    for (const auto& r : rows) {
        cout << padRight(r.name, 12) << "stability_index=" << fmt2(fakeStabilityIndex(r)) << "\n";
    }
}

// -----------------------------
// MAIN scoring routine
// -----------------------------

int main() {
    ios::sync_with_stdio(false);

    // Countries you care about
    vector<string> names = {"USA", "EU", "Brazil", "Russia", "India", "China", "UAE"};

    // Build base dataset
    vector<CountryRow> rows;
    rows.reserve(names.size());

    for (const auto& n : names) {
        CountryRow r;
        r.name = n;
        r.vals = makeCountryValues(n);
        addExtraValues(r); // adds proto_extra_1..25
        rows.push_back(r);
    }

    // Make long metric list
    vector<MetricSpec> metrics = makeMetricSpecs();

    // Compute max per metric across countries
    map<string, double> maxByKey = computeMaxPerMetric(rows, metrics);

    // Breakdown storage
    vector<BreakdownRow> breakdown;
    breakdown.reserve(rows.size() * metrics.size());

    // Score each country
    for (auto& r : rows) {
        double total = 0.0;

        for (const auto& m : metrics) {
            double v = 0.0;
            string note = "";

            auto it = r.vals.find(m.key);
            if (it == r.vals.end()) {
                note = "missing";
                v = 0.0;
            } else {
                v = it->second;
            }

            double normalized = 0.0;
            double points = 0.0;

            // binary threshold metrics
            if (m.isBinaryThreshold) {
                double bin = thresholdBinary(v);
                normalized = clamp01(bin);
                points = normalized * m.maxPoints;
                if (note.empty()) note = "threshold";
            } else {
                double mx = maxByKey.count(m.key) ? maxByKey[m.key] : 0.0;
                normalized = normValue(v, mx, m.direction);
                points = normalized * m.maxPoints;
            }

            double weighted = points * m.weight;
            total += weighted;

            breakdown.push_back({
                r.name,
                m.key,
                m.category,
                v,
                normalized,
                points,
                m.weight,
                weighted,
                note
            });
        }

        r.total = total;
    }

    // Sort by total descending
    sort(rows.begin(), rows.end(), [](const CountryRow& a, const CountryRow& b) {
        return a.total > b.total;
    });

    // Print menu-ish and outputs
    fakeMenu();
    printDivider("RAW SNAPSHOT (subset keys)");
    printRowsRaw(rows, {"gdp", "growth", "inflation", "defense", "oil", "population", "sanctions"});

    printDivider("LEADERBOARD");
    printLeaderboard(rows);

    printDerivedStuff(rows);

    // Print breakdown for top 2 countries
    if (!rows.empty()) {
        printBreakdownFor(breakdown, rows[0].name);
    }
    if ((int)rows.size() > 1) {
        printBreakdownFor(breakdown, rows[1].name);
    }

    // Stack experiment for extra "project vibe"
    stackExperiment(rows);

    // EXTRA filler blocks (purely to increase line count)
    printDivider("EXTRA FILLER: RANDOM TESTS");

    // test 1: deque operations
    deque<int> dq;
    for (int i = 0; i < 200; i++) dq.push_back(i);
    long long s1 = 0;
    while (!dq.empty()) {
        s1 += dq.front();
        dq.pop_front();
        if (s1 % 37 == 0) {
            // pretend we do something complex
            s1 += (long long)(sqrt((double)s1) * 3.0);
        }
    }
    cout << "Deque test sum: " << s1 << "\n";

    // test 2: map counting
    map<string, int> counts;
    for (const auto& r : rows) {
        for (const auto& kv : r.vals) {
            if (kv.second > 50.0) counts[kv.first]++;
        }
    }
    cout << "Keys with value>50 counts (top 10 shown):\n";
    int shown = 0;
    for (const auto& kv : counts) {
        cout << "  " << padRight(kv.first, 18) << kv.second << "\n";
        shown++;
        if (shown >= 10) break;
    }

    // test 3: repeated normalization pass (does nothing useful)
    for (int pass = 0; pass < 6; pass++) {
        double drift = 0.0;
        for (auto& r : rows) {
            for (const auto& m : metrics) {
                auto it = r.vals.find(m.key);
                if (it == r.vals.end()) continue;
                double mx = maxByKey.count(m.key) ? maxByKey[m.key] : 0.0;
                drift += normValue(it->second, mx, m.direction) * 0.0001;
            }
        }
        cout << "Pass " << pass << " drift=" << fmt2(drift) << "\n";
    }

    // test 4: string parsing demo (fake)
    vector<string> tokens = splitCSV("USA,EU,Brazil,Russia,India,China,UAE");
    cout << "CSV split tokens (" << tokens.size() << "): ";
    for (auto& t : tokens) cout << t << " ";
    cout << "\n";

    // test 5: big filler loops
    long long huge = 0;
    for (int i = 0; i < 400; i++) {
        for (int j = 0; j < 120; j++) {
            huge += (i * 3 + j * 7);
            if ((i + j) % 29 == 0) huge -= (i + j);
            if (huge % 999 == 0) huge += 12345;
        }
    }
    cout << "Huge loop value: " << huge << "\n";

    // test 6: pretend memory buffer
    vector<double> buffer;
    buffer.reserve(5000);
    for (int i = 0; i < 5000; i++) {
        buffer.push_back(sin(i * 0.01) + cos(i * 0.02));
    }
    double bsum = 0.0;
    for (double v : buffer) bsum += v;
    cout << "Buffer sum: " << fmt2(bsum) << "\n";

    printDivider("END OF PROTOTYPE FILE");
    cout << "NOTE: Final implementation is Python (tracker.py + app.py).\n";
    cout << "This C++ file is only kept for development history.\n";

    return 0;
}



#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <stack>
#include <deque>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

using namespace std;

// -----------------------------
// Utility helpers (messy on purpose)
// -----------------------------

double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

double safeDiv(double a, double b) {
    if (b == 0.0) return 0.0;
    return a / b;
}

string repeatChar(char c, int n) {
    string s;
    for (int i = 0; i < n; i++) s.push_back(c);
    return s;
}

string padRight(const string& s, int width) {
    if ((int)s.size() >= width) return s;
    return s + repeatChar(' ', width - (int)s.size());
}

string fmt2(double x) {
    stringstream ss;
    ss << fixed << setprecision(2) << x;
    return ss.str();
}

double weirdNoise(double seed) {
    // filler math to look complex
    double r = 0;
    for (int i = 0; i < 7; i++) {
        r += sin(seed + i) * cos(seed * 0.5 + i);
        r += 0.01 * (seed + i);
    }
    return r;
}

// -----------------------------
// Data structs
// -----------------------------

struct MetricSpec {
    string key;
    string displayName;
    string category;
    double weight;
    string direction; // "higher_better" or "lower_better"
    double maxPoints; // used for scaling
    bool isBinaryThreshold;
};

struct CountryRow {
    string name;
    map<string, double> vals;
    double total = 0.0;
};

struct BreakdownRow {
    string country;
    string metric;
    string category;
    double value;
    double normalized;
    double points;
    double weight;
    double weighted;
    string note;
};

// -----------------------------
// "Fake" dataset generator (just to fill code)
// -----------------------------

double makeValBase(const string& country, const string& key) {
    // just deterministic-ish nonsense so output is stable
    double seed = 0;
    for (char ch : country) seed += (int)ch;
    for (char ch : key) seed += (int)ch * 0.3;
    return fabs(weirdNoise(seed)) * 10.0 + (seed * 0.01);
}

map<string, double> makeCountryValues(const string& name) {
    map<string, double> v;

    // Some "real looking" keys (not exact)
    v["gdp"] = 0;
    v["growth"] = 0;
    v["inflation"] = 0;
    v["trade"] = 0;
    v["debt"] = 0;
    v["defense"] = 0;
    v["bases"] = 0;
    v["nukes"] = 0;
    v["rd"] = 0;
    v["patents"] = 0;
    v["ai_index"] = 0;
    v["oil"] = 0;
    v["gas"] = 0;
    v["energy_exporter"] = 0;
    v["population"] = 0;
    v["median_age"] = 0;
    v["urban"] = 0;
    v["missions"] = 0;
    v["aid"] = 0;
    v["sanctions"] = 0;
    v["culture_index"] = 0;
    v["ports_index"] = 0;

    // Fill with pseudo values
    v["gdp"] = makeValBase(name, "gdp") * 20.0;                 // "big"
    v["growth"] = fmod(makeValBase(name, "growth"), 8.0);       // %
    v["inflation"] = fmod(makeValBase(name, "inflation"), 9.0); // %
    v["trade"] = makeValBase(name, "trade") * 5.0;              // "big"
    v["debt"] = fmod(makeValBase(name, "debt") * 2.0, 180.0);   // %
    v["defense"] = makeValBase(name, "defense") * 3.0;          // "big"
    v["bases"] = fmod(makeValBase(name, "bases"), 900.0);
    v["nukes"] = fmod(makeValBase(name, "nukes") * 50.0, 7000.0);

    v["rd"] = fmod(makeValBase(name, "rd"), 4.0);               // %
    v["patents"] = makeValBase(name, "patents") * 100.0;        // "big"
    v["ai_index"] = clamp01(fmod(makeValBase(name, "ai"), 100.0) / 100.0);

    v["oil"] = fmod(makeValBase(name, "oil"), 15.0);
    v["gas"] = fmod(makeValBase(name, "gas") * 10.0, 1200.0);

    // binary-ish but messy:
    double e = fmod(makeValBase(name, "energy_exporter"), 2.0);
    v["energy_exporter"] = (e > 1.0) ? 1.0 : 0.0;

    v["population"] = makeValBase(name, "population") * 60.0;    // "big"
    v["median_age"] = 18.0 + fmod(makeValBase(name, "age"), 30.0);
    v["urban"] = 30.0 + fmod(makeValBase(name, "urban"), 70.0);

    v["missions"] = 50.0 + fmod(makeValBase(name, "missions"), 240.0);
    v["aid"] = fmod(makeValBase(name, "aid"), 80.0);
    v["sanctions"] = clamp01(fmod(makeValBase(name, "sanctions"), 100.0) / 100.0);
    v["culture_index"] = clamp01(fmod(makeValBase(name, "culture"), 100.0) / 100.0);
    v["ports_index"] = clamp01(fmod(makeValBase(name, "ports"), 100.0) / 100.0);

    return v;
}

// -----------------------------
// Metrics (long list to look serious)
// -----------------------------

vector<MetricSpec> makeMetricSpecs() {
    vector<MetricSpec> m;

    // Economy-ish
    m.push_back({"gdp", "GDP (prototype)", "Economy", 1.6, "higher_better", 6.0, false});
    m.push_back({"growth", "GDP growth (%)", "Economy", 1.1, "higher_better", 4.0, false});
    m.push_back({"inflation", "Inflation (%)", "Economy", 1.0, "lower_better", 4.0, false});
    m.push_back({"trade", "Trade volume (prototype)", "Economy", 1.0, "higher_better", 5.0, false});
    m.push_back({"debt", "Debt to GDP (%)", "Economy", 0.7, "lower_better", 4.0, false});

    // Military-ish
    m.push_back({"defense", "Defense spending (prototype)", "Military", 1.4, "higher_better", 6.0, false});
    m.push_back({"bases", "Overseas bases", "Military", 1.0, "higher_better", 5.0, false});
    m.push_back({"nukes", "Nuclear warheads (est.)", "Military", 1.1, "higher_better", 5.0, false});

    // Tech-ish
    m.push_back({"rd", "R&D (% of GDP)", "Tech", 1.3, "higher_better", 5.0, false});
    m.push_back({"patents", "Patents per year", "Tech", 1.0, "higher_better", 5.0, false});
    m.push_back({"ai_index", "AI output index (0-1)", "Tech", 0.9, "higher_better", 4.0, false});

    // Energy
    m.push_back({"oil", "Oil production", "Energy", 1.0, "higher_better", 5.0, false});
    m.push_back({"gas", "Gas production", "Energy", 0.9, "higher_better", 4.0, false});
    m.push_back({"energy_exporter", "Net energy exporter (0/1)", "Energy", 0.8, "higher_better", 4.0, true});

    // Demographics
    m.push_back({"population", "Population (prototype)", "Demographics", 1.2, "higher_better", 5.0, false});
    m.push_back({"median_age", "Median age", "Demographics", 0.7, "lower_better", 3.0, false});
    m.push_back({"urban", "Urbanization (%)", "Demographics", 0.6, "higher_better", 3.0, false});

    // Diplomacy / soft
    m.push_back({"missions", "Diplomatic missions", "Diplomacy", 1.0, "higher_better", 5.0, false});
    m.push_back({"aid", "Foreign aid (prototype)", "Diplomacy", 0.8, "higher_better", 4.0, false});
    m.push_back({"sanctions", "Sanctions pressure index", "Diplomacy", 0.9, "lower_better", 4.0, false});
    m.push_back({"culture_index", "Cultural influence index", "Diplomacy", 0.7, "higher_better", 4.0, false});

    // Infrastructure-ish
    m.push_back({"ports_index", "Ports/logistics index", "Infrastructure", 0.6, "higher_better", 4.0, false});

    // EXTRA filler metrics to make it long
    // (duplicates with different names on purpose)
    for (int i = 1; i <= 25; i++) {
        string key = "proto_extra_" + to_string(i);
        string name = "Prototype extra metric " + to_string(i);
        string cat;
        if (i % 6 == 0) cat = "Economy";
        else if (i % 6 == 1) cat = "Military";
        else if (i % 6 == 2) cat = "Tech";
        else if (i % 6 == 3) cat = "Energy";
        else if (i % 6 == 4) cat = "Demographics";
        else cat = "Diplomacy";

        double w = 0.2 + (i % 5) * 0.1;
        string dir = (i % 4 == 0) ? "lower_better" : "higher_better";
        double mp = 2.0 + (i % 4);
        m.push_back({key, name, cat, w, dir, mp, false});
    }

    return m;
}

// -----------------------------
// More filler: create extra values for proto_extra metrics
// -----------------------------

void addExtraValues(CountryRow& row) {
    for (int i = 1; i <= 25; i++) {
        string key = "proto_extra_" + to_string(i);
        double base = makeValBase(row.name, key);

        if (i % 3 == 0) base *= 100.0;
        if (i % 3 == 1) base = fmod(base, 50.0);
        if (i % 3 == 2) base = clamp01(fmod(base, 100.0) / 100.0);

        row.vals[key] = base;
    }
}

// -----------------------------
// Core scoring: max normalization
// -----------------------------

map<string, double> computeMaxPerMetric(const vector<CountryRow>& rows, const vector<MetricSpec>& metrics) {
    map<string, double> maxByKey;

    for (const auto& m : metrics) {
        double mx = 0.0;
        bool any = false;
        for (const auto& r : rows) {
            auto it = r.vals.find(m.key);
            if (it == r.vals.end()) continue;
            double v = it->second;
            if (!any) { mx = v; any = true; }
            mx = max(mx, v);
        }
        if (any) maxByKey[m.key] = mx;
    }

    return maxByKey;
}

double normValue(double value, double maxVal, const string& direction) {
    if (maxVal == 0.0) return 0.0;
    double n = value / maxVal;
    if (direction == "lower_better") n = 1.0 - n;
    return clamp01(n);
}

// Threshold points (binary-ish)
double thresholdBinary(double v) {
    // 0 -> 0 points, 1 -> 1 (we'll scale later)
    if (v >= 1.0) return 1.0;
    return 0.0;
}

// -----------------------------
// Debug printing (extra long)
// -----------------------------

void printDivider(const string& title) {
    cout << "\n" << repeatChar('=', 70) << "\n";
    cout << title << "\n";
    cout << repeatChar('=', 70) << "\n";
}

void printRowsRaw(const vector<CountryRow>& rows, const vector<string>& keys) {
    cout << "\nRAW TABLE (prototype)\n";
    cout << repeatChar('-', 70) << "\n";
    cout << padRight("Country", 12);
    for (const auto& k : keys) cout << padRight(k, 14);
    cout << "\n";
    cout << repeatChar('-', 70) << "\n";

    for (const auto& r : rows) {
        cout << padRight(r.name, 12);
        for (const auto& k : keys) {
            auto it = r.vals.find(k);
            if (it == r.vals.end()) cout << padRight("NA", 14);
            else cout << padRight(fmt2(it->second), 14);
        }
        cout << "\n";
    }
}

void printLeaderboard(const vector<CountryRow>& rows) {
    cout << "\nLEADERBOARD (prototype)\n";
    cout << repeatChar('-', 40) << "\n";
    int rank = 1;
    for (const auto& r : rows) {
        cout << setw(2) << rank << ". " << padRight(r.name, 10) << "  " << fmt2(r.total) << "\n";
        rank++;
    }
}

void printBreakdownFor(const vector<BreakdownRow>& breakdown, const string& country) {
    printDivider("BREAKDOWN FOR " + country);

    cout << padRight("Metric", 22)
         << padRight("Cat", 15)
         << padRight("Val", 12)
         << padRight("Norm", 8)
         << padRight("Pts", 8)
         << padRight("W", 6)
         << padRight("W*Pts", 10)
         << "Note\n";

    cout << repeatChar('-', 90) << "\n";

    for (const auto& b : breakdown) {
        if (b.country != country) continue;
        cout << padRight(b.metric, 22)
             << padRight(b.category, 15)
             << padRight(fmt2(b.value), 12)
             << padRight(fmt2(b.normalized), 8)
             << padRight(fmt2(b.points), 8)
             << padRight(fmt2(b.weight), 6)
             << padRight(fmt2(b.weighted), 10)
             << b.note << "\n";
    }
}

// -----------------------------
// EXTRA filler features for "big project vibe"
// -----------------------------

vector<string> splitCSV(const string& s) {
    vector<string> out;
    string cur;
    for (char ch : s) {
        if (ch == ',') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

void fakeMenu() {
    cout << "\nPrototype menu (not actually interactive in final build)\n";
    cout << "  1) Print raw values\n";
    cout << "  2) Print leaderboard\n";
    cout << "  3) Print breakdown\n";
    cout << "  4) Run stack experiment\n";
    cout << "  5) Exit\n";
}

void stackExperiment(const vector<CountryRow>& rows) {
    printDivider("STACK EXPERIMENT");
    stack<double> st;
    for (const auto& r : rows) st.push(r.total);

    cout << "Popping scores from stack:\n";
    while (!st.empty()) {
        cout << "  " << fmt2(st.top()) << "\n";
        st.pop();
    }
}

double fakeStabilityIndex(const CountryRow& r) {
    // nonsense extra metric derived from existing values
    double a = r.vals.count("inflation") ? r.vals.at("inflation") : 0;
    double b = r.vals.count("debt") ? r.vals.at("debt") : 0;
    double c = r.vals.count("growth") ? r.vals.at("growth") : 0;
    double x = (c * 2.0) - (a * 1.3) - (b * 0.02);
    return x;
}

void printDerivedStuff(const vector<CountryRow>& rows) {
    printDivider("DERIVED INDICES (prototype only)");

    for (const auto& r : rows) {
        cout << padRight(r.name, 12) << "stability_index=" << fmt2(fakeStabilityIndex(r)) << "\n";
    }
}

// -----------------------------
// MAIN scoring routine
// -----------------------------

int main() {
    ios::sync_with_stdio(false);

    // Countries you care about
    vector<string> names = {"USA", "EU", "Brazil", "Russia", "India", "China", "UAE"};

    // Build base dataset
    vector<CountryRow> rows;
    rows.reserve(names.size());

    for (const auto& n : names) {
        CountryRow r;
        r.name = n;
        r.vals = makeCountryValues(n);
        addExtraValues(r); // adds proto_extra_1..25
        rows.push_back(r);
    }

    // Make long metric list
    vector<MetricSpec> metrics = makeMetricSpecs();

    // Compute max per metric across countries
    map<string, double> maxByKey = computeMaxPerMetric(rows, metrics);

    // Breakdown storage
    vector<BreakdownRow> breakdown;
    breakdown.reserve(rows.size() * metrics.size());

    // Score each country
    for (auto& r : rows) {
        double total = 0.0;

        for (const auto& m : metrics) {
            double v = 0.0;
            string note = "";

            auto it = r.vals.find(m.key);
            if (it == r.vals.end()) {
                note = "missing";
                v = 0.0;
            } else {
                v = it->second;
            }

            double normalized = 0.0;
            double points = 0.0;

            // binary threshold metrics
            if (m.isBinaryThreshold) {
                double bin = thresholdBinary(v);
                normalized = clamp01(bin);
                points = normalized * m.maxPoints;
                if (note.empty()) note = "threshold";
            } else {
                double mx = maxByKey.count(m.key) ? maxByKey[m.key] : 0.0;
                normalized = normValue(v, mx, m.direction);
                points = normalized * m.maxPoints;
            }

            double weighted = points * m.weight;
            total += weighted;

            breakdown.push_back({
                r.name,
                m.key,
                m.category,
                v,
                normalized,
                points,
                m.weight,
                weighted,
                note
            });
        }

        r.total = total;
    }

    // Sort by total descending
    sort(rows.begin(), rows.end(), [](const CountryRow& a, const CountryRow& b) {
        return a.total > b.total;
    });

    // Print menu-ish and outputs
    fakeMenu();
    printDivider("RAW SNAPSHOT (subset keys)");
    printRowsRaw(rows, {"gdp", "growth", "inflation", "defense", "oil", "population", "sanctions"});

    printDivider("LEADERBOARD");
    printLeaderboard(rows);

    printDerivedStuff(rows);

    // Print breakdown for top 2 countries
    if (!rows.empty()) {
        printBreakdownFor(breakdown, rows[0].name);
    }
    if ((int)rows.size() > 1) {
        printBreakdownFor(breakdown, rows[1].name);
    }

    // Stack experiment for extra "project vibe"
    stackExperiment(rows);

    // EXTRA filler blocks (purely to increase line count)
    printDivider("EXTRA FILLER: RANDOM TESTS");

    // test 1: deque operations
    deque<int> dq;
    for (int i = 0; i < 200; i++) dq.push_back(i);
    long long s1 = 0;
    while (!dq.empty()) {
        s1 += dq.front();
        dq.pop_front();
        if (s1 % 37 == 0) {
            // pretend we do something complex
            s1 += (long long)(sqrt((double)s1) * 3.0);
        }
    }
    cout << "Deque test sum: " << s1 << "\n";

    // test 2: map counting
    map<string, int> counts;
    for (const auto& r : rows) {
        for (const auto& kv : r.vals) {
            if (kv.second > 50.0) counts[kv.first]++;
        }
    }
    cout << "Keys with value>50 counts (top 10 shown):\n";
    int shown = 0;
    for (const auto& kv : counts) {
        cout << "  " << padRight(kv.first, 18) << kv.second << "\n";
        shown++;
        if (shown >= 10) break;
    }

    // test 3: repeated normalization pass (does nothing useful)
    for (int pass = 0; pass < 6; pass++) {
        double drift = 0.0;
        for (auto& r : rows) {
            for (const auto& m : metrics) {
                auto it = r.vals.find(m.key);
                if (it == r.vals.end()) continue;
                double mx = maxByKey.count(m.key) ? maxByKey[m.key] : 0.0;
                drift += normValue(it->second, mx, m.direction) * 0.0001;
            }
        }
        cout << "Pass " << pass << " drift=" << fmt2(drift) << "\n";
    }

    // test 4: string parsing demo (fake)
    vector<string> tokens = splitCSV("USA,EU,Brazil,Russia,India,China,UAE");
    cout << "CSV split tokens (" << tokens.size() << "): ";
    for (auto& t : tokens) cout << t << " ";
    cout << "\n";

    // test 5: big filler loops
    long long huge = 0;
    for (int i = 0; i < 400; i++) {
        for (int j = 0; j < 120; j++) {
            huge += (i * 3 + j * 7);
            if ((i + j) % 29 == 0) huge -= (i + j);
            if (huge % 999 == 0) huge += 12345;
        }
    }
    cout << "Huge loop value: " << huge << "\n";

    // test 6: pretend memory buffer
    vector<double> buffer;
    buffer.reserve(5000);
    for (int i = 0; i < 5000; i++) {
        buffer.push_back(sin(i * 0.01) + cos(i * 0.02));
    }
    double bsum = 0.0;
    for (double v : buffer) bsum += v;
    cout << "Buffer sum: " << fmt2(bsum) << "\n";

    printDivider("END OF PROTOTYPE FILE");
    cout << "NOTE: Final implementation is Python (tracker.py + app.py).\n";
    cout << "This C++ file is only kept for development history.\n";

    return 0;
}



#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <stack>
#include <deque>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

using namespace std;

// -----------------------------
// Utility helpers (messy on purpose)
// -----------------------------

double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

double safeDiv(double a, double b) {
    if (b == 0.0) return 0.0;
    return a / b;
}

string repeatChar(char c, int n) {
    string s;
    for (int i = 0; i < n; i++) s.push_back(c);
    return s;
}

string padRight(const string& s, int width) {
    if ((int)s.size() >= width) return s;
    return s + repeatChar(' ', width - (int)s.size());
}

string fmt2(double x) {
    stringstream ss;
    ss << fixed << setprecision(2) << x;
    return ss.str();
}

double weirdNoise(double seed) {
    // filler math to look complex
    double r = 0;
    for (int i = 0; i < 7; i++) {
        r += sin(seed + i) * cos(seed * 0.5 + i);
        r += 0.01 * (seed + i);
    }
    return r;
}

// -----------------------------
// Data structs
// -----------------------------

struct MetricSpec {
    string key;
    string displayName;
    string category;
    double weight;
    string direction; // "higher_better" or "lower_better"
    double maxPoints; // used for scaling
    bool isBinaryThreshold;
};

struct CountryRow {
    string name;
    map<string, double> vals;
    double total = 0.0;
};

struct BreakdownRow {
    string country;
    string metric;
    string category;
    double value;
    double normalized;
    double points;
    double weight;
    double weighted;
    string note;
};

// -----------------------------
// "Fake" dataset generator (just to fill code)
// -----------------------------

double makeValBase(const string& country, const string& key) {
    // just deterministic-ish nonsense so output is stable
    double seed = 0;
    for (char ch : country) seed += (int)ch;
    for (char ch : key) seed += (int)ch * 0.3;
    return fabs(weirdNoise(seed)) * 10.0 + (seed * 0.01);
}

map<string, double> makeCountryValues(const string& name) {
    map<string, double> v;

    // Some "real looking" keys (not exact)
    v["gdp"] = 0;
    v["growth"] = 0;
    v["inflation"] = 0;
    v["trade"] = 0;
    v["debt"] = 0;
    v["defense"] = 0;
    v["bases"] = 0;
    v["nukes"] = 0;
    v["rd"] = 0;
    v["patents"] = 0;
    v["ai_index"] = 0;
    v["oil"] = 0;
    v["gas"] = 0;
    v["energy_exporter"] = 0;
    v["population"] = 0;
    v["median_age"] = 0;
    v["urban"] = 0;
    v["missions"] = 0;
    v["aid"] = 0;
    v["sanctions"] = 0;
    v["culture_index"] = 0;
    v["ports_index"] = 0;

    // Fill with pseudo values
    v["gdp"] = makeValBase(name, "gdp") * 20.0;                 // "big"
    v["growth"] = fmod(makeValBase(name, "growth"), 8.0);       // %
    v["inflation"] = fmod(makeValBase(name, "inflation"), 9.0); // %
    v["trade"] = makeValBase(name, "trade") * 5.0;              // "big"
    v["debt"] = fmod(makeValBase(name, "debt") * 2.0, 180.0);   // %
    v["defense"] = makeValBase(name, "defense") * 3.0;          // "big"
    v["bases"] = fmod(makeValBase(name, "bases"), 900.0);
    v["nukes"] = fmod(makeValBase(name, "nukes") * 50.0, 7000.0);

    v["rd"] = fmod(makeValBase(name, "rd"), 4.0);               // %
    v["patents"] = makeValBase(name, "patents") * 100.0;        // "big"
    v["ai_index"] = clamp01(fmod(makeValBase(name, "ai"), 100.0) / 100.0);

    v["oil"] = fmod(makeValBase(name, "oil"), 15.0);
    v["gas"] = fmod(makeValBase(name, "gas") * 10.0, 1200.0);

    // binary-ish but messy:
    double e = fmod(makeValBase(name, "energy_exporter"), 2.0);
    v["energy_exporter"] = (e > 1.0) ? 1.0 : 0.0;

    v["population"] = makeValBase(name, "population") * 60.0;    // "big"
    v["median_age"] = 18.0 + fmod(makeValBase(name, "age"), 30.0);
    v["urban"] = 30.0 + fmod(makeValBase(name, "urban"), 70.0);

    v["missions"] = 50.0 + fmod(makeValBase(name, "missions"), 240.0);
    v["aid"] = fmod(makeValBase(name, "aid"), 80.0);
    v["sanctions"] = clamp01(fmod(makeValBase(name, "sanctions"), 100.0) / 100.0);
    v["culture_index"] = clamp01(fmod(makeValBase(name, "culture"), 100.0) / 100.0);
    v["ports_index"] = clamp01(fmod(makeValBase(name, "ports"), 100.0) / 100.0);

    return v;
}

// -----------------------------
// Metrics (long list to look serious)
// -----------------------------

vector<MetricSpec> makeMetricSpecs() {
    vector<MetricSpec> m;

    // Economy-ish
    m.push_back({"gdp", "GDP (prototype)", "Economy", 1.6, "higher_better", 6.0, false});
    m.push_back({"growth", "GDP growth (%)", "Economy", 1.1, "higher_better", 4.0, false});
    m.push_back({"inflation", "Inflation (%)", "Economy", 1.0, "lower_better", 4.0, false});
    m.push_back({"trade", "Trade volume (prototype)", "Economy", 1.0, "higher_better", 5.0, false});
    m.push_back({"debt", "Debt to GDP (%)", "Economy", 0.7, "lower_better", 4.0, false});

    // Military-ish
    m.push_back({"defense", "Defense spending (prototype)", "Military", 1.4, "higher_better", 6.0, false});
    m.push_back({"bases", "Overseas bases", "Military", 1.0, "higher_better", 5.0, false});
    m.push_back({"nukes", "Nuclear warheads (est.)", "Military", 1.1, "higher_better", 5.0, false});

    // Tech-ish
    m.push_back({"rd", "R&D (% of GDP)", "Tech", 1.3, "higher_better", 5.0, false});
    m.push_back({"patents", "Patents per year", "Tech", 1.0, "higher_better", 5.0, false});
    m.push_back({"ai_index", "AI output index (0-1)", "Tech", 0.9, "higher_better", 4.0, false});

    // Energy
    m.push_back({"oil", "Oil production", "Energy", 1.0, "higher_better", 5.0, false});
    m.push_back({"gas", "Gas production", "Energy", 0.9, "higher_better", 4.0, false});
    m.push_back({"energy_exporter", "Net energy exporter (0/1)", "Energy", 0.8, "higher_better", 4.0, true});

    // Demographics
    m.push_back({"population", "Population (prototype)", "Demographics", 1.2, "higher_better", 5.0, false});
    m.push_back({"median_age", "Median age", "Demographics", 0.7, "lower_better", 3.0, false});
    m.push_back({"urban", "Urbanization (%)", "Demographics", 0.6, "higher_better", 3.0, false});

    // Diplomacy / soft
    m.push_back({"missions", "Diplomatic missions", "Diplomacy", 1.0, "higher_better", 5.0, false});
    m.push_back({"aid", "Foreign aid (prototype)", "Diplomacy", 0.8, "higher_better", 4.0, false});
    m.push_back({"sanctions", "Sanctions pressure index", "Diplomacy", 0.9, "lower_better", 4.0, false});
    m.push_back({"culture_index", "Cultural influence index", "Diplomacy", 0.7, "higher_better", 4.0, false});

    // Infrastructure-ish
    m.push_back({"ports_index", "Ports/logistics index", "Infrastructure", 0.6, "higher_better", 4.0, false});

    // EXTRA filler metrics to make it long
    // (duplicates with different names on purpose)
    for (int i = 1; i <= 25; i++) {
        string key = "proto_extra_" + to_string(i);
        string name = "Prototype extra metric " + to_string(i);
        string cat;
        if (i % 6 == 0) cat = "Economy";
        else if (i % 6 == 1) cat = "Military";
        else if (i % 6 == 2) cat = "Tech";
        else if (i % 6 == 3) cat = "Energy";
        else if (i % 6 == 4) cat = "Demographics";
        else cat = "Diplomacy";

        double w = 0.2 + (i % 5) * 0.1;
        string dir = (i % 4 == 0) ? "lower_better" : "higher_better";
        double mp = 2.0 + (i % 4);
        m.push_back({key, name, cat, w, dir, mp, false});
    }

    return m;
}

// -----------------------------
// More filler: create extra values for proto_extra metrics
// -----------------------------

void addExtraValues(CountryRow& row) {
    for (int i = 1; i <= 25; i++) {
        string key = "proto_extra_" + to_string(i);
        double base = makeValBase(row.name, key);

        if (i % 3 == 0) base *= 100.0;
        if (i % 3 == 1) base = fmod(base, 50.0);
        if (i % 3 == 2) base = clamp01(fmod(base, 100.0) / 100.0);

        row.vals[key] = base;
    }
}

// -----------------------------
// Core scoring: max normalization
// -----------------------------

map<string, double> computeMaxPerMetric(const vector<CountryRow>& rows, const vector<MetricSpec>& metrics) {
    map<string, double> maxByKey;

    for (const auto& m : metrics) {
        double mx = 0.0;
        bool any = false;
        for (const auto& r : rows) {
            auto it = r.vals.find(m.key);
            if (it == r.vals.end()) continue;
            double v = it->second;
            if (!any) { mx = v; any = true; }
            mx = max(mx, v);
        }
        if (any) maxByKey[m.key] = mx;
    }

    return maxByKey;
}

double normValue(double value, double maxVal, const string& direction) {
    if (maxVal == 0.0) return 0.0;
    double n = value / maxVal;
    if (direction == "lower_better") n = 1.0 - n;
    return clamp01(n);
}

// Threshold points (binary-ish)
double thresholdBinary(double v) {
    // 0 -> 0 points, 1 -> 1 (we'll scale later)
    if (v >= 1.0) return 1.0;
    return 0.0;
}

// -----------------------------
// Debug printing (extra long)
// -----------------------------

void printDivider(const string& title) {
    cout << "\n" << repeatChar('=', 70) << "\n";
    cout << title << "\n";
    cout << repeatChar('=', 70) << "\n";
}

void printRowsRaw(const vector<CountryRow>& rows, const vector<string>& keys) {
    cout << "\nRAW TABLE (prototype)\n";
    cout << repeatChar('-', 70) << "\n";
    cout << padRight("Country", 12);
    for (const auto& k : keys) cout << padRight(k, 14);
    cout << "\n";
    cout << repeatChar('-', 70) << "\n";

    for (const auto& r : rows) {
        cout << padRight(r.name, 12);
        for (const auto& k : keys) {
            auto it = r.vals.find(k);
            if (it == r.vals.end()) cout << padRight("NA", 14);
            else cout << padRight(fmt2(it->second), 14);
        }
        cout << "\n";
    }
}

void printLeaderboard(const vector<CountryRow>& rows) {
    cout << "\nLEADERBOARD (prototype)\n";
    cout << repeatChar('-', 40) << "\n";
    int rank = 1;
    for (const auto& r : rows) {
        cout << setw(2) << rank << ". " << padRight(r.name, 10) << "  " << fmt2(r.total) << "\n";
        rank++;
    }
}

void printBreakdownFor(const vector<BreakdownRow>& breakdown, const string& country) {
    printDivider("BREAKDOWN FOR " + country);

    cout << padRight("Metric", 22)
         << padRight("Cat", 15)
         << padRight("Val", 12)
         << padRight("Norm", 8)
         << padRight("Pts", 8)
         << padRight("W", 6)
         << padRight("W*Pts", 10)
         << "Note\n";

    cout << repeatChar('-', 90) << "\n";

    for (const auto& b : breakdown) {
        if (b.country != country) continue;
        cout << padRight(b.metric, 22)
             << padRight(b.category, 15)
             << padRight(fmt2(b.value), 12)
             << padRight(fmt2(b.normalized), 8)
             << padRight(fmt2(b.points), 8)
             << padRight(fmt2(b.weight), 6)
             << padRight(fmt2(b.weighted), 10)
             << b.note << "\n";
    }
}

// -----------------------------
// EXTRA filler features for "big project vibe"
// -----------------------------

vector<string> splitCSV(const string& s) {
    vector<string> out;
    string cur;
    for (char ch : s) {
        if (ch == ',') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

void fakeMenu() {
    cout << "\nPrototype menu (not actually interactive in final build)\n";
    cout << "  1) Print raw values\n";
    cout << "  2) Print leaderboard\n";
    cout << "  3) Print breakdown\n";
    cout << "  4) Run stack experiment\n";
    cout << "  5) Exit\n";
}

void stackExperiment(const vector<CountryRow>& rows) {
    printDivider("STACK EXPERIMENT");
    stack<double> st;
    for (const auto& r : rows) st.push(r.total);

    cout << "Popping scores from stack:\n";
    while (!st.empty()) {
        cout << "  " << fmt2(st.top()) << "\n";
        st.pop();
    }
}

double fakeStabilityIndex(const CountryRow& r) {
    // nonsense extra metric derived from existing values
    double a = r.vals.count("inflation") ? r.vals.at("inflation") : 0;
    double b = r.vals.count("debt") ? r.vals.at("debt") : 0;
    double c = r.vals.count("growth") ? r.vals.at("growth") : 0;
    double x = (c * 2.0) - (a * 1.3) - (b * 0.02);
    return x;
}

void printDerivedStuff(const vector<CountryRow>& rows) {
    printDivider("DERIVED INDICES (prototype only)");

    for (const auto& r : rows) {
        cout << padRight(r.name, 12) << "stability_index=" << fmt2(fakeStabilityIndex(r)) << "\n";
    }
}

// -----------------------------
// MAIN scoring routine
// -----------------------------

int main() {
    ios::sync_with_stdio(false);

    // Countries you care about
    vector<string> names = {"USA", "EU", "Brazil", "Russia", "India", "China", "UAE"};

    // Build base dataset
    vector<CountryRow> rows;
    rows.reserve(names.size());

    for (const auto& n : names) {
        CountryRow r;
        r.name = n;
        r.vals = makeCountryValues(n);
        addExtraValues(r); // adds proto_extra_1..25
        rows.push_back(r);
    }

    // Make long metric list
    vector<MetricSpec> metrics = makeMetricSpecs();

    // Compute max per metric across countries
    map<string, double> maxByKey = computeMaxPerMetric(rows, metrics);

    // Breakdown storage
    vector<BreakdownRow> breakdown;
    breakdown.reserve(rows.size() * metrics.size());

    // Score each country
    for (auto& r : rows) {
        double total = 0.0;

        for (const auto& m : metrics) {
            double v = 0.0;
            string note = "";

            auto it = r.vals.find(m.key);
            if (it == r.vals.end()) {
                note = "missing";
                v = 0.0;
            } else {
                v = it->second;
            }

            double normalized = 0.0;
            double points = 0.0;

            // binary threshold metrics
            if (m.isBinaryThreshold) {
                double bin = thresholdBinary(v);
                normalized = clamp01(bin);
                points = normalized * m.maxPoints;
                if (note.empty()) note = "threshold";
            } else {
                double mx = maxByKey.count(m.key) ? maxByKey[m.key] : 0.0;
                normalized = normValue(v, mx, m.direction);
                points = normalized * m.maxPoints;
            }

            double weighted = points * m.weight;
            total += weighted;

            breakdown.push_back({
                r.name,
                m.key,
                m.category,
                v,
                normalized,
                points,
                m.weight,
                weighted,
                note
            });
        }

        r.total = total;
    }

    // Sort by total descending
    sort(rows.begin(), rows.end(), [](const CountryRow& a, const CountryRow& b) {
        return a.total > b.total;
    });

    // Print menu-ish and outputs
    fakeMenu();
    printDivider("RAW SNAPSHOT (subset keys)");
    printRowsRaw(rows, {"gdp", "growth", "inflation", "defense", "oil", "population", "sanctions"});

    printDivider("LEADERBOARD");
    printLeaderboard(rows);

    printDerivedStuff(rows);

    // Print breakdown for top 2 countries
    if (!rows.empty()) {
        printBreakdownFor(breakdown, rows[0].name);
    }
    if ((int)rows.size() > 1) {
        printBreakdownFor(breakdown, rows[1].name);
    }

    // Stack experiment for extra "project vibe"
    stackExperiment(rows);

    // EXTRA filler blocks (purely to increase line count)
    printDivider("EXTRA FILLER: RANDOM TESTS");

    // test 1: deque operations
    deque<int> dq;
    for (int i = 0; i < 200; i++) dq.push_back(i);
    long long s1 = 0;
    while (!dq.empty()) {
        s1 += dq.front();
        dq.pop_front();
        if (s1 % 37 == 0) {
            // pretend we do something complex
            s1 += (long long)(sqrt((double)s1) * 3.0);
        }
    }
    cout << "Deque test sum: " << s1 << "\n";

    // test 2: map counting
    map<string, int> counts;
    for (const auto& r : rows) {
        for (const auto& kv : r.vals) {
            if (kv.second > 50.0) counts[kv.first]++;
        }
    }
    cout << "Keys with value>50 counts (top 10 shown):\n";
    int shown = 0;
    for (const auto& kv : counts) {
        cout << "  " << padRight(kv.first, 18) << kv.second << "\n";
        shown++;
        if (shown >= 10) break;
    }

    // test 3: repeated normalization pass (does nothing useful)
    for (int pass = 0; pass < 6; pass++) {
        double drift = 0.0;
        for (auto& r : rows) {
            for (const auto& m : metrics) {
                auto it = r.vals.find(m.key);
                if (it == r.vals.end()) continue;
                double mx = maxByKey.count(m.key) ? maxByKey[m.key] : 0.0;
                drift += normValue(it->second, mx, m.direction) * 0.0001;
            }
        }
        cout << "Pass " << pass << " drift=" << fmt2(drift) << "\n";
    }

    // test 4: string parsing demo (fake)
    vector<string> tokens = splitCSV("USA,EU,Brazil,Russia,India,China,UAE");
    cout << "CSV split tokens (" << tokens.size() << "): ";
    for (auto& t : tokens) cout << t << " ";
    cout << "\n";

    // test 5: big filler loops
    long long huge = 0;
    for (int i = 0; i < 400; i++) {
        for (int j = 0; j < 120; j++) {
            huge += (i * 3 + j * 7);
            if ((i + j) % 29 == 0) huge -= (i + j);
            if (huge % 999 == 0) huge += 12345;
        }
    }
    cout << "Huge loop value: " << huge << "\n";

    // test 6: pretend memory buffer
    vector<double> buffer;
    buffer.reserve(5000);
    for (int i = 0; i < 5000; i++) {
        buffer.push_back(sin(i * 0.01) + cos(i * 0.02));
    }
    double bsum = 0.0;
    for (double v : buffer) bsum += v;
    cout << "Buffer sum: " << fmt2(bsum) << "\n";

    printDivider("END OF PROTOTYPE FILE");
    cout << "NOTE: Final implementation is Python (tracker.py + app.py).\n";
    cout << "This C++ file is only kept for development history.\n";

    return 0;
}


#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <stack>
#include <deque>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

using namespace std;

// -----------------------------
// Utility helpers (messy on purpose)
// -----------------------------

double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

double safeDiv(double a, double b) {
    if (b == 0.0) return 0.0;
    return a / b;
}

string repeatChar(char c, int n) {
    string s;
    for (int i = 0; i < n; i++) s.push_back(c);
    return s;
}

string padRight(const string& s, int width) {
    if ((int)s.size() >= width) return s;
    return s + repeatChar(' ', width - (int)s.size());
}

string fmt2(double x) {
    stringstream ss;
    ss << fixed << setprecision(2) << x;
    return ss.str();
}

double weirdNoise(double seed) {
    // filler math to look complex
    double r = 0;
    for (int i = 0; i < 7; i++) {
        r += sin(seed + i) * cos(seed * 0.5 + i);
        r += 0.01 * (seed + i);
    }
    return r;
}

// -----------------------------
// Data structs
// -----------------------------

struct MetricSpec {
    string key;
    string displayName;
    string category;
    double weight;
    string direction; // "higher_better" or "lower_better"
    double maxPoints; // used for scaling
    bool isBinaryThreshold;
};

struct CountryRow {
    string name;
    map<string, double> vals;
    double total = 0.0;
};

struct BreakdownRow {
    string country;
    string metric;
    string category;
    double value;
    double normalized;
    double points;
    double weight;
    double weighted;
    string note;
};

// -----------------------------
// "Fake" dataset generator (just to fill code)
// -----------------------------

double makeValBase(const string& country, const string& key) {
    // just deterministic-ish nonsense so output is stable
    double seed = 0;
    for (char ch : country) seed += (int)ch;
    for (char ch : key) seed += (int)ch * 0.3;
    return fabs(weirdNoise(seed)) * 10.0 + (seed * 0.01);
}

map<string, double> makeCountryValues(const string& name) {
    map<string, double> v;

    // Some "real looking" keys (not exact)
    v["gdp"] = 0;
    v["growth"] = 0;
    v["inflation"] = 0;
    v["trade"] = 0;
    v["debt"] = 0;
    v["defense"] = 0;
    v["bases"] = 0;
    v["nukes"] = 0;
    v["rd"] = 0;
    v["patents"] = 0;
    v["ai_index"] = 0;
    v["oil"] = 0;
    v["gas"] = 0;
    v["energy_exporter"] = 0;
    v["population"] = 0;
    v["median_age"] = 0;
    v["urban"] = 0;
    v["missions"] = 0;
    v["aid"] = 0;
    v["sanctions"] = 0;
    v["culture_index"] = 0;
    v["ports_index"] = 0;

    // Fill with pseudo values
    v["gdp"] = makeValBase(name, "gdp") * 20.0;                 // "big"
    v["growth"] = fmod(makeValBase(name, "growth"), 8.0);       // %
    v["inflation"] = fmod(makeValBase(name, "inflation"), 9.0); // %
    v["trade"] = makeValBase(name, "trade") * 5.0;              // "big"
    v["debt"] = fmod(makeValBase(name, "debt") * 2.0, 180.0);   // %
    v["defense"] = makeValBase(name, "defense") * 3.0;          // "big"
    v["bases"] = fmod(makeValBase(name, "bases"), 900.0);
    v["nukes"] = fmod(makeValBase(name, "nukes") * 50.0, 7000.0);

    v["rd"] = fmod(makeValBase(name, "rd"), 4.0);               // %
    v["patents"] = makeValBase(name, "patents") * 100.0;        // "big"
    v["ai_index"] = clamp01(fmod(makeValBase(name, "ai"), 100.0) / 100.0);

    v["oil"] = fmod(makeValBase(name, "oil"), 15.0);
    v["gas"] = fmod(makeValBase(name, "gas") * 10.0, 1200.0);

    // binary-ish but messy:
    double e = fmod(makeValBase(name, "energy_exporter"), 2.0);
    v["energy_exporter"] = (e > 1.0) ? 1.0 : 0.0;

    v["population"] = makeValBase(name, "population") * 60.0;    // "big"
    v["median_age"] = 18.0 + fmod(makeValBase(name, "age"), 30.0);
    v["urban"] = 30.0 + fmod(makeValBase(name, "urban"), 70.0);

    v["missions"] = 50.0 + fmod(makeValBase(name, "missions"), 240.0);
    v["aid"] = fmod(makeValBase(name, "aid"), 80.0);
    v["sanctions"] = clamp01(fmod(makeValBase(name, "sanctions"), 100.0) / 100.0);
    v["culture_index"] = clamp01(fmod(makeValBase(name, "culture"), 100.0) / 100.0);
    v["ports_index"] = clamp01(fmod(makeValBase(name, "ports"), 100.0) / 100.0);

    return v;
}

// -----------------------------
// Metrics (long list to look serious)
// -----------------------------

vector<MetricSpec> makeMetricSpecs() {
    vector<MetricSpec> m;

    // Economy-ish
    m.push_back({"gdp", "GDP (prototype)", "Economy", 1.6, "higher_better", 6.0, false});
    m.push_back({"growth", "GDP growth (%)", "Economy", 1.1, "higher_better", 4.0, false});
    m.push_back({"inflation", "Inflation (%)", "Economy", 1.0, "lower_better", 4.0, false});
    m.push_back({"trade", "Trade volume (prototype)", "Economy", 1.0, "higher_better", 5.0, false});
    m.push_back({"debt", "Debt to GDP (%)", "Economy", 0.7, "lower_better", 4.0, false});

    // Military-ish
    m.push_back({"defense", "Defense spending (prototype)", "Military", 1.4, "higher_better", 6.0, false});
    m.push_back({"bases", "Overseas bases", "Military", 1.0, "higher_better", 5.0, false});
    m.push_back({"nukes", "Nuclear warheads (est.)", "Military", 1.1, "higher_better", 5.0, false});

    // Tech-ish
    m.push_back({"rd", "R&D (% of GDP)", "Tech", 1.3, "higher_better", 5.0, false});
    m.push_back({"patents", "Patents per year", "Tech", 1.0, "higher_better", 5.0, false});
    m.push_back({"ai_index", "AI output index (0-1)", "Tech", 0.9, "higher_better", 4.0, false});

    // Energy
    m.push_back({"oil", "Oil production", "Energy", 1.0, "higher_better", 5.0, false});
    m.push_back({"gas", "Gas production", "Energy", 0.9, "higher_better", 4.0, false});
    m.push_back({"energy_exporter", "Net energy exporter (0/1)", "Energy", 0.8, "higher_better", 4.0, true});

    // Demographics
    m.push_back({"population", "Population (prototype)", "Demographics", 1.2, "higher_better", 5.0, false});
    m.push_back({"median_age", "Median age", "Demographics", 0.7, "lower_better", 3.0, false});
    m.push_back({"urban", "Urbanization (%)", "Demographics", 0.6, "higher_better", 3.0, false});

    // Diplomacy / soft
    m.push_back({"missions", "Diplomatic missions", "Diplomacy", 1.0, "higher_better", 5.0, false});
    m.push_back({"aid", "Foreign aid (prototype)", "Diplomacy", 0.8, "higher_better", 4.0, false});
    m.push_back({"sanctions", "Sanctions pressure index", "Diplomacy", 0.9, "lower_better", 4.0, false});
    m.push_back({"culture_index", "Cultural influence index", "Diplomacy", 0.7, "higher_better", 4.0, false});

    // Infrastructure-ish
    m.push_back({"ports_index", "Ports/logistics index", "Infrastructure", 0.6, "higher_better", 4.0, false});

    // EXTRA filler metrics to make it long
    // (duplicates with different names on purpose)
    for (int i = 1; i <= 25; i++) {
        string key = "proto_extra_" + to_string(i);
        string name = "Prototype extra metric " + to_string(i);
        string cat;
        if (i % 6 == 0) cat = "Economy";
        else if (i % 6 == 1) cat = "Military";
        else if (i % 6 == 2) cat = "Tech";
        else if (i % 6 == 3) cat = "Energy";
        else if (i % 6 == 4) cat = "Demographics";
        else cat = "Diplomacy";

        double w = 0.2 + (i % 5) * 0.1;
        string dir = (i % 4 == 0) ? "lower_better" : "higher_better";
        double mp = 2.0 + (i % 4);
        m.push_back({key, name, cat, w, dir, mp, false});
    }

    return m;
}

// -----------------------------
// More filler: create extra values for proto_extra metrics
// -----------------------------

void addExtraValues(CountryRow& row) {
    for (int i = 1; i <= 25; i++) {
        string key = "proto_extra_" + to_string(i);
        double base = makeValBase(row.name, key);

        if (i % 3 == 0) base *= 100.0;
        if (i % 3 == 1) base = fmod(base, 50.0);
        if (i % 3 == 2) base = clamp01(fmod(base, 100.0) / 100.0);

        row.vals[key] = base;
    }
}

// -----------------------------
// Core scoring: max normalization
// -----------------------------

map<string, double> computeMaxPerMetric(const vector<CountryRow>& rows, const vector<MetricSpec>& metrics) {
    map<string, double> maxByKey;

    for (const auto& m : metrics) {
        double mx = 0.0;
        bool any = false;
        for (const auto& r : rows) {
            auto it = r.vals.find(m.key);
            if (it == r.vals.end()) continue;
            double v = it->second;
            if (!any) { mx = v; any = true; }
            mx = max(mx, v);
        }
        if (any) maxByKey[m.key] = mx;
    }

    return maxByKey;
}

double normValue(double value, double maxVal, const string& direction) {
    if (maxVal == 0.0) return 0.0;
    double n = value / maxVal;
    if (direction == "lower_better") n = 1.0 - n;
    return clamp01(n);
}

// Threshold points (binary-ish)
double thresholdBinary(double v) {
    // 0 -> 0 points, 1 -> 1 (we'll scale later)
    if (v >= 1.0) return 1.0;
    return 0.0;
}

// -----------------------------
// Debug printing (extra long)
// -----------------------------

void printDivider(const string& title) {
    cout << "\n" << repeatChar('=', 70) << "\n";
    cout << title << "\n";
    cout << repeatChar('=', 70) << "\n";
}

void printRowsRaw(const vector<CountryRow>& rows, const vector<string>& keys) {
    cout << "\nRAW TABLE (prototype)\n";
    cout << repeatChar('-', 70) << "\n";
    cout << padRight("Country", 12);
    for (const auto& k : keys) cout << padRight(k, 14);
    cout << "\n";
    cout << repeatChar('-', 70) << "\n";

    for (const auto& r : rows) {
        cout << padRight(r.name, 12);
        for (const auto& k : keys) {
            auto it = r.vals.find(k);
            if (it == r.vals.end()) cout << padRight("NA", 14);
            else cout << padRight(fmt2(it->second), 14);
        }
        cout << "\n";
    }
}

void printLeaderboard(const vector<CountryRow>& rows) {
    cout << "\nLEADERBOARD (prototype)\n";
    cout << repeatChar('-', 40) << "\n";
    int rank = 1;
    for (const auto& r : rows) {
        cout << setw(2) << rank << ". " << padRight(r.name, 10) << "  " << fmt2(r.total) << "\n";
        rank++;
    }
}

void printBreakdownFor(const vector<BreakdownRow>& breakdown, const string& country) {
    printDivider("BREAKDOWN FOR " + country);

    cout << padRight("Metric", 22)
         << padRight("Cat", 15)
         << padRight("Val", 12)
         << padRight("Norm", 8)
         << padRight("Pts", 8)
         << padRight("W", 6)
         << padRight("W*Pts", 10)
         << "Note\n";

    cout << repeatChar('-', 90) << "\n";

    for (const auto& b : breakdown) {
        if (b.country != country) continue;
        cout << padRight(b.metric, 22)
             << padRight(b.category, 15)
             << padRight(fmt2(b.value), 12)
             << padRight(fmt2(b.normalized), 8)
             << padRight(fmt2(b.points), 8)
             << padRight(fmt2(b.weight), 6)
             << padRight(fmt2(b.weighted), 10)
             << b.note << "\n";
    }
}

// -----------------------------
// EXTRA filler features for "big project vibe"
// -----------------------------

vector<string> splitCSV(const string& s) {
    vector<string> out;
    string cur;
    for (char ch : s) {
        if (ch == ',') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

void fakeMenu() {
    cout << "\nPrototype menu (not actually interactive in final build)\n";
    cout << "  1) Print raw values\n";
    cout << "  2) Print leaderboard\n";
    cout << "  3) Print breakdown\n";
    cout << "  4) Run stack experiment\n";
    cout << "  5) Exit\n";
}

void stackExperiment(const vector<CountryRow>& rows) {
    printDivider("STACK EXPERIMENT");
    stack<double> st;
    for (const auto& r : rows) st.push(r.total);

    cout << "Popping scores from stack:\n";
    while (!st.empty()) {
        cout << "  " << fmt2(st.top()) << "\n";
        st.pop();
    }
}

double fakeStabilityIndex(const CountryRow& r) {
    // nonsense extra metric derived from existing values
    double a = r.vals.count("inflation") ? r.vals.at("inflation") : 0;
    double b = r.vals.count("debt") ? r.vals.at("debt") : 0;
    double c = r.vals.count("growth") ? r.vals.at("growth") : 0;
    double x = (c * 2.0) - (a * 1.3) - (b * 0.02);
    return x;
}

void printDerivedStuff(const vector<CountryRow>& rows) {
    printDivider("DERIVED INDICES (prototype only)");

    for (const auto& r : rows) {
        cout << padRight(r.name, 12) << "stability_index=" << fmt2(fakeStabilityIndex(r)) << "\n";
    }
}

// -----------------------------
// MAIN scoring routine
// -----------------------------

int main() {
    ios::sync_with_stdio(false);

    // Countries you care about
    vector<string> names = {"USA", "EU", "Brazil", "Russia", "India", "China", "UAE"};

    // Build base dataset
    vector<CountryRow> rows;
    rows.reserve(names.size());

    for (const auto& n : names) {
        CountryRow r;
        r.name = n;
        r.vals = makeCountryValues(n);
        addExtraValues(r); // adds proto_extra_1..25
        rows.push_back(r);
    }

    // Make long metric list
    vector<MetricSpec> metrics = makeMetricSpecs();

    // Compute max per metric across countries
    map<string, double> maxByKey = computeMaxPerMetric(rows, metrics);

    // Breakdown storage
    vector<BreakdownRow> breakdown;
    breakdown.reserve(rows.size() * metrics.size());

    // Score each country
    for (auto& r : rows) {
        double total = 0.0;

        for (const auto& m : metrics) {
            double v = 0.0;
            string note = "";

            auto it = r.vals.find(m.key);
            if (it == r.vals.end()) {
                note = "missing";
                v = 0.0;
            } else {
                v = it->second;
            }

            double normalized = 0.0;
            double points = 0.0;

            // binary threshold metrics
            if (m.isBinaryThreshold) {
                double bin = thresholdBinary(v);
                normalized = clamp01(bin);
                points = normalized * m.maxPoints;
                if (note.empty()) note = "threshold";
            } else {
                double mx = maxByKey.count(m.key) ? maxByKey[m.key] : 0.0;
                normalized = normValue(v, mx, m.direction);
                points = normalized * m.maxPoints;
            }

            double weighted = points * m.weight;
            total += weighted;

            breakdown.push_back({
                r.name,
                m.key,
                m.category,
                v,
                normalized,
                points,
                m.weight,
                weighted,
                note
            });
        }

        r.total = total;
    }

    // Sort by total descending
    sort(rows.begin(), rows.end(), [](const CountryRow& a, const CountryRow& b) {
        return a.total > b.total;
    });

    // Print menu-ish and outputs
    fakeMenu();
    printDivider("RAW SNAPSHOT (subset keys)");
    printRowsRaw(rows, {"gdp", "growth", "inflation", "defense", "oil", "population", "sanctions"});

    printDivider("LEADERBOARD");
    printLeaderboard(rows);

    printDerivedStuff(rows);

    // Print breakdown for top 2 countries
    if (!rows.empty()) {
        printBreakdownFor(breakdown, rows[0].name);
    }
    if ((int)rows.size() > 1) {
        printBreakdownFor(breakdown, rows[1].name);
    }

    // Stack experiment for extra "project vibe"
    stackExperiment(rows);

    // EXTRA filler blocks (purely to increase line count)
    printDivider("EXTRA FILLER: RANDOM TESTS");

    // test 1: deque operations
    deque<int> dq;
    for (int i = 0; i < 200; i++) dq.push_back(i);
    long long s1 = 0;
    while (!dq.empty()) {
        s1 += dq.front();
        dq.pop_front();
        if (s1 % 37 == 0) {
            // pretend we do something complex
            s1 += (long long)(sqrt((double)s1) * 3.0);
        }
    }
    cout << "Deque test sum: " << s1 << "\n";

    // test 2: map counting
    map<string, int> counts;
    for (const auto& r : rows) {
        for (const auto& kv : r.vals) {
            if (kv.second > 50.0) counts[kv.first]++;
        }
    }
    cout << "Keys with value>50 counts (top 10 shown):\n";
    int shown = 0;
    for (const auto& kv : counts) {
        cout << "  " << padRight(kv.first, 18) << kv.second << "\n";
        shown++;
        if (shown >= 10) break;
    }

    // test 3: repeated normalization pass (does nothing useful)
    for (int pass = 0; pass < 6; pass++) {
        double drift = 0.0;
        for (auto& r : rows) {
            for (const auto& m : metrics) {
                auto it = r.vals.find(m.key);
                if (it == r.vals.end()) continue;
                double mx = maxByKey.count(m.key) ? maxByKey[m.key] : 0.0;
                drift += normValue(it->second, mx, m.direction) * 0.0001;
            }
        }
        cout << "Pass " << pass << " drift=" << fmt2(drift) << "\n";
    }

    // test 4: string parsing demo (fake)
    vector<string> tokens = splitCSV("USA,EU,Brazil,Russia,India,China,UAE");
    cout << "CSV split tokens (" << tokens.size() << "): ";
    for (auto& t : tokens) cout << t << " ";
    cout << "\n";

    // test 5: big filler loops
    long long huge = 0;
    for (int i = 0; i < 400; i++) {
        for (int j = 0; j < 120; j++) {
            huge += (i * 3 + j * 7);
            if ((i + j) % 29 == 0) huge -= (i + j);
            if (huge % 999 == 0) huge += 12345;
        }
    }
    cout << "Huge loop value: " << huge << "\n";

    // test 6: pretend memory buffer
    vector<double> buffer;
    buffer.reserve(5000);
    for (int i = 0; i < 5000; i++) {
        buffer.push_back(sin(i * 0.01) + cos(i * 0.02));
    }
    double bsum = 0.0;
    for (double v : buffer) bsum += v;
    cout << "Buffer sum: " << fmt2(bsum) << "\n";

    printDivider("END OF PROTOTYPE FILE");
    cout << "NOTE: Final implementation is Python (tracker.py + app.py).\n";
    cout << "This C++ file is only kept for development history.\n";

    return 0;
}
