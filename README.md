```text
 ██████╗██╗  ██╗ █████╗ ██╗███╗   ██╗    ██╗      █████╗ ███╗   ██╗ ██████╗ 
██╔════╝██║  ██║██╔══██╗██║████╗  ██║    ██║     ██╔══██╗████╗  ██║██╔════╝ 
██║     ███████║███████║██║██╔██╗ ██║    ██║     ███████║██╔██╗ ██║██║  ███╗
██║     ██╔══██║██╔══██║██║██║╚██╗██║    ██║     ██╔══██║██║╚██╗██║██║   ██║
╚██████╗██║  ██║██║  ██║██║██║ ╚████║    ███████╗██║  ██║██║ ╚████║╚██████╔╝
 ╚═════╝╚═╝  ╚═╝╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝    ╚══════╝╚═╝  ╚═╝╚═╝  ╚═══╝ ╚═════╝ 
                  "Link your files with Chain!"
                  
============= A Nebania Project =============
---------------------------------------------------------------------------
Version   : 0.7.2 (Pre Release) 
Creator   : Pilot0253
Developer : LoGoRo17 && Tohar777
PROJECT   : Chain (Interpreter)
LANGUAGE  : C++
OS        : Linux Distro / Android (Custom Kernel) ARM-v8a
STATUS    : Active Development
DISCORD   : https://discord.gg/C6S6kn7dNz
```
Chain (Previously known as Link-Lang) is a dynamic, interpreted programming
language built for NebulaOS. It features a clean, Python-like syntax based on
indentation, designed for simplicity and readability. Built from scratch using
C++ (Lexer, Parser, AST, Runtime).

### Features

Chain has evolved from a simple math parser into a fully functional, production-ready
scripting language.

#### Core Capabilities

  - **Dynamic Typing:** Supports Integer, Float, String, Char, Boolean, Lists
    (Arrays), and Dictionaries.
  - **Math Engine:** Full support for +, -, *, /, % with operator precedence (PEMDAS)
    and parentheses ().
  - **Logic & Comparison:** Support for >, <, ==, !=, and, or operators.
  - **Control Flow:**
      - `if`, `elif`, `else` conditionals (recursive parsing).
      - Pattern Matching (`match ... =>`) for modern branching.
      - `while` loops.
      - `for` loops (basic numeric range & collection iteration).
      - `try` and `catch` for robust error handling.
  - **I/O Operations:** Built-in `print()` and `input()`.
  - **Hybrid Syntax:** Blocks can be defined by whitespace (Python-style) OR by
    using `{}` braces (C/Java-style).
  - **Comments:** Use `//` for single-line comments.

#### Advanced Features (v0.7.2 Major Update)

  - 🧠 **Smart Memory Management (NEW):** Chain now features a professional-grade **Mark-and-Sweep Garbage Collector (GC)**. It automatically handles cyclic references, features dynamic threshold scaling (starts at 1MB), and uses Safepoint polling to guarantee 100% thread-safety. Say goodbye to memory leaks during heavy Game Loops or Server operations!
  - **Object-Oriented Programming (OOP):** Full support for classes, constructors
    (`init`), methods, `this`, object instantiation (`new`), Inheritance, and Magic 
    Methods (Operator Overloading).
  - **First-Class Functions & Lambdas:** Functions can be assigned to variables 
    and passed around dynamically using `func(x) { ... }`.
  - **Concurrency (Goroutines):** Run heavy tasks in the background without 
    blocking the main thread using `thread.spawn(func)`. Thread-safe by design.
  - **Rust-like Visual Error Tracing:** Chain now points exactly to the line, 
    column, and token where a Syntax or Runtime Error occurred using visual `^^^` markers.
  - **Modular System:** Use `import "file.chain"` to split your code into multiple
    files and build reusable libraries with built-in import caching.
  - **Native C++ Wrapper (`extern "c"`):** The crown jewel of Chain. Write raw C++
    code directly inside your `.chain` scripts! Chain will automatically
    compile, cache, and execute it, sharing variables seamlessly via POSIX Shared
    Memory.
  - **CREL (Chain Runtime Extension Library):** A library that lets developers 
    create low-level, native libraries using C++ and load them directly into Chain.

