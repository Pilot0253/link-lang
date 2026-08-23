#include "gc.h"
#include "types.h"
#include "env.h"

GarbageCollector gc;


void ChainList::trace() {
    for (auto& val : elements) {
        gc.markValue(val);
    }
}

void ChainDict::trace() {
    for (auto& pair : map) {
        gc.markValue(pair.second);
    }
}

void LinkClass::trace() {
    if (superclass) gc.markObject(superclass);
}

void LinkInstance::trace() {
    if (klass) gc.markObject(klass);
    for (auto& pair : fields) {
        gc.markValue(pair.second);
    }
}

void LinkFunction::trace() {
    if (closure) gc.markObject(closure); 
}


void GarbageCollector::markValue(Value& val) {
    if (std::holds_alternative<ChainList*>(val.as)) markObject(std::get<ChainList*>(val.as));
    else if (std::holds_alternative<ChainDict*>(val.as)) markObject(std::get<ChainDict*>(val.as));
    else if (std::holds_alternative<LinkClass*>(val.as)) markObject(std::get<LinkClass*>(val.as));
    else if (std::holds_alternative<LinkInstance*>(val.as)) markObject(std::get<LinkInstance*>(val.as));
    else if (std::holds_alternative<LinkFunction*>(val.as)) markObject(std::get<LinkFunction*>(val.as));
}

void GarbageCollector::collect() {
    std::lock_guard<std::mutex> lock(gcMutex); 

    for (Environment** envPtr : roots) {
        if (*envPtr != nullptr) {
            markObject(*envPtr);
        }
    }
    
    for (Value* valPtr : tempRoots) {
        if (valPtr != nullptr) {
            markValue(*valPtr);
        }
    }

    GCObject** object = &head;
    size_t sweptCount = 0;

    while (*object != nullptr) {
        if (!(*object)->isMarked) {
            GCObject* unreached = *object;
            *object = unreached->next; 
            delete unreached; 
            sweptCount++;
            totalObjects--;
        } else {
            (*object)->isMarked = false;
            object = &(*object)->next;
        }
    }

    bytesAllocated = totalObjects * sizeof(GCObject); 
    nextGC = bytesAllocated * 2;
    if (nextGC < 1024 * 1024) nextGC = 1024 * 1024; 
    
    if (debugGC) {
        std::cout << "[GC] Collected " << sweptCount << " objects. Total remaining: " << totalObjects << "\n";
    }
}