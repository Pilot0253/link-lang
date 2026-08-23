#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h> 
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <wait.h>
#include <dlfcn.h>
#endif

#include <iostream>
#include <variant> 
#include <string> 
#include <vector>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <thread>
#include <future>
#include <chrono>
#include <filesystem>
#include <random> 
#include "types.h"  
#include "env.h"    
#include "lexer.h" 
#include "parser.h" 
#include "os.h" 
#include "link_str.h"
#include "link_math.h"
#include "link_net.h"
#include "runtime.h"
#include "link_gui.h"
#include "link_wrapper.h"
#include "link_xml.h"
#include "link_audio.h"

#ifndef CHAIN_DISABLE_WEBVIEW
#include "link_webview.h"
#endif

static std::mutex userSyncMutex; 

bool isEqualObj(const Obj& a, const Obj& b) {
    if (a.as.index() != b.as.index()) return false;
    if (std::holds_alternative<int>(a.as)) return std::get<int>(a.as) == std::get<int>(b.as);
    if (std::holds_alternative<double>(a.as)) return std::get<double>(a.as) == std::get<double>(b.as);
    if (std::holds_alternative<std::string>(a.as)) return std::get<std::string>(a.as) == std::get<std::string>(b.as);
    if (std::holds_alternative<bool>(a.as)) return std::get<bool>(a.as) == std::get<bool>(b.as);
    return false;
}

struct ProfilerTimer {
    std::string name;
    bool active;
    std::chrono::high_resolution_clock::time_point start;

    ProfilerTimer(const std::string& n, bool a) : name(n), active(a) {
        if (active) start = std::chrono::high_resolution_clock::now();
    }
    ~ProfilerTimer() {
        if (active) {
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> diff = end - start;

            if (diff.count() > 0.01) {
                std::cout << "\033[1;35m[Profiler]\033[0m Exec " << name 
                          << " took: \033[1;36m" << diff.count() << " ms\033[0m\n";
            }
        }
    }
};

namespace fs= std::filesystem; 
static std::random_device rd;
static std::mt19937 gen(rd()); 

Runtime::Runtime() {
    globalEnv = gc.allocate<Environment>(nullptr);
    currentEnv = globalEnv;

    gc.roots.push_back(&globalEnv);
    gc.roots.push_back(&currentEnv);

    initNativeFunctions(); 
}

std::string Runtime::getAnsiColor(const std::string& color) {
    if (color == "red")     return "\033[31m";
    if (color == "green")   return "\033[32m";
    if (color == "yellow")  return "\033[33m";
    if (color == "blue")    return "\033[34m";
    if (color == "magenta") return "\033[35m";
    if (color == "cyan")    return "\033[36m";
    if (color == "white")   return "\033[37m";
    return "";
}

std::string Runtime::objToString(const Obj& val) {
    if (std::holds_alternative<int>(val.as)) {
        return std::to_string(std::get<int>(val.as));
    }
    if (std::holds_alternative<double>(val.as)) {
        std::string s = std::to_string(std::get<double>(val.as));
        s.erase(s.find_last_not_of('0') + 1, std::string::npos);
        if (s.back() == '.') s.pop_back();
        return s;
    }
    if (std::holds_alternative<std::string>(val.as)) {
        return std::get<std::string>(val.as);
    }
    if (std::holds_alternative<bool>(val.as)) {
        return std::get<bool>(val.as) ? "true" : "false";
    }
    
    if (std::holds_alternative<ChainList*>(val.as)) {
        auto list = std::get<ChainList*>(val.as);
        std::string res = "[";
        for (size_t i = 0; i < list->elements.size(); ++i) {
            if (std::holds_alternative<std::string>(list->elements[i].as)) res += "\"" + objToString(list->elements[i]) + "\"";
            else res += objToString(list->elements[i]);
            if (i < list->elements.size() - 1) res += ", ";
        }
        res += "]";
        return res;
    }
    if (std::holds_alternative<ChainDict*>(val.as)) {
        auto dict = std::get<ChainDict*>(val.as);
        std::string res = "{";
        int i = 0;
        for (const auto& pair : dict->map) {
            res += "\"" + pair.first + "\": ";
            if (std::holds_alternative<std::string>(pair.second.as)) res += "\"" + objToString(pair.second) + "\"";
            else res += objToString(pair.second);
            if (i < (int)dict->map.size() - 1) res += ", ";
            i++;
        }
        res += "}";
        return res;
    }
    
    return ""; 
}

char getChar() {
    #ifdef _WIN32
        return _getch(); 
    #else
        struct termios oldattr, newattr;
        char ch;
        tcgetattr(STDIN_FILENO, &oldattr);
        newattr = oldattr;
        newattr.c_lflag &= ~(ICANON | ECHO); 
        tcsetattr(STDIN_FILENO, TCSANOW, &newattr);
        ch = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldattr); 
        return ch;
    #endif
}
CREL Runtime::createAPI()
{
    return CREL(
        [this](const std::string& name, NativeFn fn)
        {
            nativeRegistry[name] = std::move(fn);
        }
    );
}

bool Runtime::loadLibrary(const std::string& path)
{
    CREL api(
        [this](const std::string& name, NativeFn fn) {
            nativeRegistry[name] = std::move(fn);
        }
    );

    return LibraryLoader::load(path, api);
}

