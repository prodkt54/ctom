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
#elif defined(__APPLE__)
    #include <unistd.h>
    #include <fcntl.h>
    #include <signal.h>
    #include <sys/wait.h>
    #include <util.h>
    #include <termios.h>
#elif defined(__linux__)
    #include <unistd.h>
    #include <fcntl.h>
    #include <signal.h>
    #include <sys/wait.h>
    #include <pty.h>
    #include <termios.h>
#else
    #include <unistd.h>
#endif

#include "../include/Terminal.hpp"

#include <iostream>
#include <cstdio>
#include <memory>
#include <array>
#include <cstring>

static Color AnsiColor(int code) {
    switch (code) {
        case 30: return BLACK;
        case 31: return (Color){220, 50, 47, 255};
        case 32: return (Color){133, 153, 0, 255};
        case 33: return (Color){181, 137, 0, 255};
        case 34: return (Color){38, 139, 210, 255};
        case 35: return (Color){211, 54, 130, 255};
        case 36: return (Color){42, 161, 152, 255};
        case 37: return RAYWHITE;
        case 90: return DARKGRAY;
        case 91: return (Color){255, 85, 85, 255};
        case 92: return (Color){152, 195, 121, 255};
        case 93: return (Color){229, 192, 123, 255};
        case 94: return (Color){97, 175, 239, 255};
        case 95: return (Color){198, 120, 221, 255};
        case 96: return (Color){86, 182, 194, 255};
        case 97: return WHITE;
        default: return WHITE;
    }
}

static void AppendSegment(TerminalLine& line, const std::string& text, Color color) {
    if (text.empty()) return;
    if (!line.segments.empty() && line.segments.back().color.r == color.r &&
        line.segments.back().color.g == color.g && line.segments.back().color.b == color.b) {
        line.segments.back().text += text;
    } else {
        line.segments.push_back({text, color});
    }
}

static void ClearLine(TerminalLine& line) {
    line.segments.clear();
}

static void BackspaceLine(TerminalLine& line) {
    if (line.segments.empty()) return;
    TerminalSegment& seg = line.segments.back();
    if (!seg.text.empty()) {
        seg.text.pop_back();
        if (seg.text.empty()) line.segments.pop_back();
    } else {
        line.segments.pop_back();
    }
}

static void AppendPlainLine(std::vector<TerminalLine>& lines, const std::string& text, Color color) {
    TerminalLine line;
    AppendSegment(line, text, color);
    lines.push_back(line);
}

static void AppendAnsiText(std::vector<TerminalLine>& lines, Color& currentColor, bool& pendingCR, const std::string& input) {
    if (lines.empty()) lines.push_back(TerminalLine{});
    TerminalLine* line = &lines.back();

    std::string buffer;
    bool inEscape = false;
    std::string esc;

    auto flush = [&]() {
        AppendSegment(*line, buffer, currentColor);
        buffer.clear();
    };

    for (size_t i = 0; i < input.size(); i++) {
        char c = input[i];
        if (!inEscape) {
            if (c == 0x1B) { // ESC
                flush();
                inEscape = true;
                esc.clear();
            } else if (c == '\r') {
                flush();
                lines.push_back(TerminalLine{});
                line = &lines.back();
                pendingCR = true;
            } else if (c == '\n') {
                if (pendingCR) {
                    pendingCR = false;
                    // finish current line and move to next
                }
                flush();
                lines.push_back(TerminalLine{});
                line = &lines.back();
            } else if (c == 0x08 || c == 0x7F) { // backspace/delete
                flush();
                BackspaceLine(*line);
            } else {
                buffer += c;
            }
        } else {
            esc += c;
            if (c == 'm') {
                // Parse SGR
                // esc format like "[31m" or "[0m"
                if (!esc.empty() && esc[0] == '[') {
                    std::string params = esc.substr(1, esc.size() - 2);
                    if (params.empty()) params = "0";
                    size_t start = 0;
                    while (start < params.size()) {
                        size_t sep = params.find(';', start);
                        std::string token = params.substr(start, sep == std::string::npos ? std::string::npos : sep - start);
                        int code = std::atoi(token.c_str());
                        if (code == 0) {
                            currentColor = theme.text;
                        } else if ((code >= 30 && code <= 37) || (code >= 90 && code <= 97)) {
                            currentColor = AnsiColor(code);
                        }
                        if (sep == std::string::npos) break;
                        start = sep + 1;
                    }
                }
                inEscape = false;
            }
        }
    }
    flush();
}

Terminal::Terminal() {}
Terminal::~Terminal() { close(); }

