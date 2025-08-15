#include <bits/stdc++.h>
using namespace std;

/*** Config knobs (tune these for speed/quality tradeoff) ***/
static const int    kMCThreshold   = 1500; // if candidates > this, use Monte Carlo approximation
static const int    kMCSamples     = 400;  // number of sampled solutions for entropy estimation
static const bool   kUseFullPoolWhenNarrow = true; // use full guess pool when 4 <= cand < 50

/*** Utilities ***/
struct Guess { string word; string result; };

static inline uint16_t encode_feedback_string(const string &s) {
    uint16_t code = 0, pow3 = 1;
    for (int i = 0; i < 5; ++i) {
        code += (s[i] - '0') * pow3;
        pow3 *= 3;
    }
    return code;
}

static inline uint16_t encode_feedback_query(const string &g, const string &sol) {
    // Wordle-accurate feedback with duplicate handling
    char res[5] = {'0','0','0','0','0'};
    bool used[5] = {false,false,false,false,false};

    // greens (represented by 2)
    for (int i = 0; i < 5; ++i) {
        if (g[i] == sol[i]) { res[i] = '2'; used[i] = true; }
    }
    // yellows (represented by 1)
    for (int i = 0; i < 5; ++i) {
        if (res[i] == '2') continue;
        for (int j = 0; j < 5; ++j) {
            if (!used[j] && g[i] == sol[j]) { res[i] = '1'; used[j] = true; break; }
        }
    }
    uint16_t code = 0, pow3 = 1;
    for (int i = 0; i < 5; ++i) { code += (res[i] - '0') * pow3; pow3 *= 3; }
    return code;
}

/*** I/O ***/
static vector<string> load_words(const string &filename) {
    ifstream in(filename);
    vector<string> words;
    string s;
    while (in >> s) {
        if (s.size() == 5) {
            for (char &c : s) c = (char)tolower(c);
            words.push_back(s);
        }
    }
    return words;
}

/*** Globals ***/
static vector<string> G_words;
static unordered_map<string,int> G_index;
static vector<uint16_t> G_fb;
static int N = 0;

static inline uint16_t FB(int i, int j) { return G_fb[(size_t)i * (size_t)N + (size_t)j]; }

/*** Precompute all feedbacks ***/
static void precompute_feedback_table(int threads = max(1u, thread::hardware_concurrency())) {
    G_fb.assign((size_t)N * (size_t)N, 0);
    atomic<int> next_row{0};
    vector<future<void>> pool;
    pool.reserve(threads);
    for (int t = 0; t < threads; ++t) {
        pool.push_back(async(launch::async, [&]() {
            int i;
            while ((i = next_row.fetch_add(1)) < N) {
                const string &gi = G_words[i];
                uint16_t *row = &G_fb[(size_t)i * (size_t)N];
                for (int j = 0; j < N; ++j) {
                    row[j] = encode_feedback_query(gi, G_words[j]);
                }
            }
        }));
    }
    for (auto &f : pool) f.get();
}

/*** Filtering ***/
static vector<int> filter_candidates(const vector<pair<int,uint16_t>> &past, const vector<int> &pool) {
    vector<int> out;
    out.reserve(pool.size());
    for (int w : pool) {
        bool ok = true;
        for (auto &gr : past) {
            if (FB(gr.first, w) != gr.second) { ok = false; break; }
        }
        if (ok) out.push_back(w);
    }
    return out;
}

/*** Entropy helpers ***/
static inline double compute_entropy_from_counts(const int *counts, int denom) {
    // denom > 0, counts size 243
    double entropy = 0.0;
    for (int k = 0; k < 243; ++k) {
        int c = counts[k];
        if (c == 0) continue;
        double p = (double)c / (double)denom;
        entropy -= p * log2(p);
    }
    return entropy;
}

static inline int draw_uniform_int(mt19937 &rng, int lo, int hi) {
    uniform_int_distribution<int> dist(lo, hi);
    return dist(rng);
}

/*** Entropy-based guess selection (with Monte Carlo fallback) ***/
static int choose_best_guess_entropy(
    const vector<int> &candidates,
    const vector<int> &guess_pool,
    int mc_threshold = kMCThreshold,
    int mc_samples   = kMCSamples
) {
    // Decide evaluation set for entropy: exact vs Monte Carlo sample
    vector<int> entropy_set;
    entropy_set.reserve(candidates.size());

    if ((int)candidates.size() > mc_threshold) {
        // Sample without replacement (fast approximate)
        entropy_set = candidates; // copy to shuffle
        static thread_local random_device rd;
        static thread_local mt19937 rng(rd());
        shuffle(entropy_set.begin(), entropy_set.end(), rng);
        if ((int)entropy_set.size() > mc_samples) entropy_set.resize(mc_samples);
    } else {
        entropy_set = candidates;
    }

    int best_idx = -1;
    double best_ent = -1.0;

    // For each guess, count pattern frequencies against entropy_set
    for (int gi : guess_pool) {
        const uint16_t *row = &G_fb[(size_t)gi * (size_t)N];
        int counts[243] = {0}; // 3^5 patterns

        for (int cj : entropy_set) {
            uint16_t fb = row[cj];
            ++counts[fb];
        }

        double ent = compute_entropy_from_counts(counts, (int)entropy_set.size());
        if (ent > best_ent) {
            best_ent = ent;
            best_idx = gi;
        }
    }

    return best_idx;
}

