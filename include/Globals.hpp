#pragma once
#include <raylib.h>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <fstream>

namespace Config {
    const int WIN_WIDTH_DEFAULT = 1280;
    const int WIN_HEIGHT_DEFAULT = 800;
    const int FPS_LIMIT = 60;
    
    // UI Dimensions
    const int NAVBAR_HEIGHT = 30;
    const int TAB_HEIGHT = 30;
    const int FOOTER_HEIGHT = 25;
    const int MENU_WIDTH = 260;
    
    // Font Sizes
    const int FONT_SIZE_UI = 20;
    const int FONT_SIZE_SMALL = 18;
    const int FONT_SIZE_EDITOR_DEFAULT = 24;
    
    // Icons
    const int ICON_SIZE_SMALL = 20;
    const int ICON_SIZE_LARGE = 64;
}

struct Theme {
    Color bg;           
    Color panelBg;      
    Color border;       
    Color text;         
    Color keyword;      
    Color type;         
    Color string;       
    Color comment;      
    Color number;       
    Color cursor;       
    Color selection;    
    Color folder;
    
    Color tabActive;    
    Color tabInactive;  

    Color menuHover;    
    Color menuText;     
    Color runButton;    
    Color closeBtn;     
    Color fileHover;    
    Color btnNormal;    
};

enum class LayoutMode { Standard, Widescreen, Focus };

struct AppSettings {
    int fontSize = Config::FONT_SIZE_EDITOR_DEFAULT;
    int sidebarWidth = 250;
    int terminalHeight = 200;
    int terminalWidth = 400;
    
    std::string fontPath = "assets/JetBrainsMono-Regular.ttf";
    std::string shellPath = "/bin/zsh";
    std::string cFlags = "g++ \"$FILE\" -o temp_run && temp_run";
    bool imagePreview = true;
    bool audioPreview = true;
    
    LayoutMode layout = LayoutMode::Standard;
    int themeIndex = 0; 
};

struct Toast {
    std::string message;
    float lifeTime;
    float maxTime;
};

extern Theme theme;
extern AppSettings settings;
extern std::vector<Toast> toastQueue;

// Helpers
void ShowToast(const std::string& msg);
void ApplyThemePreset(int index);
void LoadSettings();
void SaveSettings();
std::string CodepointToUTF8(int cp);

inline int Clamp(int v, int a, int b) {
    if (v < a) return a;
    if (v > b) return b;
    return v;
}