void Terminal::init() {
    createShellProcess();
#ifdef _WIN32
    AppendPlainLine(displayHistory, "Microsoft Windows [CMD Session]", theme.text);
#elif defined(__APPLE__)
    AppendPlainLine(displayHistory, "macOS Terminal", theme.text);
#else
    AppendPlainLine(displayHistory, "POSIX Shell", theme.text);
#endif
    AppendPlainLine(displayHistory, "Integrated Terminal Ready.", theme.text);
    AppendPlainLine(displayHistory, " ", theme.text);
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
#elif defined(__APPLE__)
    currentColor = theme.text;
    int masterFd = -1;
    pid_t pid = forkpty(&masterFd, NULL, NULL, NULL);
    if (pid == 0) {
        const char* shell = settings.shellPath.empty() ? "/bin/zsh" : settings.shellPath.c_str();
        const char* base = strrchr(shell, '/');
        const char* name = base ? base + 1 : shell;
        setenv("TERM", "xterm-256color", 1);
        execl(shell, name, "-l", (char*)NULL);
        _exit(1);
    } else if (pid > 0) {
        ptyFd = masterFd;
        shellPid = (int)pid;
        int flags = fcntl(ptyFd, F_GETFL, 0);
        fcntl(ptyFd, F_SETFL, flags | O_NONBLOCK);
    }
#elif defined(__linux__)
    currentColor = theme.text;
    int masterFd = -1;
    pid_t pid = forkpty(&masterFd, NULL, NULL, NULL);
    if (pid == 0) {
        const char* shell = settings.shellPath.empty() ? "/bin/bash" : settings.shellPath.c_str();
        const char* base = strrchr(shell, '/');
        const char* name = base ? base + 1 : shell;
        setenv("TERM", "xterm-256color", 1);
        execl(shell, name, "-l", (char*)NULL);
        _exit(1);
    } else if (pid > 0) {
        ptyFd = masterFd;
        shellPid = (int)pid;
        int flags = fcntl(ptyFd, F_GETFL, 0);
        fcntl(ptyFd, F_SETFL, flags | O_NONBLOCK);
    }
#endif
}

void Terminal::close() {
#ifdef _WIN32
    if (hProcess) { TerminateProcess(hProcess, 0); CloseHandle(hProcess); hProcess = nullptr; }
    if (hChildStd_IN_Wr) CloseHandle(hChildStd_IN_Wr); if (hChildStd_OUT_Rd) CloseHandle(hChildStd_OUT_Rd);
#endif
#if defined(__APPLE__) || defined(__linux__)
    if (shellPid > 0) {
        kill(shellPid, SIGHUP);
        waitpid(shellPid, nullptr, 0);
        shellPid = -1;
    }
    if (ptyFd >= 0) {
        ::close(ptyFd);
        ptyFd = -1;
    }
#endif
}

void Terminal::readFromPipe() {
#ifdef _WIN32
    if (!hChildStd_OUT_Rd) return;
    DWORD dwRead, dwAvail, dwLeft;
    if (PeekNamedPipe(hChildStd_OUT_Rd, NULL, 0, NULL, &dwAvail, &dwLeft) && dwAvail > 0) {
        char buffer[4096];
        if (ReadFile(hChildStd_OUT_Rd, buffer, sizeof(buffer) - 1, &dwRead, NULL) && dwRead > 0) {
            buffer[dwRead] = '\0';
            currentColor = theme.text;
            AppendAnsiText(displayHistory, currentColor, pendingCR, std::string(buffer));
        }
    }
#elif defined(__APPLE__) || defined(__linux__)
    if (ptyFd < 0) return;
    char buffer[4096];
    ssize_t n = read(ptyFd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        AppendAnsiText(displayHistory, currentColor, pendingCR, std::string(buffer));
    }
#endif
}

void Terminal::writeToPipe(const std::string& cmd) {
#ifdef _WIN32
    if (!hChildStd_IN_Wr) return; std::string fullCmd = cmd + "\r\n"; DWORD dwWritten;
    WriteFile(hChildStd_IN_Wr, fullCmd.c_str(), fullCmd.size(), &dwWritten, NULL);
#endif
#ifdef __APPLE__
    if (ptyFd < 0) return;
    std::string fullCmd = cmd + "\n";
    ::write(ptyFd, fullCmd.c_str(), fullCmd.size());
#elif defined(__linux__)
    if (ptyFd < 0) return;
    std::string fullCmd = cmd + "\n";
    ::write(ptyFd, fullCmd.c_str(), fullCmd.size());
#endif
}