void Runtime::initNativeFunctions() {
    
    nativeRegistry["print"] = [this](const std::vector<Obj>& args) -> Obj {
        for (size_t i = 0; i < args.size(); ++i) {
            std::string rawOutput = objToString(args[i]);
            std::cout << Sys::unescape(rawOutput); 
            
            if (i < args.size() - 1) std::cout << " ";
        }

        std::cout << std::endl; 
        return Obj(0);
    };
    
    nativeRegistry["input"] = [this](const std::vector<Obj>& args) -> Obj {
        if (!args.empty()) {
            std::cout << Sys::unescape(objToString(args[0])) << std::flush;
        }
        std::string line; 
        std::getline(std::cin, line);
        line = SysString::trim(line);

        try {
            size_t pos; 
            int val = std::stoi(line, &pos);
            if (pos == line.length()) return Obj(val);
        } catch (...) {}

        try {
            size_t pos;
            double val = std::stod(line, &pos);
            if (pos == line.length()) return Obj(val);
        } catch (...) {}
        
        return Obj(line);
    };

    // ==========================================
    // 1. NETWORKING MODULE (SysNet)
    // ==========================================
    nativeRegistry["net.socket"] = [](const std::vector<Obj>& args) -> Obj {
        int s = SysNet::createSocket();
        return Obj(s);
    };

    nativeRegistry["net.server"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj(-1);
        int port = 80;
        if (std::holds_alternative<int>(args[0].as)) port = std::get<int>(args[0].as);
        else if (std::holds_alternative<double>(args[0].as)) port = (int)std::get<double>(args[0].as);
        int s = SysNet::createSocket();
        if (s == -1) return Obj(-1);
        if (SysNet::bindAndListen(s, port)) {
            std::cout << "[INFO] Server listening on port " << port << "...\n";
            return Obj(s);
        }
        return Obj(-1);
    };

    // 2. Accept Client (Blocking)
    nativeRegistry["net.accept"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj(-1);
        if (std::holds_alternative<int>(args[0].as)) {
            int serverSock = std::get<int>(args[0].as);
            int clientSock = SysNet::acceptClient(serverSock);
            return Obj(clientSock);
        }
        return Obj(-1);
    };

    // 3. Client Connect
    nativeRegistry["net.connect"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 2) return Obj(-1);
        std::string ip = "127.0.0.1";
        int port = 80;
        if (std::holds_alternative<std::string>(args[0].as)) ip = std::get<std::string>(args[0].as);
        if (std::holds_alternative<int>(args[1].as)) port = std::get<int>(args[1].as);

        int s = SysNet::createSocket();
        if (SysNet::connectSocket(s, ip, port)) return Obj(s);
        return Obj(-1);
    };

    // 4. Send Data
    nativeRegistry["net.send"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 2) return Obj(false);
        if (std::holds_alternative<int>(args[0].as)) {
            return Obj(SysNet::sendData(std::get<int>(args[0].as), objToString(args[1])));
        }
        return Obj(false);
    };

    // 5. Receive Data
    nativeRegistry["net.recv"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj("");
        if (std::holds_alternative<int>(args[0].as)) {
            return Obj(SysNet::receiveData(std::get<int>(args[0].as)));
        }
        return Obj("");
    };

    // 6. Close
    nativeRegistry["net.close"] = [](const std::vector<Obj>& args) -> Obj {
        if (!args.empty() && std::holds_alternative<int>(args[0].as)) {
            SysNet::closeSocket(std::get<int>(args[0].as));
        }
        return Obj(0);
    };

    // ==========================================
    // 2. TYPE CASTING & CONVERSION
    // ==========================================
    nativeRegistry["int"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj(0);
        const Obj& val = args[0];
        
        if (std::holds_alternative<int>(val.as)) return val;
        if (std::holds_alternative<double>(val.as)) return Obj((int)std::get<double>(val.as));
        if (std::holds_alternative<bool>(val.as)) return Obj(std::get<bool>(val.as) ? 1 : 0);
        if (std::holds_alternative<std::string>(val.as)) {
            try { return Obj(std::stoi(std::get<std::string>(val.as))); } 
            catch(...) { return Obj(0); }
        }
        return Obj(0);
    };
    nativeRegistry["char"] = [](const std::vector<Obj>& args) -> Obj {
    if (args.empty()) return Obj("");
    int code = 0;
    if (std::holds_alternative<int>(args[0].as)) code = std::get<int>(args[0].as);
    else if (std::holds_alternative<double>(args[0].as)) code = (int)std::get<double>(args[0].as);
    
    std::string s(1, (char)code);
    return Obj(s);
    };
    nativeRegistry["float"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj(0.0);
        const Obj& val = args[0];
        if (std::holds_alternative<double>(val.as)) return val;
        if (std::holds_alternative<int>(val.as)) return Obj((double)std::get<int>(val.as));
        if (std::holds_alternative<std::string>(val.as)) {
            try { return Obj(std::stod(std::get<std::string>(val.as))); } 
            catch(...) { return Obj(0.0); }
        }
        return Obj(0.0);
    };
    nativeRegistry["str"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj("");
        return Obj(objToString(args[0]));
    };
    // ==========================================
    // 3. SYSTEM & IO
    // ==========================================
    nativeRegistry["term.getch"] = [](const std::vector<Obj>& args) -> Obj {
        char c = getChar(); // Ensure getChar() is visible here
        return Obj(std::string(1, c));
    };
    nativeRegistry["term.color"] = [this](const std::vector<Obj>& args) -> Obj {
    if (args.empty()) return Obj("");
    std::string colorName = objToString(args[0]);
    return Obj(getAnsiColor(colorName)); 
	};
    nativeRegistry["term.reset"] = [](const std::vector<Obj>& args) -> Obj {
        return Obj("\033[0m");
    };
    nativeRegistry["time.sleep"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj(0);
        int ms = 0;
        if (std::holds_alternative<int>(args[0].as)) ms = std::get<int>(args[0].as);
        else if (std::holds_alternative<double>(args[0].as)) ms = (int)std::get<double>(args[0].as);
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return Obj(0);
    };
    nativeRegistry["os.exec"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj("");
        return Obj(Sys::exec(objToString(args[0]).c_str())); 
    };
    nativeRegistry["os.cwd"] = [](const std::vector<Obj>& args) -> Obj {
        return Obj(fs::current_path().string());
    };
    nativeRegistry["os.getenv"] = [this](const std::vector<Obj>& args) -> Obj {
         if (args.empty()) return Obj("");
         return Obj(Sys::getEnv(objToString(args[0])));
    };
	nativeRegistry["os.unescape"] = [this](const std::vector<Obj>& args) -> Obj {
    if (args.empty()) return Obj("");
    return Obj(Sys::unescape(objToString(args[0])));
    };
    nativeRegistry["os.date"] = [this](const std::vector<Obj>& args) -> Obj {
        std::time_t t = std::time(nullptr);
        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "%H:%M", std::localtime(&t));
        return Obj(std::string(buffer));
    };
    
    // ==========================================
    // 4. FILESYSTEM (FS) & IO
    // ==========================================
    nativeRegistry["io.read"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj("");
        if (std::holds_alternative<std::string>(args[0].as)) {
            std::string path = std::get<std::string>(args[0].as);
            if (!Sys::fileExists(path)) return Obj(""); // Or throw an error
            return Obj(Sys::readFile(path));
        }
        return Obj("");
    };
    nativeRegistry["io.exists"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj(false);
        if (std::holds_alternative<std::string>(args[0].as)) {
            return Obj(Sys::fileExists(std::get<std::string>(args[0].as)));
        }
        return Obj(false);
    };
    nativeRegistry["fs.list"] = [](const std::vector<Obj>& args) -> Obj {
        std::string path = ".";
        if (!args.empty() && std::holds_alternative<std::string>(args[0].as)) {
            path = std::get<std::string>(args[0].as);
        }
        auto list = gc.allocate<ChainList>();
        try {
            for (const auto& entry : fs::directory_iterator(path)) {
                list->elements.push_back(Value(entry.path().filename().string()));
            }
        } catch(...) {}
        return Obj(list);
    };
    nativeRegistry["fs.isdir"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj(false);
        if (std::holds_alternative<std::string>(args[0].as)) {
             try { return Obj(fs::is_directory(std::get<std::string>(args[0].as))); }
             catch(...) { return Obj(false); }
        }
        return Obj(false);
    };
    nativeRegistry["fs.mkdir"] = [](const std::vector<Obj>& args) -> Obj {
        if (!args.empty() && std::holds_alternative<std::string>(args[0].as)) {
            try { fs::create_directory(std::get<std::string>(args[0].as)); } catch(...) {}
        }
        return Obj(0);
    };

    // ==========================================
    // 5. STRING LIBRARY
    // ==========================================
    nativeRegistry["len"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj(0);
        if (std::holds_alternative<std::string>(args[0].as)) 
            return Obj((int)std::get<std::string>(args[0].as).length());
        if (std::holds_alternative<ChainList*>(args[0].as))
            return Obj((int)std::get<ChainList*>(args[0].as)->elements.size());
         if (std::holds_alternative<ChainDict*>(args[0].as))
            return Obj((int)std::get<ChainDict*>(args[0].as)->map.size());
        return Obj(0);
    };

    nativeRegistry["str.sub"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 3) return Obj("");
        if (std::holds_alternative<std::string>(args[0].as) &&
            std::holds_alternative<int>(args[1].as) &&
            std::holds_alternative<int>(args[2].as)) {
            return Obj(SysString::substring(
                std::get<std::string>(args[0].as), 
                std::get<int>(args[1].as), 
                std::get<int>(args[2].as)
            ));
        }
        return Obj("");
    };

    nativeRegistry["str.lower"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj("");
        return Obj(SysString::toLower(objToString(args[0])));
    };
    
    nativeRegistry["str.upper"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj("");
        return Obj(SysString::toUpper(objToString(args[0])));
    };

    nativeRegistry["str.trim"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj("");
        if (std::holds_alternative<std::string>(args[0].as)) {
            return Obj(SysString::trim(std::get<std::string>(args[0].as)));
        }
        return Obj("");
    };

    nativeRegistry["str.replace"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 3) return Obj("");
        return Obj(SysString::replace(
            objToString(args[0]), objToString(args[1]), objToString(args[2])
        ));
    };
    
    nativeRegistry["str.split"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 2) return Obj(gc.allocate<ChainList>());
        auto vec = SysString::split(objToString(args[0]), objToString(args[1]));
        auto list = gc.allocate<ChainList>();
        for(const auto& v : vec) list->elements.push_back(Value(v));
        return Obj(list);
    };

    nativeRegistry["str.contains"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 2) return Obj(false);
        return Obj(Sys::contains(objToString(args[0]), objToString(args[1])));
    };
    
    nativeRegistry["str.pop"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj("");
        std::string input = objToString(args[0]);
        return Obj(SysString::pop(input));
    };
    nativeRegistry["str.starts_with"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 2) return Obj(false);
        std::string full = objToString(args[0]);
        std::string prefix = objToString(args[1]);
        return Obj(full.rfind(prefix, 0) == 0);
    };

    nativeRegistry["str.substr"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 2) return Obj("");
        std::string str = objToString(args[0]);
        
        int start = 0;
        if (std::holds_alternative<int>(args[1].as)) start = std::get<int>(args[1].as);
        else if (std::holds_alternative<double>(args[1].as)) start = (int)std::get<double>(args[1].as);

        if (start < 0 || start >= str.length()) return Obj("");

        if (args.size() >= 3) {
            int length = 0;
            if (std::holds_alternative<int>(args[2].as)) length = std::get<int>(args[2].as);
            else if (std::holds_alternative<double>(args[2].as)) length = (int)std::get<double>(args[2].as);
            
            return Obj(str.substr(start, length));
        }

        return Obj(str.substr(start));
    };

    nativeRegistry["str.merge"] = [this](const std::vector<Obj>& args) -> Obj {
    if (args.size() < 2) return Obj("");
    if (!std::holds_alternative<ChainList*>(args[0].as)) return Obj("");
    auto listPtr = std::get<ChainList*>(args[0].as);
    std::string delimiter = objToString(args[1]);
    std::vector<std::string> strList;
    for (const auto& item : listPtr->elements) {
        strList.push_back(objToString(item));
    }

    return Obj(SysString::merge(strList, delimiter));
   };


    // ==========================================
    // 6. MATH LIBRARY
    // ==========================================
    auto asDouble = [](const Obj& o) -> double {
        if (std::holds_alternative<double>(o.as)) return std::get<double>(o.as);
        if (std::holds_alternative<int>(o.as)) return (double)std::get<int>(o.as);
        if (std::holds_alternative<std::string>(o.as)) {
            try { return std::stod(std::get<std::string>(o.as)); } catch(...) { return 0.0; }
        }
        return 0.0;
    };

    nativeRegistry["math.random"] = [](const std::vector<Obj>& args) -> Obj {
        (void)args;
        std::uniform_real_distribution<> dis(0.0, 1.0);
        return Obj(dis(gen)); 
    };

    nativeRegistry["math.randint"] = [](const std::vector<Obj>& args) -> Obj {
        int minV = 0, maxV = 100;
        if (args.size() >= 1 && std::holds_alternative<int>(args[0].as)) minV = std::get<int>(args[0].as);
        if (args.size() >= 2 && std::holds_alternative<int>(args[1].as)) maxV = std::get<int>(args[1].as);
        std::uniform_int_distribution<> dis(minV, maxV);
        return Obj(dis(gen));
    };

    nativeRegistry["math.pi"] = [](const std::vector<Obj>& args) -> Obj { (void)args; return Obj(SysMath::pi()); };
    nativeRegistry["math.e"]  = [](const std::vector<Obj>& args) -> Obj { (void)args; return Obj(SysMath::e()); };
    
    // Basic Math & Trigonometry
    nativeRegistry["math.sin"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::sin(asDouble(args[0]))); };
    nativeRegistry["math.cos"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::cos(asDouble(args[0]))); };
    nativeRegistry["math.tan"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::tan(asDouble(args[0]))); };
    nativeRegistry["math.sqrt"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::sqrt(asDouble(args[0]))); };
    nativeRegistry["math.abs"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::abs(asDouble(args[0]))); };
    
    nativeRegistry["math.pow"] = [asDouble](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 2) return Obj(0.0);
        return Obj(SysMath::pow(asDouble(args[0]), asDouble(args[1])));
    };

    // Inverse Trigonometry
    nativeRegistry["math.asin"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::asin(asDouble(args[0]))); };
    nativeRegistry["math.acos"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::acos(asDouble(args[0]))); };
    nativeRegistry["math.atan"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::atan(asDouble(args[0]))); };
    nativeRegistry["math.atan2"] = [asDouble](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 2) return Obj(0.0);
        return Obj(SysMath::atan2(asDouble(args[0]), asDouble(args[1])));
    };

    // Hyperbolic
    nativeRegistry["math.sinh"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::sinh(asDouble(args[0]))); };
    nativeRegistry["math.cosh"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::cosh(asDouble(args[0]))); };
    nativeRegistry["math.tanh"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::tanh(asDouble(args[0]))); };

    // Exponential & Logarithm
    nativeRegistry["math.exp"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::exp(asDouble(args[0]))); };
    nativeRegistry["math.log"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::log(asDouble(args[0]))); };
    nativeRegistry["math.log10"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::log10(asDouble(args[0]))); };
    nativeRegistry["math.log2"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::log2(asDouble(args[0]))); };

    // Rounding & Utility
    nativeRegistry["math.ceil"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::ceil(asDouble(args[0]))); };
    nativeRegistry["math.floor"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::floor(asDouble(args[0]))); };
    nativeRegistry["math.round"] = [asDouble](const std::vector<Obj>& args) -> Obj { return args.empty() ? Obj(0.0) : Obj(SysMath::round(asDouble(args[0]))); };
    
    nativeRegistry["math.min"] = [asDouble](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 2) return Obj(0.0);
        return Obj(SysMath::min(asDouble(args[0]), asDouble(args[1])));
    };
    nativeRegistry["math.max"] = [asDouble](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 2) return Obj(0.0);
        return Obj(SysMath::max(asDouble(args[0]), asDouble(args[1])));
    };
    
    // ==========================================
    // 7. COLLECTIONS (List & Dict)
    // ==========================================
    nativeRegistry["list.pop"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj();
        if (std::holds_alternative<ChainList*>(args[0].as)) {
            auto list = std::get<ChainList*>(args[0].as);
            if (!list->elements.empty()) {
                Obj last = list->elements.back();
                list->elements.pop_back();
                return last;
            }
        }
        return Obj();
    };
    nativeRegistry["range"] = [](const std::vector<Obj>& args) -> Obj {
        int limit = 0;
        if (!args.empty() && std::holds_alternative<int>(args[0].as)) limit = std::get<int>(args[0].as);
        auto list = gc.allocate<ChainList>();
        for(int i=0; i<limit; i++) list->elements.push_back(Value(i));
        return Obj(list);
    };
    nativeRegistry["term.clear"] = [](const std::vector<Obj>& args) -> Obj {
        (void)args;
        std::cout << "\033[2J\033[H";
        return Obj(0);
    };
    nativeRegistry["term.move"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() >= 2) {
            int r = std::stoi(objToString(args[0]));
            int c = std::stoi(objToString(args[1]));
            std::cout << "\033[" << r << ";" << c << "H" << std::flush;
        }
        return Obj(0);
    };
    nativeRegistry["os.chdir"] = [this](const std::vector<Obj>& args) -> Obj {
        if (!args.empty()) {
            std::string path = objToString(args[0]);
            try { fs::current_path(path); } 
            catch (...) { std::cout << "Error: Cannot move to '" << path << "'\n"; }
        }
        return Obj(0);
    };
    nativeRegistry["os.setenv"] = [this](const std::vector<Obj>& args) -> Obj {
         if (args.size() >= 2) {
             Sys::setEnv(objToString(args[0]), objToString(args[1]));
         }
         return Obj(0);
    };
    nativeRegistry["io.remove"] = [this](const std::vector<Obj>& args) -> Obj {
        if (!args.empty()) Sys::removeFile(objToString(args[0]));
        return Obj(0);
    };
    nativeRegistry["io.write"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 2) return Obj(0);
        std::string path = objToString(args[0]);
        std::string content = objToString(args[1]);
        if (path == "stdout") std::cout << content << std::flush;
        else Sys::writeFile(path, content, false);
        return Obj(0);
    };
    nativeRegistry["io.append"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 2) return Obj(0);
        Sys::writeFile(objToString(args[0]), objToString(args[1]), true);
        return Obj(0);
    };
    nativeRegistry["list.add"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.size() >= 2 && std::holds_alternative<ChainList*>(args[0].as)) {
            std::get<ChainList*>(args[0].as)->elements.push_back(args[1]);
        }
        return Obj(0);
    };
        nativeRegistry["list.insert"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.size() >= 3 && std::holds_alternative<ChainList*>(args[0].as) && 
            std::holds_alternative<int>(args[1].as)) {
            auto list = std::get<ChainList*>(args[0].as);
            int idx = std::get<int>(args[1].as);
            if (idx >= 0 && idx <= (int)list->elements.size()) {
                list->elements.insert(list->elements.begin() + idx, args[2]);
                return Obj(true);
            }
        }
        return Obj(false);
    };

    nativeRegistry["list.remove"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.size() >= 2 && std::holds_alternative<ChainList*>(args[0].as) && 
            std::holds_alternative<int>(args[1].as)) {
            auto list = std::get<ChainList*>(args[0].as);
            int idx = std::get<int>(args[1].as);
            if (idx >= 0 && idx < (int)list->elements.size()) {
                list->elements.erase(list->elements.begin() + idx);
                return Obj(true);
            }
        }
        return Obj(false);
    };

    // ==========================================
    // 8. DICTIONARY FUNCTION ADDED
    // ==========================================
    nativeRegistry["dict.keys"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj(gc.allocate<ChainList>());
        if (std::holds_alternative<ChainDict*>(args[0].as)) {
            auto dict = std::get<ChainDict*>(args[0].as);
            auto list = gc.allocate<ChainList>();
            for (const auto& pair : dict->map) {
                list->elements.push_back(Value(pair.first)); 
            }
            return Obj(list);
        }
        return Obj(gc.allocate<ChainList>());
    };

    nativeRegistry["dict.has"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() >= 2 && std::holds_alternative<ChainDict*>(args[0].as)) {
            auto dict = std::get<ChainDict*>(args[0].as);
            std::string key = objToString(args[1]);
            return Obj(dict->map.find(key) != dict->map.end());
        }
        return Obj(false);
    };

    nativeRegistry["dict.remove"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() >= 2 && std::holds_alternative<ChainDict*>(args[0].as)) {
            auto dict = std::get<ChainDict*>(args[0].as);
            std::string key = objToString(args[1]);
            return Obj(dict->map.erase(key) > 0); 
        }
        return Obj(false);
    };

    // ==========================================
    // 9. GUI MODULE 
    // ==========================================

    auto asInt = [](const Obj& o) -> int {
        if (std::holds_alternative<int>(o.as)) return std::get<int>(o.as);
        if (std::holds_alternative<double>(o.as)) return (int)std::get<double>(o.as);
        if (std::holds_alternative<std::string>(o.as)) {
            try { return std::stoi(std::get<std::string>(o.as)); } catch(...) { return 0; }
        }
        return 0;
    };
    
    nativeRegistry["gui_get_char"] = [](const std::vector<Obj>& args) -> Obj {
        return Obj(SysGui::getCharPressed());
    };
    nativeRegistry["gui_get_key"] = [](const std::vector<Obj>& args) -> Obj {
        return Obj(SysGui::getKeyPressed());
    };
    nativeRegistry["gui_is_key_down"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj(false);
        return Obj(SysGui::isKeyDown(objToString(args[0])));
    };

    nativeRegistry["gui_measure_text"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 2) return Obj(0);
        std::string txt = objToString(args[0]);
        int size = std::stoi(objToString(args[1]));
        return Obj(SysGui::measureText(txt, size));
    };

    nativeRegistry["gui_debug"] = [](const std::vector<Obj>& args) -> Obj {
        SysGui::enableDebug();
        return Obj(0);
    };

    nativeRegistry["gui_setup"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 3) return Obj(0);
        
        int w = 800; 
        int h = 600;
        
        // Get width (args[0])
        if (std::holds_alternative<int>(args[0].as)) w = std::get<int>(args[0].as);
        else if (std::holds_alternative<double>(args[0].as)) w = (int)std::get<double>(args[0].as);
        
        // Get height (args[1])
        if (std::holds_alternative<int>(args[1].as)) h = std::get<int>(args[1].as);
        else if (std::holds_alternative<double>(args[1].as)) h = (int)std::get<double>(args[1].as);
        
        std::string title = objToString(args[2]);
        
        SysGui::setup(w, h, title);
        return Obj(0);
    };

    nativeRegistry["gui_close"] = [](const std::vector<Obj>& args) -> Obj {
        SysGui::close();
        return Obj(0);
    };

    nativeRegistry["gui_running"] = [](const std::vector<Obj>& args) -> Obj {
        return Obj(SysGui::running());
    };

    nativeRegistry["gui_start"] = [](const std::vector<Obj>& args) -> Obj {
        SysGui::start();
        return Obj(0);
    };
    
    nativeRegistry["gui_present"] = [](const std::vector<Obj>& args) -> Obj {
        SysGui::present();
        return Obj(0);
    };

    nativeRegistry["gui_clear"] = [this](const std::vector<Obj>& args) -> Obj {
    std::string color = "white";
    if (!args.empty()) color = objToString(args[0]); 
    SysGui::clear(color);
    return Obj(0);
    };

    nativeRegistry["gui_text"] = [asInt, this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 3) return Obj(0);
        int x = asInt(args[0]);
        int y = asInt(args[1]);
        std::string text = objToString(args[2]);
        std::string color = "black";
        int size = 20;
        
        if (args.size() >= 4) color = objToString(args[3]);
        if (args.size() >= 5) size = asInt(args[4]);

        SysGui::drawText(x, y, text, color, size);
        return Obj(0);
    };
    
    nativeRegistry["gui_rect"] = [asInt, this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 5) return Obj(0);
        int x = asInt(args[0]);
        int y = asInt(args[1]);
        int w = asInt(args[2]);
        int h = asInt(args[3]);
        std::string color = objToString(args[4]);
        SysGui::drawRect(x, y, w, h, color);
        return Obj(0);
    };

    nativeRegistry["gui_line"] = [asInt, this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 6) return Obj(0);
        int x1 = asInt(args[0]);
        int y1 = asInt(args[1]);
        int x2 = asInt(args[2]);
        int y2 = asInt(args[3]);
        int thick = asInt(args[4]);
        std::string color = objToString(args[5]);
        SysGui::drawLine(x1, y1, x2, y2, thick, color);
        return Obj(0);
    };

	nativeRegistry["gui_is_mouse_down"] = [this](const std::vector<Obj>& args) -> Obj {
        return Obj(SysGui::isMouseDown());
    };

    nativeRegistry["gui.mouse_x"] = [](const std::vector<Obj>& args) -> Obj {
        return Obj(SysGui::getMouseX());
    };
    nativeRegistry["gui.mouse_y"] = [](const std::vector<Obj>& args) -> Obj {
        return Obj(SysGui::getMouseY());
    };

    nativeRegistry["gui_click"] = [](const std::vector<Obj>& args) -> Obj {
        return Obj(SysGui::isMousePressed());
    };
    
    nativeRegistry["gui_key"] = [this](const std::vector<Obj>& args) -> Obj { 
        if (args.empty()) return Obj(false);
        std::string key = objToString(args[0]);
        return Obj(SysGui::isKeyDown(key));
    };
    nativeRegistry["gui_load_font"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 2) return Obj(0);
        std::string path = objToString(args[0]);
        int size = std::stoi(objToString(args[1]));
        SysGui::loadFont(path, size);
        return Obj(0);
    };
    nativeRegistry["gui_is_key_pressed"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj(false);
        std::string key = objToString(args[0]);
        return Obj(SysGui::isKeyPressed(key));
    };
    nativeRegistry["gui.measure_height"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 2) return Obj(0);
        std::string text = objToString(args[0]);
        
        int size = 20; 
        if (std::holds_alternative<int>(args[1].as)) size = std::get<int>(args[1].as);
        
        return Obj(SysGui::measureTextHeight(text, size));
    };
    nativeRegistry["gui.get_mouse_wheel"] = [this](const std::vector<Obj>& args) -> Obj {
        return Obj((double)SysGui::getMouseWheel());
    };

    nativeRegistry["gui_load_image"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 2) return Obj(false);
        std::string path = objToString(args[0]);
        std::string name = objToString(args[1]);
        
        SysGui::loadImage(path, name);
        return Obj(true);
    };

    nativeRegistry["gui_draw_image"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 5) return Obj(false);
        std::string name = objToString(args[0]);
        int x = std::get<int>(args[1].as);
        int y = std::get<int>(args[2].as);
        int w = std::get<int>(args[3].as);
        int h = std::get<int>(args[4].as);
        
        SysGui::drawImage(name, x, y, w, h);
        return Obj(true);
    };
    nativeRegistry["gui.width"] = [](const std::vector<Obj>& args) -> Obj {
        return Obj(SysGui::getScreenWidth());
    };
    nativeRegistry["gui.height"] = [](const std::vector<Obj>& args) -> Obj {
        return Obj(SysGui::getScreenHeight());
    };
    nativeRegistry["gui_quit"] = [](const std::vector<Obj>& args) -> Obj {
    SysGui::stop();
    return Obj(true);
    };
    nativeRegistry["gui_get_time"] = [](const std::vector<Obj>& args) -> Obj {
        return Obj(SysGui::getTime());
    };
    nativeRegistry["gui_begin_clip"] = [asInt](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 4) return Obj(0);
        SysGui::beginScissor(asInt(args[0]), asInt(args[1]), asInt(args[2]), asInt(args[3]));
        return Obj(0);
    };
    nativeRegistry["gui_end_clip"] = [](const std::vector<Obj>& args) -> Obj {
        (void)args; SysGui::endScissor(); return Obj(0);
    };

    // ==========================================
    // 10. AUDIO MODULE 
    // ==========================================
    nativeRegistry["audio.init"] = [](const std::vector<Obj>& args) -> Obj {
        SysAudio::init();
        return Obj(0);
    };

    nativeRegistry["audio.get_eq"] = [](const std::vector<Obj>& args) -> Obj {
        if (!args.empty() && std::holds_alternative<int>(args[0].as)) {
            int band = std::get<int>(args[0].as);
            return Obj((double)SysAudio::getSpectrum(band));
        }
        return Obj(0.0);
    };

    nativeRegistry["audio.play"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) {
            SysAudio::resume(); 
            return Obj(true);
        }
        std::string path = objToString(args[0]);
        return Obj(SysAudio::play(path)); 
    };

    nativeRegistry["audio.pause"] = [](const std::vector<Obj>& args) -> Obj {
        SysAudio::pause();
        return Obj(0);
    };

    nativeRegistry["audio.stop"] = [](const std::vector<Obj>& args) -> Obj {
        SysAudio::stop();
        return Obj(0);
    };

    nativeRegistry["audio.update"] = [](const std::vector<Obj>& args) -> Obj {
        SysAudio::update();
        return Obj(0);
    };

    nativeRegistry["audio.volume"] = [](const std::vector<Obj>& args) -> Obj {
        if (!args.empty() && std::holds_alternative<double>(args[0].as)) {
            SysAudio::setVolume((float)std::get<double>(args[0].as));
        }
        return Obj(0);
    };

    nativeRegistry["audio.length"] = [](const std::vector<Obj>& args) -> Obj {
        return Obj((double)SysAudio::getTimeLength());
    };

    nativeRegistry["audio.played"] = [](const std::vector<Obj>& args) -> Obj {
        return Obj((double)SysAudio::getTimePlayed());
    };

    nativeRegistry["audio.seek"] = [](const std::vector<Obj>& args) -> Obj {
        if (!args.empty() && std::holds_alternative<double>(args[0].as)) {
            SysAudio::seek((float)std::get<double>(args[0].as));
        }
        return Obj(0);
    };

    nativeRegistry["audio.close"] = [](const std::vector<Obj>& args) -> Obj {
        SysAudio::close();
        return Obj(0);
    };
    nativeRegistry["audio.load_sound"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() >= 2) {
            SysAudio::loadSound(objToString(args[0]), objToString(args[1]));
        }
        return Obj(0);
    };

    nativeRegistry["audio.play_sound"] = [this](const std::vector<Obj>& args) -> Obj {
        if (!args.empty()) {
            SysAudio::playSound(objToString(args[0]));
        }
        return Obj(0);
    };
    nativeRegistry["audio.spectrum"] = [](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj(0.0);
        int band = 0;
        if (std::holds_alternative<int>(args[0].as)) band = std::get<int>(args[0].as);
        else if (std::holds_alternative<double>(args[0].as)) band = (int)std::get<double>(args[0].as);
        return Obj((double)SysAudio::getSpectrum(band)); 
    };
     // ==========================================
     // 11. XML MODULE
     // ==========================================
    nativeRegistry["xml_LoadFile"] = [this](const std::vector<Obj>& args) -> Obj {
        std::string path = objToString(args[0]);

        auto* doc = LinkXML::readFromFile(path);

        if (!doc)
            return Obj(0);
        return Obj(1);
    };
    nativeRegistry["xml_LoadString"] = [this](const std::vector<Obj>& args) -> Obj {
        std::string path = objToString(args[0]);

        auto* doc = LinkXML::readFromString(path);

        if (!doc)
            return Obj(0);
        return Obj(1);
    };
    nativeRegistry["xml_getRoot"] = [this](const std::vector<Obj>& args) -> Obj {
        return Obj(LinkXML::getRoot());
    };
    nativeRegistry["xml_destroyXML"] = [](const std::vector<Obj>& args) -> Obj {
        LinkXML::destroyXML();
        return Obj(0);
    };
    nativeRegistry["xml_exists"] = [this](const std::vector<Obj>& args) -> Obj {
        if(args.size() < 2){
            return Obj(0);
        }
        std::string path = objToString(args[0]);
        std::string value = objToString(args[1]);

        return Obj(LinkXML::exists(path,value));
    };
    nativeRegistry["xml_getText"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.empty())
            return Obj(0);

        std::string path = objToString(args[0]);

        if (path.empty())
            return Obj(0);

        tinyxml2::XMLElement* element = LinkXML::findPath(path);

        if (!element)
            return Obj(0);

        return Obj(LinkXML::getElementText(element));
    };

    // ==========================================
    // 12. MULTITHREADING / CONCURRENCY MODULE
    // ==========================================
    nativeRegistry["thread.spawn"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.empty()) return Obj(false);

        std::cout << "\033[1;33m[Chain]\033[0m You are Running a Multi-Thread Program" << std::endl;

        Obj callee = args[0];
 
        if (std::holds_alternative<LinkFunction*>(callee.as)) {
            auto funcObj = std::get<LinkFunction*>(callee.as);
  
            std::vector<Obj> threadArgs;
            for (size_t i = 1; i < args.size(); i++) {
                threadArgs.push_back(args[i]);
            }

            std::thread t([this, funcObj, threadArgs]() {
                Runtime threadRt;
                threadRt.globalEnv = this->globalEnv;
                threadRt.currentEnv = gc.allocate<Environment>(funcObj->closure);
                threadRt.nativeRegistry = this->nativeRegistry; 
                
                FuncDecl* fn = funcObj->declaration;
                
                for (size_t i = 0; i < fn->params.size(); ++i) {
                    if (i < threadArgs.size()) threadRt.currentEnv->define(fn->params[i], threadArgs[i]);
                }
                
                try {
                    for (auto& s : fn->body) threadRt.runStatement(s.get());
                } catch (const ReturnException&) {
                } catch (const BreakException&) {
                } catch (const ContinueException&) {
                } catch (const std::exception& e) {
                    std::cout << "\n\033[1;31m[Thread Error]\033[0m " << e.what() << "\n";
                } catch (...) {
                    std::cout << "\n\033[1;31m[Thread Error]\033[0m Unknown fatal error.\n";
                }
            });
            
            t.detach();
            return Obj(true);
        }
        
        std::cout << "Runtime Error: thread.spawn requires a function as first argument.\n";
        return Obj(false);
    };

    nativeRegistry["sync.lock"] = [](const std::vector<Obj>& args) -> Obj {
        (void)args;
        userSyncMutex.lock(); 
        return Obj(true);
    };

    nativeRegistry["sync.unlock"] = [](const std::vector<Obj>& args) -> Obj {
        (void)args;
        userSyncMutex.unlock(); 
        return Obj(true);
    };

    // ==========================================
    // 13. WEBVIEW MODULE (NATIVE DESKTOP WEBAPP)
    // ==========================================
