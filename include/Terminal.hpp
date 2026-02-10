#pragma once
#include "Globals.hpp"
#include <vector>
#include <string>

class Terminal {
private:
    std::vector<std::string> displayHistory;
    std::vector<std::string> cmdHistory;
    int historyIndex = -1;
    std::string inputBuffer;
    
    // Use void* to avoid including windows.h here
    void* hChildStd_IN_Rd = nullptr;
    void* hChildStd_IN_Wr = nullptr;
    void* hChildStd_OUT_Rd = nullptr;
    void* hChildStd_OUT_Wr = nullptr;
    void* hProcess = nullptr; 

    void createShellProcess();
    void readFromPipe();
    void writeToPipe(const std::string& cmd);

public:
    Terminal();
    ~Terminal();
    
    void init();
    void close();
    void update(bool isFocused);
    void render(Rectangle bounds, Font font);
    void runCommand(const std::string& cmd);
};