void Terminal::writeRawToPipe(const std::string& data) {
#ifdef _WIN32
    if (!hChildStd_IN_Wr) return;
    DWORD dwWritten;
    WriteFile(hChildStd_IN_Wr, data.c_str(), data.size(), &dwWritten, NULL);
#endif
#ifdef __APPLE__
    if (ptyFd < 0) return;
    ::write(ptyFd, data.c_str(), data.size());
#elif defined(__linux__)
    if (ptyFd < 0) return;
    ::write(ptyFd, data.c_str(), data.size());
#endif
}

void Terminal::runCommand(const std::string& cmd) {
    if (cmd == "clear" || cmd == "cls") {
        displayHistory.clear();
        scrollOffset = 0;
    } else {
#ifdef _WIN32
        writeToPipe(cmd);
#else
        writeToPipe(cmd);
#endif
        cmdHistory.push_back(cmd);
    }
}

void Terminal::update(bool isFocused) {
    readFromPipe();
    if (!isFocused) return;

    // Scrollback
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        scrollOffset -= (int)wheel;
        if (scrollOffset < 0) scrollOffset = 0;
        int maxScroll = (int)displayHistory.size();
        if (scrollOffset > maxScroll) scrollOffset = maxScroll;
    }

#ifdef _WIN32
    // History navigation
    if (IsKeyPressed(KEY_UP)) { 
        if (!cmdHistory.empty()) { 
            if (historyIndex == -1) historyIndex = cmdHistory.size()-1; 
            else if(historyIndex > 0) historyIndex--; 
            inputBuffer = cmdHistory[historyIndex]; 
        } 
    }
    if (IsKeyPressed(KEY_DOWN)) { 
        if (historyIndex != -1) { 
            if (historyIndex < (int)cmdHistory.size()-1) { 
                historyIndex++; 
                inputBuffer = cmdHistory[historyIndex]; 
            } else { 
                historyIndex = -1; 
                inputBuffer = ""; 
            } 
        } 
    }

    // Input text
    int c = GetCharPressed();
    while (c > 0) { if (c >= 32 && c <= 126) inputBuffer += (char)c; c = GetCharPressed(); }
    if (IsKeyPressed(KEY_BACKSPACE) && !inputBuffer.empty()) inputBuffer.pop_back();
    
    // Execute
    if (IsKeyPressed(KEY_ENTER)) { 
        if (!inputBuffer.empty()) {
            runCommand(inputBuffer);
            inputBuffer = "";
            historyIndex = -1;
        } else {
            writeToPipe("");
        }
    }
#else
    // Send key input directly to PTY for real terminal behavior
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl && IsKeyPressed(KEY_C)) { writeRawToPipe("\x03"); return; }
    if (ctrl && IsKeyPressed(KEY_D)) { writeRawToPipe("\x04"); return; }
    if (ctrl && IsKeyPressed(KEY_Z)) { writeRawToPipe("\x1A"); return; }

    int c = GetCharPressed();
    while (c > 0) {
        char ch = (char)c;
        writeRawToPipe(std::string(1, ch));
        c = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE)) writeRawToPipe("\x7f");
    if (IsKeyPressed(KEY_ENTER)) writeRawToPipe("\n");
    if (IsKeyPressed(KEY_TAB)) writeRawToPipe("\t");
    if (IsKeyPressed(KEY_UP)) writeRawToPipe("\x1b[A");
    if (IsKeyPressed(KEY_DOWN)) writeRawToPipe("\x1b[B");
    if (IsKeyPressed(KEY_LEFT)) writeRawToPipe("\x1b[D");
    if (IsKeyPressed(KEY_RIGHT)) writeRawToPipe("\x1b[C");
#endif
}

void Terminal::render(Rectangle bounds, Font font) {
    if (bounds.height <= 0) return;
    
    // --- FIX: DRAW HEADER INSIDE BOUNDS ---
    float headerH = 25.0f;
    
    BeginScissorMode((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height);
        float y = bounds.y + bounds.height - 25;
#ifdef _WIN32
        DrawTextEx(font, ("> " + inputBuffer + "_").c_str(), {bounds.x + 5, y}, Config::FONT_SIZE_UI, 1, theme.keyword);
#endif
        int startIndex = (int)displayHistory.size() - 1 - scrollOffset;
        for (int i = startIndex; i >= 0; i--) {
            y -= 22;
            if (y < bounds.y) break;
            float x = bounds.x + 5;
            for (const auto& seg : displayHistory[i].segments) {
            if (!seg.text.empty()) {
                DrawTextEx(font, seg.text.c_str(), {x, y}, Config::FONT_SIZE_UI, 1, seg.color);
                x += MeasureTextEx(font, seg.text.c_str(), Config::FONT_SIZE_UI, 1).x;
            }
        }
        }
    EndScissorMode();
}
