#include "../include/Globals.hpp"
#include "../include/Editor.hpp"
#include "../include/FileManager.hpp"
#include "../include/Terminal.hpp"
#include <cstdlib> 
#include <unistd.h>
#include <string>

static void SetupAppWorkingDir() {
    // When launched from Finder, cwd isn't the project folder. Try app Resources.
    std::string appDir = GetApplicationDirectory();
    if (!appDir.empty()) {
        std::string resourcesDir = appDir + "../Resources/";
        if (DirectoryExists(resourcesDir.c_str())) {
            chdir(resourcesDir.c_str());
        }
    }
}

struct AppState {
    bool showMenuFile = false; 
    bool showMenuHelp = false; 
    bool showSettings = false; 
    bool showAbout = false;
    bool runMakefile = false;
    bool resizeSide = false; 
    bool resizeTerm = false;
    int focus = 0; 
    int editingField = 0; 
    int inputCursor = 0;
};

// --- UI HELPERS ---

bool DrawMenuBtn(Rectangle r, const char* text, Font font, Color bgColor) {
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, hover ? theme.menuHover : bgColor);
    DrawRectangleLinesEx(r, 1, theme.border);
    
    Vector2 textSize = MeasureTextEx(font, text, (float)Config::FONT_SIZE_SMALL, 1);
    float textX = r.x + (r.width - textSize.x) / 2;
    float textY = r.y + (r.height - textSize.y) / 2;
    DrawTextEx(font, text, {textX, textY}, (float)Config::FONT_SIZE_SMALL, 1, theme.menuText);
    
    return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

bool DrawMenuItem(float x, float y, float w, const char* text, Font font) { 
    return DrawMenuBtn({x, y, w, 30}, text, font, theme.panelBg); 
}

void DrawColorSlider(Rectangle bounds, const char* label, unsigned char* value, Font font) {
    DrawTextEx(font, label, {bounds.x, bounds.y}, (float)Config::FONT_SIZE_SMALL, 1, GRAY);
    Rectangle bar = {bounds.x + 60, bounds.y + 5, bounds.width - 70, 15};
    DrawRectangleRec(bar, GRAY);
    
    float pct = (float)(*value) / 255.0f;
    DrawRectangle((int)(bar.x + (bar.width * pct) - 5), (int)(bar.y - 2), 10, 19, WHITE);
    
    if (CheckCollisionPointRec(GetMousePosition(), {bar.x, bar.y-5, bar.width, 25}) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float rel = GetMousePosition().x - bar.x; 
        rel = Clamp(rel, 0, bar.width);
        *value = (unsigned char)((rel / bar.width) * 255);
    }
}

void HandleTextInput(std::string& target, int& cursor) {
    if (cursor > (int)target.size()) cursor = target.size();
    int c = GetCharPressed();
    while (c > 0) { 
        if (c >= 32 && c <= 126) { target.insert(cursor, 1, (char)c); cursor++; } 
        c = GetCharPressed(); 
    }
    if (IsKeyPressed(KEY_BACKSPACE) && cursor > 0 && !target.empty()) { target.erase(cursor - 1, 1); cursor--; }
    if (IsKeyPressed(KEY_LEFT) && cursor > 0) cursor--;
    if (IsKeyPressed(KEY_RIGHT) && cursor < (int)target.size()) cursor++;
}

// --- MODALS ---

