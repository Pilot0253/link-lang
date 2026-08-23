#pragma once
#include <variant>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include "os.h" 
#include "gc.h" 

struct Stmt;        
struct LinkClass;   
struct LinkInstance;
struct FuncDecl;           
struct Environment;        

struct LinkFunction : public GCObject {
    FuncDecl* declaration;
    Environment* closure; 

    void trace() override;
};

using List = std::vector<Value>;
using Dict = std::unordered_map<std::string, Value>;

struct ChainList : public GCObject {
    List elements;
    void trace() override;
};

struct ChainDict : public GCObject {
    Dict map;
    void trace() override;
};

struct LinkClass : public GCObject {
    std::string name;
    LinkClass* superclass; 
    std::unordered_map<std::string, Stmt*> methods; 

    void trace() override;
};

struct LinkInstance : public GCObject {
    LinkClass* klass;        
    std::unordered_map<std::string, Value> fields;  

    void trace() override;
};

struct Value {
    using ValVariant = std::variant<
        std::monostate, int, double, std::string, char, bool, 
        ChainList*, ChainDict*, LinkClass*, LinkInstance*, LinkFunction*
    >;
    
    ValVariant as;

    Value() : as(std::monostate{}) {}
    Value(const Value& other) = default;
    Value(Value&& other) = default;
    Value& operator=(const Value& other) = default;
    Value& operator=(Value&& other) = default;
    Value(int v) : as(v) {}
    Value(double v) : as(v) {}
    Value(std::string v) : as(v) {}
    Value(const char* v) : as(std::string(v)) {} 
    Value(char v) : as(v) {}
    Value(bool v) : as(v) {}
    
    // Constructor untuk tipe GC
    Value(ChainList* v) : as(v) {}
    Value(ChainDict* v) : as(v) {}
    Value(LinkClass* v) : as(v) {}
    Value(LinkInstance* v) : as(v) {}
    Value(LinkFunction* v) : as(v) {} 
};

using Obj = Value;

struct ReturnException { Obj value; ReturnException(Obj v) : value(v) {} };
struct BreakException {}; 
struct ContinueException {}; 
struct RuntimeException { std::string message; RuntimeException(std::string msg) : message(msg) {} };

struct ChainError : public std::exception {
    int line;
    int col;
    std::string message;
    ChainError(int l, int c, std::string msg) : line(l), col(c), message(msg) {}
    const char* what() const noexcept override { return message.c_str(); }
};

inline void printObj(const Obj& val) {
    if (std::holds_alternative<int>(val.as)) std::cout << std::get<int>(val.as);
    else if (std::holds_alternative<double>(val.as)) std::cout << std::get<double>(val.as);
    else if (std::holds_alternative<std::string>(val.as)) std::cout << Sys::unescape(std::get<std::string>(val.as));
    else if (std::holds_alternative<char>(val.as)) std::cout << std::get<char>(val.as);
    else if (std::holds_alternative<bool>(val.as)) std::cout << (std::get<bool>(val.as) ? "true" : "false");
    else if (std::holds_alternative<ChainList*>(val.as)) {
        auto list = std::get<ChainList*>(val.as);
        std::cout << "[";
        for (size_t i = 0; i < list->elements.size(); ++i) {
            printObj(list->elements[i]);
            if (i < list->elements.size() - 1) std::cout << ", ";
        }
        std::cout << "]";
    }
    else if (std::holds_alternative<ChainDict*>(val.as)) {
        auto dict = std::get<ChainDict*>(val.as);
        std::cout << "{";
        int i = 0;
        for (const auto& pair : dict->map) {
            std::cout << "\"" << pair.first << "\": ";
            printObj(pair.second);
            if (i < (int)dict->map.size() - 1) std::cout << ", ";
            i++;
        }
        std::cout << "}";
    }
    else if (std::holds_alternative<LinkClass*>(val.as)) {
        std::cout << "<Class " << std::get<LinkClass*>(val.as)->name << ">";
    }
    else if (std::holds_alternative<LinkInstance*>(val.as)) {
        auto instance = std::get<LinkInstance*>(val.as);
        std::cout << "<Instance " << instance->klass->name << ">";
    }
    else if (std::holds_alternative<LinkFunction*>(val.as)) {
        std::cout << "<Function>";
    }
    else std::cout << "nil";
}