#pragma once
#include <string>

#if defined(_WIN32) || defined(_WIN64)
    #ifdef BUILDING_CHAIN_WEBVIEW_DLL
        #define CHAIN_WEBVIEW_API __declspec(dllexport)
    #else
        #define CHAIN_WEBVIEW_API
    #endif
#else
    #define CHAIN_WEBVIEW_API
#endif

namespace SysWebview {
    CHAIN_WEBVIEW_API void create(const std::string& title, int width, int height, const std::string& html);
}