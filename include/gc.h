#pragma once
#include <iostream>
#include <vector>
#include <mutex>

struct Value;
struct Environment;

struct GCObject {
    bool isMarked = false;
    GCObject* next = nullptr; 
    virtual ~GCObject() = default;
    virtual void trace() = 0; 
};

class GarbageCollector {
private:
    GCObject* head = nullptr;      
    std::mutex gcMutex;            
    
    size_t bytesAllocated = 0;
    size_t nextGC = 1024 * 1024;   
    size_t totalObjects = 0;

public:
    bool debugGC = false; 

    std::vector<Environment**> roots;
    std::vector<Value*> tempRoots; 

    void markValue(Value& val);
    void markObject(GCObject* obj) {
        if (obj == nullptr || obj->isMarked) return;
        obj->isMarked = true;
        obj->trace();
    }

    void pushTempRoot(Value* val) {
        std::lock_guard<std::mutex> lock(gcMutex);
        tempRoots.push_back(val);
    }
    
    void popTempRoot() {
        std::lock_guard<std::mutex> lock(gcMutex);
        tempRoots.pop_back();
    }

    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        std::lock_guard<std::mutex> lock(gcMutex);
        
        T* object = new T(std::forward<Args>(args)...);
        object->isMarked = false;
        object->next = head;
        head = object;
        
        bytesAllocated += sizeof(T);
        totalObjects++;
        
        return object;
    }

    // Fungsi Safepoint
    void checkGC() {
        if (bytesAllocated >= nextGC) {
            collect();
        }
    }

    void collect();
};

extern GarbageCollector gc;