#ifndef CHAIN_DISABLE_WEBVIEW
    nativeRegistry["webview.show"] = [this](const std::vector<Obj>& args) -> Obj {
        if (args.size() < 4) {
            std::cout << "Runtime Error: webview.show butuh 4 argumen (title, width, height, html)\n";
            return Obj(false);
        }
        
        // Default Display Resolution Value 
        std::string title = objToString(args[0]);
        int width = 640;
        int height = 480;
        
        if (std::holds_alternative<int>(args[1].as)) width = std::get<int>(args[1].as);
        else if (std::holds_alternative<double>(args[1].as)) width = (int)std::get<double>(args[1].as);
        
        if (std::holds_alternative<int>(args[2].as)) height = std::get<int>(args[2].as);
        else if (std::holds_alternative<double>(args[2].as)) height = (int)std::get<double>(args[2].as);
        
        std::string html = objToString(args[3]);

        SysWebview::create(title, width, height, html);
        
        return Obj(true);
    };
#else
    // Exception for Linux - i686
    nativeRegistry["webview.show"] = [](const std::vector<Obj>& args) -> Obj {
        (void)args;
        std::cout << "Runtime Error: Webview module is not supported on 32-bit (i686) systems.\n";
        return Obj(false);
    };
#endif
}

bool Runtime::isTruthy(const Obj& o) {
    if (std::holds_alternative<bool>(o.as)) return std::get<bool>(o.as);
    if (std::holds_alternative<int>(o.as)) return std::get<int>(o.as) != 0;
    if (std::holds_alternative<double>(o.as)) return std::get<double>(o.as) != 0.0;
    if (std::holds_alternative<std::string>(o.as)) return !std::get<std::string>(o.as).empty();
    return !std::holds_alternative<std::monostate>(o.as);
}

