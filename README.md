# Wordle-Solver-Entropy
## **🎯 Wordle-Solver-Entropy**
_A blazing-fast, information-theoretic Wordle AI_

Wordle-Solver-Entropy is a high-performance C++ solver that uses information theory to crack the popular NYT Wordle game with near-optimal efficiency. It strategically selects guesses to maximize entropy (information gain), narrowing the possible solutions as fast as possible.

## 🧠 How It Works

At a high level, the solver cycles through this loop:
1. Filter remaining words by matching past feedback
2. Evaluate each possible guess:
      → Simulate its feedback patterns for all possible solutions
      → Calculate the expected information gain (entropy)
3. Pick the guess with the highest entropy
4. Repeat until solved

## **Visual overview:**
```text
     ┌─────────────┐
     │ Word List   │   e.g., ["slate", "crane", "trace", ...]
     └─────┬───────┘
           │
           ▼
   ┌─────────────┐
   │ Filter pool │   ← Remove words inconsistent with past feedback
   └─────┬───────┘
         │
         ▼
   ┌─────────────┐
   │ Entropy calc│   ← Pick the guess that best splits the search space
   └─────┬───────┘
         │
         ▼
   ┌─────────────┐
   │ Make guess  │
   └─────────────┘
         │
   (Repeat until solved)
```
   
## **⚡ Performance Features**
- Entropy-based guess selection: Always picks the guess that, on average, will reduce the most uncertainty.
- Monte Carlo fallback: When >1500 candidates remain, uses a random sample for faster entropy estimation.
- Precomputed feedback table: All pairwise Wordle feedback patterns are computed once and stored.
- Multithreaded testing: Uses all available CPU cores to batch-test performance.
- First-guess override: Always starts with "slate" (configurable).

## **📊 Benchmark Results**
Original NYT word list (2,315 words, short_words.txt):

- Average guesses: 3.4903 (within ~2% of the theoretical minimum 3.4212)
- Time to solve all words: ~0.3s
- Win rate: 100% (≤6 guesses)

## **Extended list (14,855 words, words.txt):**
- Average guesses: 4.24
- Time to solve all words: 27.1s
- Win rate: 98.9%
- Common failure cases: rare/duplicate heavy words like waqfs, vives

_💡 Real-world optimization potential: weighting guesses by real-world word and letter frequency could improve performance against human-curated word sets._

## **🖥️ Installation & Usage**

Note: If you're using the solver to play wordle nowadays, be sure to provide the extended words list "words.txt" to the solver in the line G_words = load_words("words.txt") so that it encompasses all the possible solutions of the extended word bank. Also, be sure to use interactive mode (option 1 when the console prompts you), and enter the result string as five digits, 0 for gray, 1 for yellow, 2 for green. (e.g. 00122 for 2 grays, one yellow, and 2 greens.).

```
# Clone the repository: 
git clone https://github.com/yourusername/Wordle-Solver-Entropy.git
cd Wordle-Solver-Entropy
```
```
# Compile (requires C++17 or later)
g++ -O3 -std=c++17 -pthread wordle_solver.cpp -o solver
```
```
# Run interactive mode
./solver
# Choose option 1
```
```
# Run benchmark mode
./solver
# Choose option 2
```
## **🎮 What is Wordle?**

Wordle is a 5-letter word guessing game.
After each guess, tiles show:

🟩 Green: correct letter & position
🟨 Yellow: correct letter, wrong position
⬜ Gray: letter not in the word

More about the rules: [Wikipedia]([url](https://en.wikipedia.org/wiki/Wordle))

## **🤝 Contributions**

PRs, optimizations, and alternative strategy experiments are welcome!
Whether you’re into AI theory, C++ performance tuning, or just want to use the solver to improve your wordle scores, feel free to dive in! 