/*** Play one game ***/
static int play_wordle_idx(int sol_idx) {
    vector<pair<int,uint16_t>> history;
    vector<int> pool(N);
    iota(pool.begin(), pool.end(), 0);
    int turns = 0;
    const uint16_t solved = encode_feedback_string("22222");

    while (true) {
        vector<int> cand = filter_candidates(history, pool);
        const vector<int> &guess_pool =
            (kUseFullPoolWhenNarrow && cand.size() >= 4 && cand.size() < 50) ? pool : cand;

        int guess;

        if (turns == 0) {
            // Force "slate" as first guess if available
            auto it = G_index.find("slate");
            if (it != G_index.end()) {
                guess = it->second;
            } else {
                guess = choose_best_guess_entropy(cand, guess_pool);
            }
        } else {
            guess = choose_best_guess_entropy(cand, guess_pool);
        }

        ++turns;

        uint16_t fb = FB(guess, sol_idx);
        history.emplace_back(guess, fb);
        if (fb == solved) break;
    }
    
    return turns;
}

/*** Interactive mode ***/
static void interactive_solver() {
    vector<pair<int,uint16_t>> history;
    vector<int> pool(N);
    iota(pool.begin(), pool.end(), 0);
    const uint16_t solved = encode_feedback_string("22222");

    int turn = 0;
    while (true) {
        vector<int> cand = filter_candidates(history, pool);
        cout << "Remaining candidates: " << cand.size() << endl;
        const vector<int> &guess_pool =
            (kUseFullPoolWhenNarrow && cand.size() >= 4 && cand.size() < 50) ? pool : cand;

        int best;
        if (turn == 0) {
            auto it = G_index.find("slate");
            if (it != G_index.end()) {
                best = it->second;
            } else {
                best = choose_best_guess_entropy(cand, guess_pool);
            }
        } else {
            best = choose_best_guess_entropy(cand, guess_pool);
        }

        cout << "Suggested guess: " << G_words[best] << endl;
        ++turn;

        cout << "Enter your guess (or 'quit'):" << endl;
        string g; cin >> g;
        if (g == "quit") return;

        for (char &c : g) c = (char)tolower(c);

        auto it = G_index.find(g);
        if (it == G_index.end()) { cout << "Not in dictionary" << endl; continue; }

        cout << "Enter result string (e.g. 20100): " << endl;
        string r; cin >> r;
        if (r.size() != 5) { cout << "Bad result format" << endl; continue; }

        uint16_t code = encode_feedback_string(r);
        history.emplace_back(it->second, code);

        if (code == solved) {
            cout << "Solved in " << history.size() << " guesses!" << endl;
            return;
        }
    }
}

/*** Test all solutions (parallel) ***/
static void test_all_solutions(int max_workers = max(1u, thread::hardware_concurrency())) {
    using clock = std::chrono::high_resolution_clock;
    auto start_time = clock::now();

    atomic<int> next{0};
    mutex agg_mtx;
    long long total_guesses = 0;
    int wins = 0;
    int losses = 0;
    vector<string> lost_words;

    vector<future<void>> workers;
    for (int w = 0; w < max_workers; ++w) {
        workers.push_back(async(launch::async, [&]() {
            long long local_sum = 0;
            int local_wins = 0;
            int local_losses = 0;
            vector<string> local_lost_words;

            int i;
            while ((i = next.fetch_add(1)) < N) {
                int turns = play_wordle_idx(i);
                local_sum += turns;
                if (turns <= 6) {
                    local_wins++;
                } else {
                    local_losses++;
                    local_lost_words.push_back(G_words[i]);
                }

                if ((i+1) % 50 == 0) {
                    cerr << (i+1) << "/" << N << " done\n";
                }
            }

            lock_guard<mutex> lk(agg_mtx);
            total_guesses += local_sum;
            wins += local_wins;
            losses += local_losses;
            lost_words.insert(lost_words.end(), local_lost_words.begin(), local_lost_words.end());
        }));
    }
    for (auto &f : workers) f.get();

    auto end_time = clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    double win_rate = 100.0 * wins / N;

    cout << fixed << setprecision(4);
    cout << "Average guesses: " << (double)total_guesses / N << endl;
    cout << "Win rate (<=6 guesses): " << win_rate << "%\n";
    cout << "Total games: " << N << ", Wins: " << wins << ", Losses: " << losses << endl;
    cout << "Total time: " << elapsed.count() << " seconds." << endl;

    if (!lost_words.empty()) {
        cout << "\nWords lost (" << lost_words.size() << "):\n";
        for (auto &w : lost_words) cout << w << "\n";
    }
}


/*** Main ***/
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    G_words = load_words("short_words.txt");
    if (G_words.empty()) { cerr << "words.txt not found or empty.\n"; return 1; }

    N = (int)G_words.size();
    G_index.reserve(N * 2);
    for (int i = 0; i < N; ++i) G_index[G_words[i]] = i;

    int threads = max(1u, thread::hardware_concurrency());
    cerr << "Precomputing feedback table for " << N << " words using " << threads << " threads...\n";
    precompute_feedback_table(threads);
    cerr << "Done.\n";

    cout << "Choose mode: 1) Interactive  2) Test performance" << endl;
    int mode; cin >> mode;

    if (mode == 1) interactive_solver();
    else test_all_solutions(threads);

    return 0;
}