FuncDecl* Runtime::findMethod(LinkClass* klass, const std::string& name) {
    if (klass->methods.count(name)) {
        return dynamic_cast<FuncDecl*>(klass->methods[name]);
    }
    if (klass->superclass) {
        return findMethod(klass->superclass, name);
    }
    return nullptr;
}

void Runtime::printObj(const Obj& val) {
    std::cout << objToString(val);
}

Obj Runtime::evaluateExpr(Expr* expr) {
    if (!expr) return Obj();

    // =======================
    // 1. LITERALS & VARIABLES
    // =======================
    if (auto num = dynamic_cast<NumberExpr*>(expr)) return Obj(num->value);
    if (auto flt = dynamic_cast<FloatExpr*>(expr)) return Obj(flt->value);
    if (auto str = dynamic_cast<StringExpr*>(expr)) return Obj(str->value);
    if (auto chr = dynamic_cast<CharExpr*>(expr)) return Obj(chr->value);
    if (auto bl = dynamic_cast<BoolExpr*>(expr)) return Obj(bl->value);
    if (auto var = dynamic_cast<VariableExpr*>(expr)) return currentEnv->get(var->name);

    // ==================
    // 2. DATA STRUCTURES 
    // ==================
    if (auto arr = dynamic_cast<ArrayExpr*>(expr)) {
        auto list = gc.allocate<ChainList>();
        for (auto& el : arr->elements) list->elements.push_back(evaluateExpr(el.get()));
        return Obj(list);
    }
    if (auto dictNode = dynamic_cast<DictExpr*>(expr)) {
        auto dict = gc.allocate<ChainDict>();
        for (auto& p : dictNode->pairs) {
            Obj key = evaluateExpr(p.first.get());
            Obj val = evaluateExpr(p.second.get());
            if (std::holds_alternative<std::string>(key.as)) dict->map[std::get<std::string>(key.as)] = val;
            else std::cout << "Runtime Error: Dict key must be string.\n";
        }
        return Obj(dict);
    }

    // ================
    // 3. INDEX ACCESS 
    // ================
    if (auto idx = dynamic_cast<IndexExpr*>(expr)) {
        Obj object = evaluateExpr(idx->object.get());
        Obj index = evaluateExpr(idx->index.get());
        if (std::holds_alternative<ChainList*>(object.as) && std::holds_alternative<int>(index.as)) {
            auto list = std::get<ChainList*>(object.as);
            int i = std::get<int>(index.as);
            if (i < 0) i += list->elements.size(); 
            if (i >= 0 && i < (int)list->elements.size()) return list->elements[i];
        } else if (std::holds_alternative<ChainDict*>(object.as) && std::holds_alternative<std::string>(index.as)) {
            auto dict = std::get<ChainDict*>(object.as);
            std::string key = std::get<std::string>(index.as);
            if (dict->map.count(key)) return dict->map[key];
        }
        return Obj();
    }

    // ============
    // 4. OOP LOGIC 
    // ============
    if (auto newExpr = dynamic_cast<NewExpr*>(expr)) {
        Obj classObj = currentEnv->get(newExpr->className);
        if (!std::holds_alternative<LinkClass* >(classObj.as)) return Obj();

        auto klass = std::get<LinkClass* >(classObj.as);
        auto instance = gc.allocate<LinkInstance>();
        instance->klass = klass;

        FuncDecl* init = findMethod(klass, "init");
        if (init) {
            std::vector<Obj> args;
            for (auto& arg : newExpr->args) args.push_back(evaluateExpr(arg.get()));

            auto prevEnv = currentEnv;
            currentEnv = gc.allocate<Environment>(globalEnv);
            currentEnv->define("this", Obj(instance));
            for (size_t i = 0; i < init->params.size(); ++i) {
                 if (i < args.size()) currentEnv->define(init->params[i], args[i]);
            }
            try {
                for (auto& s : init->body) runStatement(s.get());
            } catch (const ReturnException&) {}
            currentEnv = prevEnv;
        }
        return Obj(instance);
    }
    
    if (dynamic_cast<ThisExpr*>(expr)) return currentEnv->get("this");
    
    if (auto lam = dynamic_cast<LambdaExpr*>(expr)) {
        auto linkFunc = gc.allocate<LinkFunction>();
        linkFunc->declaration = lam->anonFunc.get();
        linkFunc->closure = currentEnv; 
        return Obj(linkFunc);
    }

    if (auto get = dynamic_cast<GetExpr*>(expr)) {
        Obj obj = evaluateExpr(get->object.get());

        if (std::holds_alternative<LinkInstance*>(obj.as)) {
            auto instance = std::get<LinkInstance*>(obj.as);
            if (instance->fields.count(get->name)) return instance->fields[get->name];
        }

        if (std::holds_alternative<ChainDict*>(obj.as)) {
            auto dict = std::get<ChainDict*>(obj.as);
            if (dict->map.count(get->name)) return dict->map[get->name];
        }
        return Obj();
    }
    
    if (auto set = dynamic_cast<SetExpr*>(expr)) {
        Obj obj = evaluateExpr(set->object.get());
        // 1. Check if it is an OOP instance
        if (std::holds_alternative<LinkInstance*>(obj.as)) {
             auto instance = std::get<LinkInstance*>(obj.as);
             Obj val = evaluateExpr(set->value.get());
             instance->fields[set->name] = val;
             return val;
        }
        // 2. Check if it is a Dictionary (NEW FEATURE)
        if (std::holds_alternative<ChainDict*>(obj.as)) {
             auto dict = std::get<ChainDict*>(obj.as);
             Obj val = evaluateExpr(set->value.get());
             dict->map[set->name] = val;
             return val;
        }
        return Obj();
    }

    if (auto methodCall = dynamic_cast<MethodCallExpr*>(expr)) {
         Obj obj = evaluateExpr(methodCall->object.get());
         if (!std::holds_alternative<LinkInstance*>(obj.as)) return Obj();
         auto instance = std::get<LinkInstance*>(obj.as);
         FuncDecl* method = findMethod(instance->klass, methodCall->method);
         if (!method) return Obj();

         std::vector<Obj> args;
         for (auto& arg : methodCall->args) args.push_back(evaluateExpr(arg.get()));
         
         auto prevEnv = currentEnv;
         currentEnv = gc.allocate<Environment>(globalEnv);
         currentEnv->define("this", Obj(instance));
         for (size_t i = 0; i < method->params.size(); ++i) {
             if (i < args.size()) currentEnv->define(method->params[i], args[i]);
         }
         try { for (auto& s : method->body) runStatement(s.get()); } 
         catch (const ReturnException& e) { currentEnv = prevEnv; return e.value; }
         currentEnv = prevEnv;
         return Obj();
    }

    // =========================================================
    // 5. FUNCTION CALLS (THIS SECTION HAS COMPLETELY CHANGED!)
    // =========================================================
    if (auto call = dynamic_cast<CallExpr*>(expr)) {
        ProfilerTimer timer("Function '" + call->func + "'", enableProfiling);
        
        std::vector<Obj> args;
        for (auto& arg : call->args) {
            args.push_back(evaluateExpr(arg.get()));
        }
        
        // 1. Check Native Registry (print, os.exec, dll)
        if (nativeRegistry.find(call->func) != nativeRegistry.end()) {
            return nativeRegistry[call->func](args);
        }
        
        // 2. Execute from environment variable (user-defined function)
        Obj callee = currentEnv->get(call->func);
        if (std::holds_alternative<LinkFunction*>(callee.as)) {
            auto funcObj = std::get<LinkFunction*>(callee.as);
            FuncDecl* fn = funcObj->declaration;
            
            if (args.size() != fn->params.size()) {
                std::cout << "Runtime Error: Function " << fn->name << " arg mismatch.\n";
                return Obj();
            }

            auto previousEnv = currentEnv;
            currentEnv = gc.allocate<Environment>(funcObj->closure);
            
            for (size_t i = 0; i < fn->params.size(); ++i) {
                currentEnv->define(fn->params[i], args[i]);
            }

            try {
                for (auto& s : fn->body) runStatement(s.get());
            } catch (const ReturnException& e) {
                currentEnv = previousEnv; 
                return e.value; // Return the value
            }

            currentEnv = previousEnv;
            return Obj();
        }
        
        std::cout << "Runtime Error: Unknown function '" << call->func << "'\n";
        return Obj();
    }

    // =========================================================
    // 6. BINARY OPERATIONS (Same as before, copy old logic)
    // =========================================================
            if (auto bin = dynamic_cast<BinaryExpr*>(expr)) {
            Obj left = evaluateExpr(bin->lhs.get());   
            Obj right = evaluateExpr(bin->rhs.get());  
            
            if (std::holds_alternative<LinkInstance*>(left.as)) {
                auto instance = std::get<LinkInstance*>(left.as);
                std::string magicName = "";
                if (bin->op == '+') magicName = "__add__";
                else if (bin->op == '-') magicName = "__sub__";
                else if (bin->op == '*') magicName = "__mul__";
                else if (bin->op == '/') magicName = "__div__";
                else if (bin->op == '%') magicName = "__mod__";
                else if (bin->op == '=') magicName = "__eq__";

                if (!magicName.empty()) {
                    FuncDecl* method = findMethod(instance->klass, magicName);
                    if (method) {
                        auto prevEnv = currentEnv;
                        currentEnv = gc.allocate<Environment>(globalEnv);
                        currentEnv->define("this", left); 
                        if (method->params.size() > 0) currentEnv->define(method->params[0], right); 
                        
                        try { for (auto& s : method->body) runStatement(s.get()); } 
                        catch (const ReturnException& e) { currentEnv = prevEnv; return e.value; }
                        
                        currentEnv = prevEnv;
                        return Obj(); 
                    }
                }
            }

            if (std::holds_alternative<int>(left.as) && std::holds_alternative<int>(right.as)) {
                int l = std::get<int>(left.as);
                int r = std::get<int>(right.as);
                
                switch (bin->op) {
                    case '&': return Obj(l & r);
                    case '|': return Obj(l | r);
                    case '^': return Obj(l ^ r);
                    case 'L': return Obj(l << r); // Left Shift
                    case 'R': return Obj(l >> r); // Right Shift
                }
            }
            
            if (bin->op == '&' || bin->op == '|') {
            bool l = isTruthy(left);
            bool r = isTruthy(right);
            if (bin->op == '&') return Obj(l && r);
            if (bin->op == '|') return Obj(l || r);
        }
            
            if (std::holds_alternative<std::string>(left.as)) {
                std::string sLeft = std::get<std::string>(left.as);
                std::string sRight = objToString(right); 
                
                if (bin->op == '+') return Obj(sLeft + sRight);
            }

            if (std::holds_alternative<int>(left.as) && std::holds_alternative<int>(right.as)) {
                int l = std::get<int>(left.as), r = std::get<int>(right.as);
                switch (bin->op) {
                    case '+': return Obj(l + r); case '-': return Obj(l - r); case '!': return Obj(l != r); 
                    case '*': return Obj(l * r); case '/': return Obj((r != 0) ? l / r : 0);
                    case '%': return Obj((r != 0) ? l % r : 0); 
                    case '<': return Obj(l < r); case '>': return Obj(l > r); case '=': return Obj(l == r);
                    case '{': return Obj(l <= r); case '}': return Obj(l >= r); 
                }
            } else if ((std::holds_alternative<double>(left.as)||std::holds_alternative<int>(left.as)) && (std::holds_alternative<double>(right.as)||std::holds_alternative<int>(right.as))) {
                double l = std::holds_alternative<int>(left.as)?std::get<int>(left.as):std::get<double>(left.as);
                double r = std::holds_alternative<int>(right.as)?std::get<int>(right.as):std::get<double>(right.as);
                switch (bin->op) {
                    case '+': return Obj(l + r); case '-': return Obj(l - r); case '!': return Obj(l != r); 
                    case '*': return Obj(l * r); case '/': return Obj((r != 0.0) ? l / r : 0.0);
                    case '%': return Obj((r != 0.0) ? std::fmod(l, r) : 0.0);
                    case '<': return Obj(l < r); case '>': return Obj(l > r); case '=': return Obj(l == r);
                    case '{': return Obj(l <= r); case '}': return Obj(l >= r);
                }
            } else if (std::holds_alternative<std::string>(left.as) && std::holds_alternative<std::string>(right.as)) {
                if (bin->op == '=') return Obj(std::get<std::string>(left.as) == std::get<std::string>(right.as));
                if (bin->op == '!') return Obj(std::get<std::string>(left.as) != std::get<std::string>(right.as));
            } else if (std::holds_alternative<bool>(left.as) && std::holds_alternative<bool>(right.as)) {
                bool l = std::get<bool>(left.as);
                bool r = std::get<bool>(right.as);
                if (bin->op == '=') return Obj(l == r);
                if (bin->op == '!') return Obj(l != r);
            }
        }

    return Obj();
}