void DrawSettings(Rectangle bounds, Font font, Editor& editor, Terminal& terminal, AppState& app) {
    DrawRectangleRec(bounds, theme.panelBg); 
    DrawRectangleLinesEx(bounds, 2, theme.border);
    DrawTextEx(font, "SETTINGS", {bounds.x+10, bounds.y+10}, 24, 1, theme.menuText);
    
    static int tab = 0;
    if(DrawMenuBtn({bounds.x+10, bounds.y+40, 100, 30}, "General", font, tab==0?theme.btnNormal:theme.panelBg)) tab = 0;
    if(DrawMenuBtn({bounds.x+115, bounds.y+40, 100, 30}, "Theme", font, tab==1?theme.btnNormal:theme.panelBg)) tab = 1;

    float y = bounds.y + 80;
    if (tab == 0) { 
        // Font Path
        DrawTextEx(font, "Font Path:", {bounds.x+20, y}, (float)Config::FONT_SIZE_UI, 1, GRAY);
        Rectangle fontBox = {bounds.x+20, y+25, bounds.width-120, 30};
        DrawRectangleRec(fontBox, app.editingField == 1 ? theme.bg : theme.border);
        DrawRectangleLinesEx(fontBox, 1, theme.border);
        DrawTextEx(font, settings.fontPath.c_str(), {fontBox.x+5, fontBox.y+5}, (float)Config::FONT_SIZE_SMALL, 1, theme.text);
        
        if(CheckCollisionPointRec(GetMousePosition(), fontBox) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { 
            app.editingField=1; app.inputCursor=settings.fontPath.size(); 
        }
        if(DrawMenuBtn({fontBox.x+fontBox.width+10, y+25, 70, 30}, "Browse", font, theme.btnNormal)) {
            std::string f = OpenWindowsFilePicker("assets/");
            if(!f.empty()) settings.fontPath = f;
        }
        if (app.editingField == 1 && (int)(GetTime()*2)%2==0) {
            float cw = MeasureTextEx(font, settings.fontPath.substr(0, app.inputCursor).c_str(), (float)Config::FONT_SIZE_SMALL, 1).x;
            DrawRectangle((int)(fontBox.x+5+cw), (int)(fontBox.y+5), 2, 20, theme.cursor);
        }

        y += 70;
        // Build Command
        DrawTextEx(font, "Build Command ($FILE):", {bounds.x+20, y}, (float)Config::FONT_SIZE_UI, 1, GRAY);
        Rectangle cflagBox = {bounds.x+20, y+25, bounds.width-40, 30};
        DrawRectangleRec(cflagBox, app.editingField == 2 ? theme.bg : theme.border);
        DrawRectangleLinesEx(cflagBox, 1, theme.border);
        DrawTextEx(font, settings.cFlags.c_str(), {cflagBox.x+5, cflagBox.y+5}, (float)Config::FONT_SIZE_SMALL, 1, theme.text);
        
        if(CheckCollisionPointRec(GetMousePosition(), cflagBox) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { 
            app.editingField=2; app.inputCursor=settings.cFlags.size(); 
        }
        if (app.editingField == 2 && (int)(GetTime()*2)%2==0) {
            float cw = MeasureTextEx(font, settings.cFlags.substr(0, app.inputCursor).c_str(), (float)Config::FONT_SIZE_SMALL, 1).x;
            DrawRectangle((int)(cflagBox.x+5+cw), (int)(cflagBox.y+5), 2, 20, theme.cursor);
        }

        y += 70;
        // Image Preview
        DrawTextEx(font, "Image Preview:", {bounds.x+20, y}, (float)Config::FONT_SIZE_UI, 1, GRAY);
        if (DrawMenuBtn({bounds.x+160, y-5, 60, 26}, settings.imagePreview ? "On" : "Off", font, theme.btnNormal)) {
            settings.imagePreview = !settings.imagePreview;
        }

        y += 40;
        // Audio Preview
        DrawTextEx(font, "Audio Preview:", {bounds.x+20, y}, (float)Config::FONT_SIZE_UI, 1, GRAY);
        if (DrawMenuBtn({bounds.x+160, y-5, 60, 26}, settings.audioPreview ? "On" : "Off", font, theme.btnNormal)) {
            settings.audioPreview = !settings.audioPreview;
        }

        y += 40;
        // Shell Path
        DrawTextEx(font, "Shell Path:", {bounds.x+20, y}, (float)Config::FONT_SIZE_UI, 1, GRAY);
        Rectangle shellBox = {bounds.x+20, y+25, bounds.width-40, 30};
        DrawRectangleRec(shellBox, app.editingField == 3 ? theme.bg : theme.border);
        DrawRectangleLinesEx(shellBox, 1, theme.border);
        DrawTextEx(font, settings.shellPath.c_str(), {shellBox.x+5, shellBox.y+5}, (float)Config::FONT_SIZE_SMALL, 1, theme.text);
        if (CheckCollisionPointRec(GetMousePosition(), shellBox) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            app.editingField=3; app.inputCursor=settings.shellPath.size();
        }
        if (app.editingField == 3 && (int)(GetTime()*2)%2==0) {
            float cw = MeasureTextEx(font, settings.shellPath.substr(0, app.inputCursor).c_str(), (float)Config::FONT_SIZE_SMALL, 1).x;
            DrawRectangle((int)(shellBox.x+5+cw), (int)(shellBox.y+5), 2, 20, theme.cursor);
        }
        // Quick picks
        if (DrawMenuBtn({bounds.x+20, y+60, 70, 26}, "zsh", font, theme.btnNormal)) settings.shellPath="/bin/zsh";
        if (DrawMenuBtn({bounds.x+95, y+60, 70, 26}, "bash", font, theme.btnNormal)) settings.shellPath="/bin/bash";
        if (DrawMenuBtn({bounds.x+170, y+60, 70, 26}, "sh", font, theme.btnNormal)) settings.shellPath="/bin/sh";

        y += 95;
        // Layout
        DrawTextEx(font, "Layout:", {bounds.x+20, y+5}, (float)Config::FONT_SIZE_UI, 1, GRAY);
        if(DrawMenuBtn({bounds.x+100, y, 80, 30}, "Standard", font, settings.layout==LayoutMode::Standard?theme.btnNormal:theme.panelBg)) settings.layout=LayoutMode::Standard;
        if(DrawMenuBtn({bounds.x+185, y, 100, 30}, "Wide", font, settings.layout==LayoutMode::Widescreen?theme.btnNormal:theme.panelBg)) settings.layout=LayoutMode::Widescreen;
        if(DrawMenuBtn({bounds.x+290, y, 60, 30}, "Focus", font, settings.layout==LayoutMode::Focus?theme.btnNormal:theme.panelBg)) settings.layout=LayoutMode::Focus;

        y += 50;
        // Actions
        if(DrawMenuBtn({bounds.x+20, y, 140, 30}, "Apply Settings", font, theme.btnNormal)) {
            Font newFont = LoadFontEx(settings.fontPath.c_str(), 96, 0, 250);
            SetTextureFilter(newFont.texture, TEXTURE_FILTER_BILINEAR);
            editor.reloadFont(newFont);
            terminal.close();
            terminal.init();
            SaveSettings();
            ShowToast("Settings Saved");
        }
        if(DrawMenuBtn({bounds.x+170, y, 140, 30}, "Reset Settings", font, theme.closeBtn)) {
            ApplyThemePreset(0); settings.fontPath = "assets/JetBrainsMono-Regular.ttf"; ShowToast("Reset Done");
        }
    } else { 
        // Themes
        DrawTextEx(font, "Preset:", {bounds.x+20, y+5}, (float)Config::FONT_SIZE_UI, 1, GRAY);
        if(DrawMenuBtn({bounds.x+100, y, 80, 30}, "Default", font, settings.themeIndex==0?theme.btnNormal:theme.panelBg)) ApplyThemePreset(0);
        if(DrawMenuBtn({bounds.x+185, y, 80, 30}, "Obsidian", font, settings.themeIndex==1?theme.btnNormal:theme.panelBg)) ApplyThemePreset(1);
        if(DrawMenuBtn({bounds.x+270, y, 60, 30}, "Light", font, settings.themeIndex==2?theme.btnNormal:theme.panelBg)) ApplyThemePreset(2);
        if(DrawMenuBtn({bounds.x+335, y, 70, 30}, "Custom", font, settings.themeIndex==3?theme.btnNormal:theme.panelBg)) settings.themeIndex=3;

        y += 40;
        if (settings.themeIndex == 3) {
            static int colorTarget = 0; 
            DrawTextEx(font, "Edit:", {bounds.x+20, y+5}, (float)Config::FONT_SIZE_UI, 1, GRAY);
            if(DrawMenuBtn({bounds.x+80, y, 40, 30}, "BG", font, colorTarget==0?theme.btnNormal:theme.panelBg)) colorTarget=0;
            if(DrawMenuBtn({bounds.x+125, y, 60, 30}, "Term", font, colorTarget==1?theme.btnNormal:theme.panelBg)) colorTarget=1;
            if(DrawMenuBtn({bounds.x+190, y, 50, 30}, "Text", font, colorTarget==2?theme.btnNormal:theme.panelBg)) colorTarget=2;
            if(DrawMenuBtn({bounds.x+245, y, 80, 30}, "Menu", font, colorTarget==3?theme.btnNormal:theme.panelBg)) colorTarget=3;

            y += 40;
            Color* c = (colorTarget==0) ? &theme.bg : (colorTarget==1 ? &theme.panelBg : (colorTarget==2 ? &theme.text : &theme.menuText));
            DrawColorSlider({bounds.x+20, y, 200, 20}, "R", &c->r, font); y+=30;
            DrawColorSlider({bounds.x+20, y, 200, 20}, "G", &c->g, font); y+=30;
            DrawColorSlider({bounds.x+20, y, 200, 20}, "B", &c->b, font);
            
            DrawRectangle((int)(bounds.x+250), (int)(y-60), 50, 80, *c); 
            DrawRectangleLines((int)(bounds.x+250), (int)(y-60), 50, 80, theme.text);
        }
    }

    if (DrawMenuBtn({bounds.x+bounds.width-40, bounds.y, 40, 30}, "X", font, theme.closeBtn)) { app.showSettings = false; app.editingField=0; }
    
    if (app.editingField == 1) HandleTextInput(settings.fontPath, app.inputCursor);
    if (app.editingField == 2) HandleTextInput(settings.cFlags, app.inputCursor);
    if (app.editingField == 3) HandleTextInput(settings.shellPath, app.inputCursor);
}

void DrawAbout(Rectangle bounds, Font font, AppState& app, Texture2D icon) {
    DrawRectangleRec(bounds, theme.panelBg); 
    DrawRectangleLinesEx(bounds, 2, theme.border);
    DrawTextEx(font, "ABOUT", {bounds.x + 10, bounds.y + 10}, 20, 1, theme.keyword);
    
    if (DrawMenuBtn({bounds.x + bounds.width - 40, bounds.y, 40, 30}, "X", font, theme.closeBtn)) app.showAbout = false;

    float contentY = bounds.y + 50;

    if (icon.id > 0) {
        float targetSize = (float)Config::ICON_SIZE_LARGE;
        float scale = targetSize / (float)icon.width;
        float iconX = bounds.x + (bounds.width - targetSize) / 2;
        DrawTextureEx(icon, {iconX, contentY}, 0.0f, scale, WHITE);
        contentY += targetSize + 15;
    }

    const char* title = "ctom IDE";
    Vector2 titleSize = MeasureTextEx(font, title, 30, 1);
    DrawTextEx(font, title, {bounds.x + (bounds.width - titleSize.x)/2, contentY}, 30, 1, theme.text);
    
    contentY += 40;
    const char* author = "Created by prodkt54";
    Vector2 authSize = MeasureTextEx(font, author, 20, 1);
    DrawTextEx(font, author, {bounds.x + (bounds.width - authSize.x)/2, contentY}, 20, 1, theme.comment);

    contentY += 25;
    const char* ver = "Version 0.3.1";
    Vector2 verSize = MeasureTextEx(font, ver, 18, 1);
    DrawTextEx(font, ver, {bounds.x + (bounds.width - verSize.x)/2, contentY}, 18, 1, GRAY);
}

// --- MISSING FUNCTION ADDED HERE ---
void OpenModal(AppState& app, int type) {
    app.showMenuFile = false; 
    app.showMenuHelp = false;
    app.showSettings = (type == 1); 
    app.showAbout = (type == 2);
    app.editingField = 0;
}
// -----------------------------------

void DrawToasts(Font font, int w, int h) {
    float y = h - 60;
    for (auto it = toastQueue.begin(); it != toastQueue.end();) {
        it->lifeTime -= GetFrameTime();
        if (it->lifeTime <= 0) it = toastQueue.erase(it);
        else {
            float alpha = 1.0f; if (it->lifeTime < 0.5f) alpha = it->lifeTime / 0.5f;
            Color bg = theme.runButton; bg.a = (unsigned char)(200 * alpha);
            Color txt = WHITE; txt.a = (unsigned char)(255 * alpha);
            float tw = MeasureTextEx(font, it->message.c_str(), 20, 1).x + 20;
            DrawRectangleRec({(float)w - tw - 20, y, tw, 40}, bg);
            DrawTextEx(font, it->message.c_str(), {(float)w - tw - 10, y + 10}, 20, 1, txt);
            y -= 50; ++it;
        }
    }
}

// --- MAIN LOOP ---

int main() {
    SetupAppWorkingDir();
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(Config::WIN_WIDTH_DEFAULT, Config::WIN_HEIGHT_DEFAULT, "ctom"); 
    InitAudioDevice();
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    
    // Load Icons
    Image appIconImg = LoadImage("assets/icon.png");
    Texture2D logoTexture = { 0 }; 
    if (appIconImg.data != NULL) {
        SetWindowIcon(appIconImg);
        logoTexture = LoadTextureFromImage(appIconImg);
        SetTextureFilter(logoTexture, TEXTURE_FILTER_BILINEAR);
        UnloadImage(appIconImg);
    }

    LoadSettings();
    Font mainFont = LoadFontEx(settings.fontPath.c_str(), 96, 0, 250);
    SetTextureFilter(mainFont.texture, TEXTURE_FILTER_BILINEAR);
    ApplyThemePreset(settings.themeIndex);
    Editor editor; editor.init(mainFont); 
    FileManager fileMgr; fileMgr.init(); 
    Terminal terminal; terminal.init(); 
    AppState app;

    while (!WindowShouldClose()) {
        float w = (float)GetScreenWidth(); 
        float h = (float)GetScreenHeight(); 
        Vector2 m = GetMousePosition();
        bool isModalOpen = app.showSettings || app.showAbout || app.showMenuFile || app.showMenuHelp;

        if (IsKeyPressed(KEY_ESCAPE)) {
            if (app.showMenuFile) app.showMenuFile = false;
            else if (app.showMenuHelp) app.showMenuHelp = false;
            else if (app.showSettings) app.showSettings = false;
            else if (app.showAbout) app.showAbout = false;
        }

        if (!isModalOpen) {
            Rectangle rResSide, rResTerm;
            int headerH = Config::NAVBAR_HEIGHT;
            
            if (settings.layout == LayoutMode::Standard) { 
                rResSide = {(float)settings.sidebarWidth-4, (float)headerH, 8, h-headerH}; 
                rResTerm = {(float)settings.sidebarWidth, h-settings.terminalHeight-4, w-settings.sidebarWidth, 8}; 
            } 
            else if (settings.layout == LayoutMode::Widescreen) { 
                rResSide = {(float)settings.sidebarWidth-4, (float)headerH, 8, h-headerH}; 
                rResTerm = {w-settings.terminalWidth-4, (float)headerH, 8, h-headerH}; 
            }
            
            bool hSide = CheckCollisionPointRec(m, rResSide); 
            bool hTerm = CheckCollisionPointRec(m, rResTerm);
            
            if (settings.layout != LayoutMode::Focus) {
                if (hSide) SetMouseCursor(MOUSE_CURSOR_RESIZE_EW); 
                else if (hTerm) SetMouseCursor(settings.layout==LayoutMode::Standard ? MOUSE_CURSOR_RESIZE_NS : MOUSE_CURSOR_RESIZE_EW); 
                else SetMouseCursor(MOUSE_CURSOR_DEFAULT);
                
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { if (hSide) app.resizeSide = true; if (hTerm) app.resizeTerm = true; }
            }
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) { app.resizeSide=false; app.resizeTerm=false; }
            
            if (app.resizeSide) { 
                settings.sidebarWidth=(int)m.x; 
                settings.sidebarWidth=Clamp(settings.sidebarWidth,150,(int)w-100); 
            }
            if (app.resizeTerm) {
                if(settings.layout==LayoutMode::Standard) { 
                    settings.terminalHeight=(int)(h-m.y); 
                    settings.terminalHeight=Clamp(settings.terminalHeight,50,(int)h-100); 
                } else { 
                    settings.terminalWidth=(int)(w-m.x); 
                    settings.terminalWidth=Clamp(settings.terminalWidth,200,(int)w-settings.sidebarWidth); 
                }
            }
        }

        Rectangle rFiles, rTerm, rEdit;
        float topY = (float)Config::NAVBAR_HEIGHT;
        float tabH = (float)Config::TAB_HEIGHT;
        
        if (settings.layout == LayoutMode::Focus) { 
            rEdit = {0, topY, w, h-topY}; 
        }
        else if (settings.layout == LayoutMode::Widescreen) { 
            rFiles = {0, topY + 25, (float)settings.sidebarWidth, h-(topY+25)}; 
            rTerm = {w-(float)settings.terminalWidth, topY+25, (float)settings.terminalWidth, h-(topY+25)}; 
            rEdit = {(float)settings.sidebarWidth, topY, w-settings.sidebarWidth-settings.terminalWidth, h-topY}; 
        }
        else { 
            rFiles = {0, topY + 25, (float)settings.sidebarWidth, h-(topY+25)}; 
            rTerm = {(float)settings.sidebarWidth, h-(float)settings.terminalHeight, w-settings.sidebarWidth, (float)settings.terminalHeight}; 
            rEdit = {(float)settings.sidebarWidth, topY, w-settings.sidebarWidth, h-topY-settings.terminalHeight}; 
        }

        bool inputCaptured = isModalOpen; 
        if (!inputCaptured && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (m.y > topY && !app.resizeSide && !app.resizeTerm) {
                if (CheckCollisionPointRec(m, rFiles)) app.focus = 1; 
                else if (CheckCollisionPointRec(m, rTerm)) app.focus = 2; 
                else if (CheckCollisionPointRec(m, rEdit)) app.focus = 0;
            }
        }
        
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL); 
        bool shift = IsKeyDown(KEY_LEFT_SHIFT);
        if (ctrl && !shift && IsKeyPressed(KEY_O)) { fileMgr.openFileDialog(); app.focus=0; }
        if (ctrl && shift && IsKeyPressed(KEY_O)) { fileMgr.openFolderDialog(); app.focus=1; }

        if (!app.showSettings && !app.showAbout) {
            fileMgr.update(rFiles, app.focus==1 && !app.showMenuFile); 
            std::string sel = fileMgr.popSelectedFile();
            if (!sel.empty()) {
                auto lower = sel;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                bool isImage = lower.size() >= 4 &&
                               (lower.rfind(".png") == lower.size() - 4 ||
                                lower.rfind(".jpg") == lower.size() - 4 ||
                                lower.rfind(".jpeg") == lower.size() - 5 ||
                                lower.rfind(".gif") == lower.size() - 4 ||
                                lower.rfind(".bmp") == lower.size() - 4);
                bool isAudio = lower.size() >= 4 &&
                               (lower.rfind(".wav") == lower.size() - 4 ||
                                lower.rfind(".mp3") == lower.size() - 4 ||
                                lower.rfind(".ogg") == lower.size() - 4);
                if (isImage && settings.imagePreview) {
                    editor.setPreview(sel);
                    app.focus=0;
                } else if (isAudio && settings.audioPreview) {
                    editor.setPreview(sel);
                    app.focus=0;
                } else if (!isImage && !isAudio) {
                    editor.loadFile(sel);
                    app.focus=0;
                }
            }
            terminal.update(app.focus==2); 
            editor.update(rEdit, app.focus==0 && !app.showMenuFile && !app.showMenuHelp);
        }

        BeginDrawing();
            ClearBackground(theme.bg);
            DrawRectangle(0,0,w,Config::NAVBAR_HEIGHT,theme.panelBg); 
            
            // Draw Menus
            if(DrawMenuBtn({0,0,60,30},"File",mainFont, app.showMenuFile?theme.btnNormal:theme.panelBg)) { app.showMenuFile=!app.showMenuFile; app.showMenuHelp=false; }
            if(DrawMenuBtn({60,0,60,30},"Help",mainFont, app.showMenuHelp?theme.btnNormal:theme.panelBg)) { app.showMenuHelp=!app.showMenuHelp; app.showMenuFile=false; }
            if(DrawMenuBtn({120,0,100,30},"Settings",mainFont, theme.panelBg)) OpenModal(app, 1);

            // Draw Run Button
            float runX = w - 140; 
            Rectangle rRun = {runX, 0, 140, 30}; 
            bool hRun = CheckCollisionPointRec(m, rRun);
            DrawRectangleRec(rRun, hRun ? theme.runButton : theme.panelBg); 
            DrawRectangleLinesEx(rRun, 1, theme.border);
            DrawTextEx(mainFont, app.runMakefile ? "Run: Make" : "Run: File", {runX+10, 5}, 20, 1, WHITE);
            
            if (hRun && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !isModalOpen) {
                std::string cmd;
                if (app.runMakefile) {
#ifdef _WIN32
                    cmd = "start cmd /c \"make && pause\"";
#elif defined(__linux__)
                    cmd = "x-terminal-emulator -e sh -lc \"make; echo; read -n1 -p 'Press any key to close'\"";
#else
                    cmd = "osascript -e 'tell app \"Terminal\" to do script \"make; read -n 1\"'";
#endif
                }
                else {
                    std::string path = editor.getCurrentPath();
                    if (path.empty()) terminal.runCommand("echo No file selected");
                    else {
                        std::string buildCmd = settings.cFlags;
                        size_t pos = buildCmd.find("$FILE");
                        if (pos != std::string::npos) buildCmd.replace(pos, 5, "\"" + path + "\"");
#ifdef _WIN32
                        cmd = "start cmd /c \"" + buildCmd + " && echo. && pause\"";
#elif defined(__linux__)
                        cmd = "x-terminal-emulator -e sh -lc \"" + buildCmd + "; echo; read -n1 -p 'Press any key to close'\"";
#else
                        cmd = "osascript -e 'tell app \"Terminal\" to do script \"" + buildCmd + "; read -n 1\"'";
#endif
                    }
                }
                if (!cmd.empty()) { 
                    system(cmd.c_str()); 
                    terminal.runCommand("echo Launched external console...");
                }
            }
            if (hRun && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) app.runMakefile = !app.runMakefile;
            
            DrawLine(0,Config::NAVBAR_HEIGHT,w,Config::NAVBAR_HEIGHT,theme.border);

            if (settings.layout != LayoutMode::Focus) { 
                fileMgr.render(rFiles, mainFont); 
                terminal.render(rTerm, mainFont); 
            }
            editor.render(rEdit);
            
            // Highlight active panel
            Rectangle rf = (app.focus==0) ? rEdit : (app.focus==1) ? rFiles : rTerm; 
            DrawRectangleLinesEx(rf, 1, BLUE);

            // Dropdowns
            if (app.showMenuFile) {
                float mx=0,my=30,mw=260; 
                DrawRectangle(mx,my,mw,180,theme.panelBg); 
                DrawRectangleLines(mx,my,mw,180,theme.border);
                if(DrawMenuItem(mx,my,mw,"New (Ctrl+N)",mainFont)) { editor.createNewFile(); app.showMenuFile=false; }
                if(DrawMenuItem(mx,my+30,mw,"Open File (Ctrl+O)",mainFont)) { fileMgr.openFileDialog(); app.focus=0; app.showMenuFile=false; }
                if(DrawMenuItem(mx,my+60,mw,"Open Folder (Ctrl+Sh+O)",mainFont)) { fileMgr.openFolderDialog(); app.focus=1; app.showMenuFile=false; }
                if(DrawMenuItem(mx,my+90,mw,"Save (Ctrl+S)",mainFont)) { editor.saveFile(); app.showMenuFile=false; }
                if(DrawMenuItem(mx,my+120,mw,"Save As...",mainFont)) { editor.saveAs(); app.showMenuFile=false; }
                if(DrawMenuItem(mx,my+150,mw,"Exit",mainFont)) break;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(m, {mx,my,mw,180}) && m.y > 30) app.showMenuFile = false;
            }
            if (app.showMenuHelp) {
                float mx=60,my=30,mw=300; 
                DrawRectangle(mx,my,mw,130,theme.panelBg); 
                DrawRectangleLines(mx,my,mw,130,theme.border);
                if(DrawMenuItem(mx,my,mw,"About ctom", mainFont)) OpenModal(app, 2);
                DrawTextEx(mainFont,"Shortcuts:",{mx+10,my+35},18,1,theme.keyword);
                DrawTextEx(mainFont,"Ctrl+O/S/C/V/A", {mx+10,my+55},18,1,theme.menuText);
                DrawTextEx(mainFont,"Hover Tab 'x': Close", {mx+10,my+75},18,1,theme.menuText);
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(m, {mx,my,mw,130}) && m.y > 30) app.showMenuHelp = false;
            }

            if (app.showSettings) { 
                DrawRectangle(0,0,w,h,{0,0,0,100}); 
                DrawSettings({(w-500)/2, (h-420)/2, 500, 420}, mainFont, editor, terminal, app); 
            }
            if (app.showAbout) { 
                DrawRectangle(0,0,w,h,{0,0,0,100}); 
                DrawAbout({(w-400)/2, (h-250)/2, 400, 250}, mainFont, app, logoTexture); 
            }
            
            DrawToasts(mainFont, w, h);
        EndDrawing();
    }

    CloseAudioDevice();
    
    if (logoTexture.id > 0) UnloadTexture(logoTexture);
    fileMgr.cleanup(); 
    SaveSettings(); 
    UnloadFont(mainFont); 
    CloseWindow(); 
    return 0;
}
