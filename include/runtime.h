#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set> 
#include <functional>
#include <variant>
#include "types.h"
#include "env.h"
#include "parser.h" 
#include <CREL/CREL.hpp>
#include "crel_loader.h"

using NativeFn = std::function<Obj(const std::vector<Obj>&)>;

class Runtime {
private:
    Environment* globalEnv;
    Environment* currentEnv;
    
    // Registry Map
    std::unordered_map<std::string, NativeFn> nativeRegistry;
    std::unordered_map<std::string, FuncDecl*> functionRegistry;

    std::vector<std::unique_ptr<Program>> loadedPrograms;
    std::unordered_set<std::string> importedFiles; 

    // Helper Functions
    void initNativeFunctions();
    CREL createAPI();
    std::string objToString(const Obj& o);
    std::string getAnsiColor(const std::string& color);
    void printObj(const Obj& val);
    
    // Logic Helper
    bool isTruthy(const Obj& o);
    FuncDecl* findMethod(LinkClass* klass, const std::string& name);

public:
    Runtime(); // Constructor

    bool enableProfiling = false; // Profiling flag

    // Main execution function
    bool loadLibrary(const std::string& path);
    void run(const std::string& source, bool debug);
    void runStatement(Stmt* stmt);
    Obj evaluateExpr(Expr* expr);
    void execute(std::unique_ptr<Program> program); 
};
