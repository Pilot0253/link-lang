#ifndef CHAIN_DISABLE_WEBVIEW

#define WEBVIEW_EDGE 1
#include "link_webview.h"
#include "webview/webview.h"
#include <iostream>
#include <fstream>
#include <filesystem> 

std::string urlEncode(const std::string& value) {
    std::string escaped;
    for (char c : value) {
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped += c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
            escaped += buf;
        }
    }
    return escaped;
}

namespace SysWebview {
    
    void create(const std::string& title, int width, int height, const std::string& html) {
        try {
            std::string tempFilePath = std::filesystem::current_path().string() + "/.chain_temp_ui.html";
            
            std::ofstream out(tempFilePath);
            if (out.is_open()) {
                out << html;
                out.close();
            }
            
            webview::webview w(false, nullptr);
            
            w.set_title(title);
            w.set_size(width, height, WEBVIEW_HINT_NONE);
            w.navigate("file://" + tempFilePath);
            
            w.bind("chain_print", [](std::string s) -> std::string {
                if (s.length() >= 4) s = s.substr(2, s.length() - 4);
                std::cout << "\033[1;36m[Webview UI]\033[0m " << s << std::endl;
                return ""; 
            });
 
            w.run(); 
            
            std::filesystem::remove(tempFilePath);

        } catch (const std::exception& e) {
            std::cout << "[Webview Fatal Error] " << e.what() << std::endl;
        }
    }
}

#endif
