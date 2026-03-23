// SDL2main handles Vita entry point setup via libSDL2main.a
#include <SDL3/SDL_main.h>
#include "game.h"
#include "tools.h"

#ifdef __vita__
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/power.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static FILE* gLog = nullptr;
static void vitaLog(const char* msg) {
    if(gLog) { fprintf(gLog, "%s\n", msg); fflush(gLog); }
}

// Copy a single file using Vita native IO
static bool vitaCopyFile(const std::string& src, const std::string& dst) {
    SceUID in = sceIoOpen(src.c_str(), SCE_O_RDONLY, 0);
    if(in < 0) return false;
    SceUID out = sceIoOpen(dst.c_str(), SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if(out < 0) { sceIoClose(in); return false; }
    char buf[8192];
    int bytesRead;
    while((bytesRead = sceIoRead(in, buf, sizeof(buf))) > 0) {
        sceIoWrite(out, buf, bytesRead);
    }
    sceIoClose(in);
    sceIoClose(out);
    return true;
}

// Recursively copy directory using Vita native IO
static void vitaCopyDir(const std::string& src, const std::string& dst) {
    sceIoMkdir(dst.c_str(), 0666);
    SceUID dir = sceIoDopen(src.c_str());
    if(dir < 0) {
        vitaLog(("  sceIoDopen failed for: " + src).c_str());
        return;
    }
    SceIoDirent entry;
    memset(&entry, 0, sizeof(entry));
    while(sceIoDread(dir, &entry) > 0) {
        if(strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0) {
            memset(&entry, 0, sizeof(entry));
            continue;
        }
        std::string srcPath = src + "/" + entry.d_name;
        std::string dstPath = dst + "/" + entry.d_name;
        if(SCE_S_ISDIR(entry.d_stat.st_mode)) {
            vitaCopyDir(srcPath, dstPath);
        } else {
            SceUID check = sceIoOpen(dstPath.c_str(), SCE_O_RDONLY, 0);
            if(check < 0) {
                vitaCopyFile(srcPath, dstPath);
            } else {
                sceIoClose(check);
            }
        }
        memset(&entry, 0, sizeof(entry));
    }
    sceIoDclose(dir);
}

// Read log file into lines
static std::vector<std::string> readLogLines() {
    std::vector<std::string> lines;
    FILE* f = fopen("ux0:data/tails-adventure/log.txt", "r");
    if(!f) {
        lines.push_back("(log file not found)");
        return lines;
    }
    char buf[256];
    while(fgets(buf, sizeof(buf), f)) {
        std::string line(buf);
        // Strip newline
        if(!line.empty() && line.back() == '\n') line.pop_back();
        lines.push_back(line);
    }
    fclose(f);
    return lines;
}

// Show error screen using SDL — displays log and waits for X to exit
static void vitaShowErrorScreen(const char* errorMsg) {
    // Try to init SDL minimally if not already done
    if(!SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK);
    }

    SDL_Window* win = TA::window;
    SDL_Renderer* ren = TA::renderer;
    bool ownedWindow = false;

    if(win == nullptr) {
        win = SDL_CreateWindow("Error", 960, 544, 0);
        ownedWindow = true;
    }
    if(ren == nullptr && win != nullptr) {
        ren = SDL_CreateRenderer(win, NULL);
    }

    if(win == nullptr || ren == nullptr) {
        // Absolute fallback — nothing we can do
        sceKernelExitProcess(1);
        return;
    }

    std::vector<std::string> logLines = readLogLines();

    // Add the crash error message at the bottom
    logLines.push_back("---");
    logLines.push_back("CRASH: " + std::string(errorMsg));
    logLines.push_back("---");
    logLines.push_back("Press X to exit");

    bool running = true;
    while(running) {
        // Draw background
        SDL_SetRenderDrawColor(ren, 20, 20, 40, 255);
        SDL_RenderClear(ren);

        // Draw title bar
        SDL_SetRenderDrawColor(ren, 180, 0, 0, 255);
        SDL_FRect titleBar = {0, 0, 960, 28};
        SDL_RenderFillRect(ren, &titleBar);

        // Title text
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        /* SDL_RenderDebugText not available in SDL2 */

        // Log lines — SDL_RenderDebugText uses 8x8 px chars
        float y = 36;
        int maxLines = (544 - 40) / 12;
        int startLine = (int)logLines.size() > maxLines
            ? (int)logLines.size() - maxLines : 0;

        for(int i = startLine; i < (int)logLines.size(); i++) {
            // Highlight crash and press X lines
            if(logLines[i].rfind("CRASH:", 0) == 0) {
                SDL_SetRenderDrawColor(ren, 255, 100, 100, 255);
            } else if(logLines[i] == "Press X to exit") {
                SDL_SetRenderDrawColor(ren, 100, 255, 100, 255);
            } else {
                SDL_SetRenderDrawColor(ren, 220, 220, 220, 255);
            }
            SDL_RenderDebugText(ren, 8, y, logLines[i].c_str());
            y += 12;
        }

        SDL_RenderPresent(ren);

        // Poll for X button (b1) or Cross to exit
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            if(event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if(event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                if(event.gbutton.button == 1) { // Cross / X = b1
                    running = false;
                }
            }
            // Also accept any key
            if(event.type == SDL_EVENT_KEY_DOWN) {
                running = false;
            }
        }
    }

    if(ownedWindow) {
        if(ren) SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
    }

    sceKernelExitProcess(0);
}
#endif

int main(int argc, char* argv[]) {
#ifdef __vita__
    // Open log immediately before anything else
    FILE* earlyLog = fopen("ux0:data/tails-adventure/early.txt", "w");
    if(earlyLog) { fprintf(earlyLog, "main() reached\n"); fflush(earlyLog); fclose(earlyLog); }

    // Boost CPU to 444MHz, GPU to 222MHz (default is ~111MHz = 15fps)
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    sceIoMkdir("ux0:data/tails-adventure", 0666);
    sceIoMkdir("ux0:data/tails-adventure/assets", 0666);

    gLog = fopen("ux0:data/tails-adventure/log.txt", "w");
    vitaLog("Starting Tails Adventure...");

    vitaLog("Copying assets app0:/assets -> ux0:data/tails-adventure/assets...");
    vitaCopyDir("app0:/assets", "ux0:data/tails-adventure/assets");
    vitaLog("Asset copy done");
#endif

    for(int pos = 1; pos < argc; pos++) {
        TA::arguments.insert(argv[pos]);
    }

#ifdef __vita__
    vitaLog("Initialising game...");
    try {
#endif

    TA_Game game;

#ifdef __vita__
    vitaLog("Game initialised, entering loop...");
#endif

    while(game.process()) {
        game.update();
    }

#ifdef __vita__
    } catch(const std::exception& e) {
        vitaLog(("EXCEPTION: " + std::string(e.what())).c_str());
        if(gLog) fclose(gLog);
        gLog = nullptr;
        vitaShowErrorScreen(e.what());
        return 1;
    } catch(...) {
        vitaLog("UNKNOWN EXCEPTION caught in main");
        if(gLog) fclose(gLog);
        gLog = nullptr;
        vitaShowErrorScreen("Unknown exception - check log for last known step");
        return 1;
    }

    vitaLog("Game exited cleanly.");
    if(gLog) fclose(gLog);
    sceKernelExitProcess(0);
#endif

    return 0;
}
