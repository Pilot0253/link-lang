---

# 🚀 ChainLang v0.7.2 Pre Release 

This update marks one of the largest milestones in ChainLang’s development history. We have completely overhauled the core architecture of the *Parser* and *Runtime*, resolved critical bugs, and introduced industry-grade features.

ChainLang is no longer just a prototype scripting language; it is now a **production-ready programming language** designed for Scientific Computing, Interactive GUI Development, and Native Desktop Application Development!

Here is the full list of new features, upgrades, and fixes included in this release...

## ✨ Major Features

### 1. 🧠 Smart Memory Management (Mark-and-Sweep GC)
The biggest architectural leap in this update. We have completely stripped out C++ `std::shared_ptr` (Reference Counting) and replaced it with a custom, professional-grade **Mark-and-Sweep Garbage Collector**.
* **Zero Memory Leaks:** Safely handles complex data structures and eliminates memory leaks caused by Cyclic References (e.g., Objects or Lists referencing each other).
* **Dynamic Thresholding:** The GC is smart. It scales its collection threshold dynamically based on the program's actual RAM usage, ensuring maximum performance without thrashing.
* **Thread-Safe Safepoints:** GC sweeps only trigger at safe execution points, ensuring 100% safety during multithreading.
* **New CLI Flag:** Run your scripts with `chainlang --gc <file>` to monitor the Garbage Collector sweeping memory in real-time!

### 2. 🛡️ Visual Error Handling 
Say goodbye to confusing crash logs! ChainLang now features an informative visual error-tracing system. When a *Syntax Error* or *Runtime Error* occurs, the engine does not silently fail; instead, it renders the exact line of code alongside caret indicators (`^^^`) pointing directly to the origin of the typo or error.

### 3. 🧵 Concurrency (Multithreading)
ChainLang now supports Asynchronous Programming! Using the new `thread.spawn(func)` module, long-running processes (such as intensive processing loops or network servers) can run on background threads. This ensures your primary thread (such as Raylib UI or Webview loops) remains smooth and responsive without freezing or lagging. Fully compatible and thread-safe with the new Garbage Collector.

### 4. 🖥️ Native Desktop Apps (Webview Engine)
Introducing our latest feature: native desktop application development (similar to VSCode or Discord architectures) directly within ChainLang!
* Uses a modern architectural split: HTML/CSS for the UI, ChainLang/C++ for the backend.
* Supports bi-directional IPC: Invoke C++ functions from HTML, and inject dynamic JavaScript from C++ using `webview.eval()`.
* Lightweight runtime footprint by binding directly to native OS WebKit engines (avoiding bundled Chromium/Electron overhead).

### 5. 🧬 Advanced Object-Oriented Programming (OOP)
ChainLang's OOP model has been upgraded:
* **Inheritance:** Classes can now inherit properties and methods from parent classes using the `class Robot : Engine` syntax.
* **Magic Methods (Operator Overloading):** Implement special methods like `__add__` to allow native operator evaluation on objects (e.g., `obj1 + obj2`).

### 6. 🧩 Pattern Matching & Lambdas
* **Match Statement:** A cleaner, modern alternative to nested `if/elif` blocks for structural pattern branching.
* **First-Class Functions (Lambdas):** Functions can be assigned to variables and passed around as arguments via the `set myFunc = func(x) { ... }` syntax.

---

## 🛠️ Standard Library Upgrades

### 🧮 `math` Module (Scientific Computing Library)
The math module has been refactored to use accurate double-precision floating-point arithmetic. ChainLang is now suited for baseline Data Science and Mathematical modeling!
* **New:** Euler's constant `math.e()`.
* **New (Inverse Trigonometry):** `asin`, `acos`, `atan`, `atan2`.
* **New (Hyperbolic):** `sinh`, `cosh`, `tanh`.
* **New (Exponential & Logarithmic):** `exp`, `log`, `log10`, `log2`.
* **New (Utilities):** `ceil`, `floor`, `round`, `min`, `max`.
* **New (Core):** Modulo operator (`%`) is now natively supported across the engine.

### 🎨 `gui` Module (Raylib Enhancements)
* **New:** `gui_line(x1, y1, x2, y2, thick, color)` for line rendering (useful for graph and vector visualizations).
* **New:** `gui_begin_clip()` and `gui_end_clip()` for Scissor/Masking support, allowing scrollable regions inside custom UIs (such as Code Editors).
* **Fixes:** The GUI coordinate system is now more accurate and handles window resizing events smoothly.

---

## 🐛 Bug Fixes & Optimizations

* **[Parser] Constructor Syntax:** Fixed a frustrating bug where class constructors required the `func` keyword. You can now write `init(name) { ... }` natively as originally intended.
* **[Lexer] Multiline Strings:** The lexer now correctly parses string literals across multiple lines (containing newlines/`\n`). Essential for embedding raw HTML or JSON templates.
* **[Lexer] Escape Characters:** Resolved "Unterminated String" parsing issues. The lexer now handles standard escape sequences (like `\"`, `\n`, `\t`) properly.
* **[Parser] Strict Mode & Fail-Fast:** The parser no longer ignores errors further down in execution files. Syntax errors are caught immediately during the parsing phase.
* **[Parser] Whitespace handling in Try-Catch & Classes:** Fixed a bug where empty spaces or newlines between `try`/`catch` blocks or inside `class` declarations caused parse failures.
* **[Runtime] `print` Function:** Resolved terminal buffer flushing bugs on Linux/macOS. Output sent via `print()` now flushes to the console in real-time with proper `\n` processing.
* **[Runtime] `str.substr` Function:** Resolved an issue where the 3rd argument (length) was ignored by C++. String slicing is now fully accurate.
* **[Runtime] `//` Comments:** Fixed an issue where the lexer occasionally interpreted `//` as a division operator sequence. Single-line comments are now strictly ignored.

---

**Thank you for using ChainLang! Keep innovating and building the future with us! 🌌**
