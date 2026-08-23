#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <chrono> 
#include "lexer.h"
#include "parser.h"
#include "runtime.h" 
#include "help.h"
#include "repl_core.h"
#include "gc.h"

bool isBlockStart(const std::string& line) {
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos) return false; 
    
    std::string s = line.substr(start);
    size_t end = s.find_last_not_of(" \t");
    if (end != std::string::npos) {
        s = s.substr(0, end + 1);
    }
    if (s.rfind("if", 0) == 0) return true;
    if (s.rfind("while", 0) == 0) return true;
    if (s.rfind("for", 0) == 0) return true;
    if (s.rfind("func", 0) == 0) return true;
    if (s.rfind("app", 0) == 0) return true;
    if (s.rfind("window", 0) == 0) return true;
    if (s.rfind("elif", 0) == 0) return true;
    if (s.rfind("else", 0) == 0) return true;
    if (!s.empty() && s.back() == '{') return true;
    if (!s.empty() && s.back() == '[') return true;
    
    return false;
}

void printVisualError(const std::string& source, const ChainError& e) {
    std::cout << "\n\033[1;31m[Chain Error]\033[0m " << e.message << "\n";
    std::cout << "  --> line " << e.line << ":" << e.col << "\n\n";

    std::vector<std::string> lines;
    std::string currentLine;
    for (char c : source) {
        if (c == '\n') { lines.push_back(currentLine); currentLine = ""; }
        else { currentLine += c; }
    }
    lines.push_back(currentLine);

    if (e.line > 1 && e.line - 2 < (int)lines.size()) {
        std::cout << " " << (e.line - 1) << " | " << lines[e.line - 2] << "\n";
    }
    
    if (e.line > 0 && e.line - 1 < (int)lines.size()) {
        std::cout << "\033[1;31m>\033[0m" << e.line << " | " << lines[e.line - 1] << "\n";
        
        std::cout << "  " << std::string(std::to_string(e.line).length(), ' ') << " | ";
        for (int i = 1; i < e.col; i++) std::cout << " ";
        std::cout << "\033[1;33m^^^ Here :)\033[0m\n";
    }
    std::cout << "\n";
}

void run(Runtime& runtime, const std::string& source, bool isDebug) {
    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse(); 

        if (isDebug) {
            std::cout << "\n--- DEBUG: AST STRUCTURE ---\n";
            program->print();
            std::cout << "----------------------------\n";
        }

        runtime.execute(std::move(program)); 

    } catch (const ChainError& e) {
        printVisualError(source, e); 
    } catch (const std::exception& e) {
        std::cerr << "\033[1;31m[Runtime Error]\033[0m " << e.what() << std::endl;
    }
}

int main(int argc, char** argv) {
    Runtime runtime; 
    std::string filename;
    std::vector<std::string> libraries;
    int flagsCount = 0;
    bool debugMode = false;
    bool timeMode = false;

    auto hasExtension = [](const std::string& name, const std::string& ext) {
        if (name.size() < ext.size())
            return false;

        return name.compare(name.size() - ext.size(),ext.size(),ext) == 0;
    };

    for (int i = 1; i < argc; i++) {

        std::string arg = argv[i];

        if (arg == "--debug") {
            debugMode = true;
        }

        else if (arg == "--time") {
            timeMode = true;
        }

        else if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        }

        else if (arg == "--version" || arg == "-v") {
            printVersion();
            return 0;
        }

        else if (arg == "--debug" || arg == "-d") {
            debugMode = true;
        }

        else if (arg == "--time" || arg == "-t") {
            timeMode = true;
        }

        else if (arg == "--gc" || arg == "-gc") {
            gc.debugGC = true;
            flagsCount++;
        }

        // Load a C++ runtime library
        else if (arg == "--crel") {

            if (i + 1 >= argc) {
                std::cerr
                    << "Error: --crel requires a library path.\n";

                return 1;
            }

            std::string library = argv[++i];

            libraries.push_back(library);
        }

        else {
            if (filename.empty()) {
                filename = arg;
            }
            else {
                std::cerr
                    << "Error: Unexpected argument: "
                    << arg << "\n";

                return 1;
            }
        }
    }
    for (const auto& library : libraries) {
        std::cout << "[CREL] Loading: " << library << std::endl;

        if (!runtime.loadLibrary(library)) {
            std::cerr << "[CREL] Failed to load: " << library << std::endl;

            return 1;
        }

        std::cout << "[CREL] Loaded successfully: " << library << std::endl;
    }

    if (argc < 2 || argc == 1 + flagsCount) {
        std::cout << "Nebania Chain v0.7.0 (Analyzer)" << std::endl;
        if (debugMode) std::cout << "[DEBUG MODE ACTIVE]" << std::endl;
        if (timeMode) std::cout << "[TIME PROFILER ACTIVE]" << std::endl;
        std::cout << "Type 'exit' to quit." << std::endl;
        
        ReplEditor editor;
        std::string inputBuffer;
        int indentLevel = 0; 

        while (true) {
            std::string prompt = (inputBuffer.empty()) ? "\033[1;32mchain>\033[0m " : "\033[1;33m...  \033[0m ";
            std::string line = editor.readLine(prompt, indentLevel);

            if (line == "exit") break;
            if (!line.empty() && line.back() == '{') {
                indentLevel++;
            }
            if (inputBuffer.empty()) {
                if (isBlockStart(line)) {
                    inputBuffer += line + "\n";
                    editor.addToHistory(line); 
                } else {
                    if (!line.empty()) {
                        editor.addToHistory(line);
                        run(runtime, line, debugMode);
                    }
                }
            } else {
                if (line.empty()) { 
                    editor.addToHistory(inputBuffer); 
                    run(runtime, inputBuffer, debugMode);
                    inputBuffer.clear();
                    indentLevel = 0; 
                } else {
                    if (line.find("}") != std::string::npos) {
                        if (indentLevel > 0) indentLevel--;
                    }
                    inputBuffer += line + "\n";
                }
            }
        }
        return 0;
    }
    for(int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg != "--debug" && arg != "-d" && arg != "--time" && arg != "-t" && arg != "--gc" && arg != "-gc") {
            filename = arg;
            break;
        }
    }

    if (filename.empty()) {
        std::cout << "Error: No file specified." << std::endl;
        return 1;
    }

    if (hasExtension(filename, ".link")) {
        std::cout << "[Tip] This file seems to use the .link extension, which has been deprecated!\nNo worries, Nebania HQ recommends renaming this to a .chain file.\nIt's quick and won't do any harm to your code." << std::endl;
    }

    std::ifstream file(filename);
    if(!file){
        std::cout << "No such file or directory: " << filename << std::endl;
        return 1;
    }

    std::string source((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    if (timeMode) {
        runtime.enableProfiling = true; 
        
        auto start = std::chrono::high_resolution_clock::now();
        run(runtime, source, debugMode);
        auto end = std::chrono::high_resolution_clock::now();
        
        std::chrono::duration<double, std::milli> duration = end - start;
        std::cout << "\n\033[1;32m========================================\033[0m\n";
        std::cout << "\033[1;33m[ChainLang Profiler]\033[0m Total Execution finished in: \033[1;36m" 
                  << duration.count() << " ms\033[0m\n";
        std::cout << "\033[1;32m========================================\033[0m\n";
    } else {
        run(runtime, source, debugMode);
    }

    return 0;        
} 