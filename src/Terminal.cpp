// ... (Includes and CleanString helper stay the same) ...
#ifdef _WIN32
    #define CloseWindow Win32_CloseWindow
    #define ShowCursor  Win32_ShowCursor
    #define PlaySound   Win32_PlaySound
    #define LoadImage   Win32_LoadImage
    #define DrawText    Win32_DrawText
    #define DrawTextEx  Win32_DrawTextEx
    #define NOGDI 
    #define Rectangle Win32_Rectangle_Dummy
    #include <windows.h>
    #undef CloseWindow
    #undef ShowCursor
    #undef PlaySound
    #undef LoadImage
    #undef DrawText
    #undef DrawTextEx
    #undef Rectangle
    #undef ERROR 
#else
    #include <unistd.h>
#endif

#include "../include/Terminal.hpp"

#ifdef _WIN32
#include <iostream>
#include <cstdio>
#include <memory>
#include <array>
#endif

std::string CleanString(const std::string& input) {
    std::string output; bool inEscape = false;
    for (char c : input) { if (c == 0x1B) inEscape = true; else if (inEscape && c == 'm') inEscape = false; else if (!inEscape && c != '\r') output += c; }
    return output;
}

Terminal::Terminal() {}
Terminal::~Terminal() { close(); }

void Terminal::init() {
    createShellProcess();
    writeToPipe("cd /d %USERPROFILE%\\Documents"); writeToPipe("cls");
    displayHistory.push_back("Microsoft Windows [CMD Session]");
    displayHistory.push_back("Integrated Terminal Ready.");
    displayHistory.push_back(" ");
}

void Terminal::createShellProcess() {
#ifdef _WIN32
    SECURITY_ATTRIBUTES saAttr; saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); saAttr.bInheritHandle = TRUE; saAttr.lpSecurityDescriptor = NULL;
    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) return;
    if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) return;
    if (!CreatePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &saAttr, 0)) return;
    if (!SetHandleInformation(hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) return;
    PROCESS_INFORMATION piProcInfo; STARTUPINFOA siStartInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION)); ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = hChildStd_OUT_Wr; siStartInfo.hStdOutput = hChildStd_OUT_Wr; siStartInfo.hStdInput = hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW; siStartInfo.wShowWindow = 0; 
    char cmdLine[] = "cmd.exe";
    if (CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo)) {
        hProcess = piProcInfo.hProcess; CloseHandle(piProcInfo.hThread); CloseHandle(hChildStd_OUT_Wr); CloseHandle(hChildStd_IN_Rd);    
    }
#endif
}

void Terminal::close() {
#ifdef _WIN32
    if (hProcess) { TerminateProcess(hProcess, 0); CloseHandle(hProcess); hProcess = nullptr; }
    if (hChildStd_IN_Wr) CloseHandle(hChildStd_IN_Wr); if (hChildStd_OUT_Rd) CloseHandle(hChildStd_OUT_Rd);
#endif
}

void Terminal::readFromPipe() {
#ifdef _WIN32
    if (!hChildStd_OUT_Rd) return;
    DWORD dwRead, dwAvail, dwLeft;
    if (PeekNamedPipe(hChildStd_OUT_Rd, NULL, 0, NULL, &dwAvail, &dwLeft) && dwAvail > 0) {
        char buffer[4096];
        if (ReadFile(hChildStd_OUT_Rd, buffer, sizeof(buffer) - 1, &dwRead, NULL) && dwRead > 0) {
            buffer[dwRead] = '\0'; std::string str(buffer); size_t pos = 0;
            while ((pos = str.find('\n')) != std::string::npos) {
                std::string line = str.substr(0, pos); if (!line.empty() && line.back() == '\r') line.pop_back();
                displayHistory.push_back(CleanString(line)); str.erase(0, pos + 1);
            }
            if (!str.empty()) displayHistory.push_back(CleanString(str));
        }
    }
#endif
}

void Terminal::writeToPipe(const std::string& cmd) {
#ifdef _WIN32
    if (!hChildStd_IN_Wr) return; std::string fullCmd = cmd + "\r\n"; DWORD dwWritten;
    WriteFile(hChildStd_IN_Wr, fullCmd.c_str(), fullCmd.size(), &dwWritten, NULL);
#endif
}

void Terminal::runCommand(const std::string& cmd) { if (cmd == "clear" || cmd == "cls") displayHistory.clear(); else { writeToPipe(cmd); cmdHistory.push_back(cmd); } }

void Terminal::update(bool isFocused) {
    readFromPipe();
    if (!isFocused) return;
    if (IsKeyPressed(KEY_UP)) { if (!cmdHistory.empty()) { if (historyIndex == -1) historyIndex = cmdHistory.size()-1; else if(historyIndex > 0) historyIndex--; inputBuffer = cmdHistory[historyIndex]; } }
    if (IsKeyPressed(KEY_DOWN)) { if (historyIndex != -1) { if (historyIndex < (int)cmdHistory.size()-1) { historyIndex++; inputBuffer = cmdHistory[historyIndex]; } else { historyIndex = -1; inputBuffer = ""; } } }
    int c = GetCharPressed();
    while (c > 0) { if (c >= 32 && c <= 126) inputBuffer += (char)c; c = GetCharPressed(); }
    if (IsKeyPressed(KEY_BACKSPACE) && !inputBuffer.empty()) inputBuffer.pop_back();
    if (IsKeyPressed(KEY_ENTER)) { if (!inputBuffer.empty()) { runCommand(inputBuffer); inputBuffer = ""; historyIndex = -1; } else writeToPipe(""); }
}

void Terminal::render(Rectangle bounds, Font font) {
    if (bounds.height <= 0) return;
    
    // --- FIX: DRAW HEADER INSIDE BOUNDS ---
    float headerH = 25.0f;
    
    // 1. Draw Header
    DrawRectangle(bounds.x, bounds.y, bounds.width, headerH, theme.border);
    DrawTextEx(font, "TERMINAL", {bounds.x + 5, bounds.y + 2}, Config::FONT_SIZE_UI, 1, theme.text);
    
    // 2. Draw Content BG
    Rectangle contentRect = {bounds.x, bounds.y + headerH, bounds.width, bounds.height - headerH};
    DrawRectangleRec(contentRect, theme.panelBg);
    DrawRectangleLinesEx(bounds, 1, theme.border);

    BeginScissorMode((int)contentRect.x, (int)contentRect.y, (int)contentRect.width, (int)contentRect.height);
        float y = contentRect.y + contentRect.height - 25;
        DrawTextEx(font, ("> " + inputBuffer + "_").c_str(), {contentRect.x + 5, y}, Config::FONT_SIZE_UI, 1, theme.keyword);
        for (int i = displayHistory.size() - 1; i >= 0; i--) {
            y -= 22; if (y < contentRect.y) break;
            DrawTextEx(font, displayHistory[i].c_str(), {contentRect.x + 5, y}, Config::FONT_SIZE_UI, 1, theme.text);
        }
    EndScissorMode();
}