// --- 4. RUN STATEMENT ---
void Runtime::runStatement(Stmt* stmt) {
    if (!stmt) return;

    // 1. EXPRESSION & VARIABLE
    if (auto exprStmt = dynamic_cast<ExprStmt*>(stmt)) {
        evaluateExpr(exprStmt->expression.get());
        return;
    }
    if (auto set = dynamic_cast<SetStmt*>(stmt)) {
        currentEnv->assign(set->name, evaluateExpr(set->expression.get()));
        return;
    }
    
    // 2. ARRAY INDEX SET (list[0] = 1)
    if (auto setIdx = dynamic_cast<SetIndexStmt*>(stmt)) {
        Obj listObj = evaluateExpr(setIdx->list.get());
        Obj indexObj = evaluateExpr(setIdx->index.get());
        Obj val = evaluateExpr(setIdx->value.get());

        if (std::holds_alternative<ChainList*>(listObj.as) && 
            std::holds_alternative<int>(indexObj.as)) {
            auto list = std::get<ChainList*>(listObj.as);
            int idx = std::get<int>(indexObj.as);
            if (idx < 0) idx += list->elements.size();
            if (idx >= 0 && idx < (int)list->elements.size()) list->elements[idx] = val;
            else std::cout << "Runtime Error: Index out of bounds\n";
        }
        return;
    }

    // 3. CALL STATEMENT 
    if (auto call = dynamic_cast<CallStmt*>(stmt)) {
        ProfilerTimer timer("Call '" + call->func + "'", enableProfiling);
        
        std::vector<Obj> args;
        for (auto& arg : call->args) args.push_back(evaluateExpr(arg.get()));

        if (nativeRegistry.count(call->func)) {
            nativeRegistry[call->func](args);
            return;
        }

        Obj callee = currentEnv->get(call->func);
        if (std::holds_alternative<LinkFunction*>(callee.as)) {
            auto funcObj = std::get<LinkFunction*>(callee.as);
            FuncDecl* fn = funcObj->declaration;
            
            if (args.size() != fn->params.size()) {
                std::cout << "Runtime Error: Arg mismatch.\n"; return;
            }
            
            auto prevEnv = currentEnv;
            currentEnv = gc.allocate<Environment>(funcObj->closure);
            
            for (size_t i = 0; i < fn->params.size(); ++i) {
                currentEnv->define(fn->params[i], args[i]);
            }
            try { 
                for (auto& s : fn->body) runStatement(s.get()); 
            } catch (const ReturnException&) {}
            
            currentEnv = prevEnv;
            return;
        }
        std::cout << "Runtime Error: Unknown function '" << call->func << "'\n";
        return;
    }

    // 4. CONTROL FLOW (If, While, For, Try-Catch)
    if (auto ifStmt = dynamic_cast<IfStmt*>(stmt)) {
            if (isTruthy(evaluateExpr(ifStmt->condition.get()))) {
                for (auto& s : ifStmt->thenBranch) runStatement(s.get());
            } else {
                for (auto& s : ifStmt->elseBranch) runStatement(s.get());
            }
            return;
        }
    if (auto whileLoop = dynamic_cast<WhileStmt*>(stmt)) {
        ProfilerTimer timer("While Loop", enableProfiling);
        
        while (isTruthy(evaluateExpr(whileLoop->condition.get()))) {
                try {
                    for (auto& s : whileLoop->body) {
                        runStatement(s.get());
                        gc.checkGC();
                    }
                } 
                catch (const BreakException&) {
                    break; // Stop while loop C++
                }
                catch (const ContinueException&) {
                    continue; 
                }
            }
            return;
        }
    if (auto loop = dynamic_cast<ForStmt*>(stmt)) {
        ProfilerTimer timer("For Loop", enableProfiling);
        
        Obj collection = evaluateExpr(loop->collection.get());
        gc.pushTempRoot(&collection); 

        if (std::holds_alternative<ChainList*>(collection.as)) {
             auto list = std::get<ChainList*>(collection.as);
             currentEnv->define(loop->iteratorName, Obj(0)); 
             
             for (auto& item : list->elements) {
                currentEnv->assign(loop->iteratorName, item);
                try {
                    for (auto& s : loop->body) {
                      runStatement(s.get()); 
                      gc.checkGC(); 
                    }
                }
                 catch (const BreakException&) {
                     break; 
                 }
                 catch (const ContinueException&) {
                     continue; 
                 }
             }
        }
        gc.popTempRoot(); 
        return;
    }
    if (auto tryStmt = dynamic_cast<TryStmt*>(stmt)) {
            try {
                for (auto& s : tryStmt->tryBody) runStatement(s.get());
            } catch (const RuntimeException& e) {
                auto prevEnv = currentEnv;
                currentEnv = gc.allocate<Environment>(prevEnv);
                currentEnv->define(tryStmt->errorVar, Obj(e.message));
                for (auto& s : tryStmt->catchBody) runStatement(s.get());
                currentEnv = prevEnv;
            }
            return;
        }
    if (auto ret = dynamic_cast<ReturnStmt*>(stmt)) {
			Obj result; 
			if (ret->value) result = evaluateExpr(ret->value.get()); 
			throw ReturnException(result); 
		}
	if (dynamic_cast<BreakStmt*>(stmt)) {
            throw BreakException(); 
        }
    if (dynamic_cast<ContinueStmt*>(stmt)) {
            throw ContinueException(); 
        }

    // 5. DEFINITIONS
    if (auto func = dynamic_cast<FuncDecl*>(stmt)) {
        auto linkFunc = gc.allocate<LinkFunction>();
        linkFunc->declaration = func;
        linkFunc->closure = currentEnv; 
        currentEnv->define(func->name, Obj(linkFunc)); 
        return;
    }

    if (auto cls = dynamic_cast<ClassDecl*>(stmt)) {
        auto klass = gc.allocate<LinkClass>();
        klass->name = cls->name;
 
        if (!cls->superclass.empty()) {
            Obj superObj = currentEnv->get(cls->superclass);
            if (std::holds_alternative<LinkClass* >(superObj.as)) {
                klass->superclass = std::get<LinkClass* >(superObj.as);
            } else {
                std::cout << "Runtime Error: Superclass '" << cls->superclass << "' not found.\n";
            }
        }

        for (auto& method : cls->methods) klass->methods[method->name] = method.get();
        currentEnv->define(cls->name, Obj(klass));
        return;
    }

    if (auto matchStmt = dynamic_cast<MatchStmt*>(stmt)) {
        Obj matchVal = evaluateExpr(matchStmt->value.get());
        gc.pushTempRoot(&matchVal); 

        for (auto& c : matchStmt->cases) {
            if (c.pattern == nullptr) { 
                for (auto& s : c.body) runStatement(s.get());
                break;
            }
            
            Obj patVal = evaluateExpr(c.pattern.get());
            if (isEqualObj(matchVal, patVal)) {
                for (auto& s : c.body) runStatement(s.get());
                break; 
            }
        }
        gc.popTempRoot();
        return;
    }

    if (dynamic_cast<ClearStmt*>(stmt)) {
        #ifdef _WIN32 
        system("cls"); 
        #else 
        system("clear"); 
        #endif
        return; 
    }
    if (auto prop = dynamic_cast<PropertyStmt*>(stmt)) { 
        if (prop->name == "sh") { int s = system(prop->value.c_str()); (void)s; }
        return;
    }
    if (auto imp = dynamic_cast<ImportStmt*>(stmt)) {
         std::string path = imp->path;
         if (!Sys::fileExists(path)) {
             std::cout << "Runtime Error: Cannot import '" << path << "'. File not found.\n";
             return;
         }

         std::error_code ec;
         std::string absPath = fs::absolute(path, ec).string();
         if (ec) absPath = path; 
         if (importedFiles.find(absPath) != importedFiles.end()) {
             return; 
         }

         importedFiles.insert(absPath);

         std::string source = Sys::readFile(path);
         Lexer lexer(source);
         auto tokens = lexer.tokenize();
         Parser parser(tokens);
         auto importedProgram = parser.parse();
         
         if (importedProgram) {
             loadedPrograms.push_back(std::move(importedProgram));
             Program* storedProgram = loadedPrograms.back().get();
             for (auto& s : storedProgram->statements) {
                 runStatement(s.get());
             }
         }
         return;
    }
    if (auto ext = dynamic_cast<ExternStmt*>(stmt)) {
        #ifdef _WIN32
        std::cout << "Runtime Error: Extern blocks require POSIX environments.\n";
        return;
        #else
        
        // --- 1. DETECT ALL ACTIVE VARIABLES IN CHAIN ---
        std::vector<std::string> int_vars;
        std::vector<std::string> double_vars;
        
        // Collect from current scope up to global
        Environment* env_ptr = currentEnv;
        std::unordered_map<std::string, Obj> all_vars;
        while (env_ptr) {
            for (const auto& [name, val] : env_ptr->values) {
                if (all_vars.find(name) == all_vars.end()) all_vars[name] = val;
            }
            env_ptr = env_ptr->enclosing;
        }

        // Separate into Int and Double
        for (const auto& [name, val] : all_vars) {
            if (std::holds_alternative<int>(val.as)) int_vars.push_back(name);
            else if (std::holds_alternative<double>(val.as)) double_vars.push_back(name);
        }

        // --- 2. GENERATE SIGNATURE FOR CACHING ---
        std::string var_signature = "";
        for (auto& n : int_vars) var_signature += n + "i,";
        for (auto& n : double_vars) var_signature += n + "d,";
        
        std::string hashName = "mod_ext_" + std::to_string(std::hash<std::string>{}(ext->code + var_signature));
        std::string cacheDir = ".link_cache/";
        Sys::exec(("mkdir -p " + cacheDir).c_str()); 
        
        std::string cppPath = cacheDir + hashName + ".cpp";
        std::string soPath = cacheDir + hashName + ".so";

        // --- 3. WRITE C++ FILE IF NOT CACHED ---
        if (!Sys::fileExists(soPath)) {
            std::string includes = "", body = "";
            std::istringstream stream(ext->code);
            std::string line;
            while (std::getline(stream, line)) {
                size_t start = line.find_first_not_of(" \t");
                if (start != std::string::npos && line[start] == '#') includes += line + "\n";
                else body += line + "\n"; 
            }

            std::ofstream out(cppPath);
            out << "#include <iostream>\n#include <string>\n";
            out << "#define print(x) std::cout << (x) << std::endl;\n";
        
            // Int memory at offset 0 - 2047
            for (size_t i = 0; i < int_vars.size(); i++) {
                out << "#define LINK_" << int_vars[i] << " (((int*)link_shm)[" << i << "])\n";
            }
            
            for (size_t i = 0; i < double_vars.size(); i++) {
                out << "#define LINK_" << double_vars[i] << " (((double*)((char*)link_shm + 2048))[" << i << "])\n";
            }

            out << includes << "\nextern \"C\" void link_entry(void* link_shm) {\n" << body << "\n}\n";
            out.close();

            std::string cmd = "g++ -shared -fPIC -o " + soPath + " " + cppPath + " " + ext->flags;
            
            if (system(cmd.c_str()) != 0) {
                std::cout << "Runtime Error: Gagal mengkompilasi C++ native.\n"; return;
            }
        }

        // --- 4. SETUP POSIX SHARED MEMORY ---
        const char* shm_name = "/link_lang_shm";
        int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
        ftruncate(shm_fd, 4096); 
        void* shared_mem = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

        // --- 5. PUSH: CHAIN -> SHARED MEMORY ---
        int* shm_ints = (int*)shared_mem;
        double* shm_doubles = (double*)((char*)shared_mem + 2048);
        
        for (size_t i = 0; i < int_vars.size(); i++) 
            shm_ints[i] = std::get<int>(all_vars[int_vars[i]].as);
            
        for (size_t i = 0; i < double_vars.size(); i++) 
            shm_doubles[i] = std::get<double>(all_vars[double_vars[i]].as);

        // --- 6. EXECUTE C++ (FORK) ---
        pid_t pid = fork();
        if (pid == 0) {
            void* handle = dlopen(soPath.c_str(), RTLD_NOW);
            if (handle) {
                typedef void (*EntryFunc)(void*);
                auto func = (EntryFunc)dlsym(handle, "link_entry");
                if (func) func(shared_mem); 
                dlclose(handle);
            }
            exit(0); 
        } else if (pid > 0) {
            int status; waitpid(pid, &status, 0); 
            
            // --- 7. PULL: SHARED MEMORY -> CHAIN ---
            for (size_t i = 0; i < int_vars.size(); i++) 
                currentEnv->assign(int_vars[i], Obj(shm_ints[i]));
                
            for (size_t i = 0; i < double_vars.size(); i++) 
                currentEnv->assign(double_vars[i], Obj(shm_doubles[i]));

            munmap(shared_mem, 4096); shm_unlink(shm_name);
        }
        #endif
        return;
    }
}

void Runtime::execute(std::unique_ptr<Program> program) {
    if (!program) return;
    for (auto& stmt : program->statements) {
        runStatement(stmt.get());
        gc.checkGC();
    }
}
