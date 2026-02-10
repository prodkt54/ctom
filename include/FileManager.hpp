#pragma once
#include "Globals.hpp"
#include <filesystem>

namespace fs = std::filesystem;

// External platform functions
extern std::string OpenWindowsFolderPicker();
extern std::string OpenWindowsFilePicker(const char* initialDir = nullptr);
extern std::string SaveWindowsFileDialog(const char* defaultName = nullptr);

class FileManager {
private:
    fs::path currentPath;
    std::vector<fs::directory_entry> entries;
    int scrollIndex = 0;
    float itemHeight = 24.0f;
    std::string selectedFile = "";
    bool isLoaded = false;
    
    Texture2D folderIcon = { 0 };

public:
    void init();
    void cleanup();
    void refresh();
    
    void openFolderDialog();
    void openFileDialog();
    std::string popSelectedFile();
    
    void update(Rectangle bounds, bool isFocused);
    void render(Rectangle bounds, Font font);
};
