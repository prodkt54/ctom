#pragma once
#include "Globals.hpp"
#include <vector>
#include <string>

struct TerminalSegment { 
    std::string text;
    Color color;
};

struct TerminalLine {
    std::vector<TerminalSegment> segments;
};

class Terminal {
private:
    std::vector<TerminalLine> displayHistory;
    std::vector<std::string> cmdHistory;
    int historyIndex = -1;
    std::string inputBuffer;
    int scrollOffset = 0;
    bool pendingCR = false;

    Color currentColor = WHITE;
#if defined(__APPLE__) || defined(__linux__)
    int ptyFd = -1;
    int shellPid = -1;
#endif
    
    // Use void* to avoid including windows.h here
    void* hChildStd_IN_Rd = nullptr;
    void* hChildStd_IN_Wr = nullptr;
    void* hChildStd_OUT_Rd = nullptr;
    void* hChildStd_OUT_Wr = nullptr;
    void* hProcess = nullptr; 

    void createShellProcess();
    void readFromPipe();
    void writeToPipe(const std::string& cmd);
    void writeRawToPipe(const std::string& data);

public:
    Terminal();
    ~Terminal();
    
    void init();
    void close();
    void update(bool isFocused);
    void render(Rectangle bounds, Font font);
    void runCommand(const std::string& cmd);
};