#### Built-in Standard Libraries

Chain now comes with powerful built-in modules:

  - **Webview Engine (`webview.`):** Build Native Desktop Apps! Render modern UIs 
    using HTML/CSS while using Chain as the high-performance backend.
  - **GUI Engine (`gui_`):** Powered by Raylib. Create windows, draw shapes, render
    text, handle mouse/keyboard inputs natively, and use Scissor/Clipping modes.
  - **Audio Engine (`audio.`):** Stream MP3/WAV music or load multiple Sound Effects
    (SFX) into RAM for game development. Includes Spectrum/EQ analysis.
  - **Networking (`net.`):** Low-level TCP socket support. Create clients, servers,
    send/receive data, and scan ports — cross-platform (Linux & Windows).
  - **System & OS (`os.`, `io.`):** Execute shell commands (`os.exec`), read/write files,
    and manage environment variables.
  - **Math & Strings (`math.`, `str.`):** Fully equipped for Scientific Computing 
    (Trigonometry, Exponential, Logarithms), random number generation, string
    splitting, replacing, and trimming.
  - **XML Parsing (`xml_`):** Read and Write parsed XML files and extract values. 

### The Nebania Ecosystem

Chain is the core of a growing suite of tools built for NebulaOS, collectively
known as the **Penthouse Apps**:

  - **Bellhop** — A custom shell for NebulaOS. Supports job control, command history,
    aliases, tab completion, and a Chain plugin system (`.chain` scripts as shell
    extensions; `.link` scripts are still accepted but deprecated).
    
  - **Martini** — A terminal-based file manager. Browse the filesystem, open `.chain`
    scripts in Sucrose, or run them with `chainlang` directly. The built-in Concierge
    system analyses `.chain` files using a scoring algorithm (`sick.chain`) to detect
    Bellhop plugins and install them on the spot — all from a TUI interface.
    
  - **Sucrose** — A terminal text editor with full syntax highlighting for Chain,
    configurable via a `syntax.chain` config file. No recompile needed to add new
    language support.

Together, these tools form the NebulaOS user environment: a shell, a file manager,
and an editor all designed to work natively with Chain. The Penthouse Apps are
available for download at https://nebania.site

### Runtime Modes

1.  **File Mode:** Execute `.chain` script files (`.link` files are also accepted, but
    will print a deprecation warning).
2.  **Interactive REPL:** A smart shell that supports multi-line blocks (Shift+Enter
    logic).
3.  **AST Debug Mode:** Run with `chainlang --debug <file>` to visualize the Abstract
    Syntax Tree.
4.  **Time Profiler Mode:** Run with `chainlang --time <file>` to measure execution time.
5.  **GC Monitor Mode:** Run with `chainlang --gc <file>` to monitor the Garbage Collector in real-time and print memory sweep logs to the terminal.

### Installation & Build

Ensure you have a C++ compiler installed (G++ recommended) and Raylib installed
on your system.

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/Pilot0253/chain-lang.git
    cd chain-lang
    ```
2.  **Install Dependencies (Linux):**
    ```bash
    sudo apt install libraylib-dev libwebkit2gtk-4.0-dev
    ```
3.  **Build the project:**
    ```bash
    # POSIX (Linux, BSD, OSX...)
    make          # for a standard build
    make debug    # for a build with debug symbols (for development)
    make release  # for an optimized build
    
    # Windows
    make -f makefile.win         # for a standard build
    make debug -f makefile.win   # for a build with debug symbols
    make -f makefile.win release # for an optimized build
    ```
4.  **Optional — install to PATH:**
    ```bash
    sudo make install
    ```
    This keeps the `chainlang` binary in this project folder and symlinks it into
    `/usr/local/bin` as both `chain` and `chainlang`, so you can run either command
    from anywhere.

### Usage

**To run a file ending in .chain:**
```bash
chainlang examples/hello.chain
```

**To run a script and monitor the Garbage Collector (GC):**
```bash
chainlang --gc examples/heavy_loop.chain
```

**.link files are still supported for backwards compatibility, but running one
prints a soft deprecation warning asking you to switch to .chain:**
```bash
chainlang examples/hello.link
```



