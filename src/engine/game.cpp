#include "game.h"
#include <chrono>
#include "SDL3/SDL_hints.h"
#include "SDL3_mixer/SDL_mixer.h"
#include "error.h"
#include "gamepad.h"
#include "keyboard.h"
#include "resource_manager.h"
#include "save.h"
#include "sound.h"
#include "tools.h"
#include "touchscreen.h"

#ifdef __vita__
#include <cstdio>
static FILE* gLog = nullptr;
static void vitaLog(const char* msg) {
    if(gLog) { fprintf(gLog, "%s\n", msg); fflush(gLog); }
}
static bool vitaShowFPS = false;
static float vitaFPS = 0.0f;
static int vitaFPSFrameCount = 0;
static std::chrono::time_point<std::chrono::high_resolution_clock> vitaFPSTimer;
#endif

TA_Game::TA_Game() {
#ifdef __vita__
    gLog = fopen("ux0:data/tails-adventure/log.txt", "w");
    vitaLog("TA_Game constructor started");
    vitaFPSTimer = std::chrono::high_resolution_clock::now();
#endif

    initSDL();
#ifdef __vita__
    vitaLog("initSDL() OK");
#endif

    TA::save::load();
#ifdef __vita__
    vitaLog("save::load() OK");
#endif

    createWindow();
#ifdef __vita__
    vitaLog("createWindow() OK");
#endif

    TA::random::init(std::chrono::high_resolution_clock::now().time_since_epoch().count());
#ifdef __vita__
    vitaLog("random::init() OK");
#endif

    TA::gamepad::init();
#ifdef __vita__
    vitaLog("gamepad::init() OK");
#endif

    TA::resmgr::load();
#ifdef __vita__
    vitaLog("resmgr::load() OK");
#endif

    font.loadFont("fonts/pause_menu.toml");
#ifdef __vita__
    vitaLog("font loaded OK");
#endif

    startTime = std::chrono::high_resolution_clock::now();
    screenStateMachine.init();
#ifdef __vita__
    vitaLog("screenStateMachine::init() OK - constructor done");
#endif
}

void TA_Game::initSDL() {
#ifdef __vita__
    vitaLog("initSDL: calling SDL_Init...");
#endif
    if(!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMEPAD |
                 SDL_INIT_EVENTS | 0)) {
        TA::handleSDLError("%s", "SDL init failed");
    }
#ifdef __vita__
    vitaLog("initSDL: SDL_Init OK, calling Mix_Init...");
#endif
    if(Mix_Init(MIX_INIT_OGG) != MIX_INIT_OGG) {
        TA::handleSDLError("%s", "SDL_mixer init failed");
    }
#ifdef __vita__
    vitaLog("initSDL: Mix_Init OK, calling Mix_OpenAudio...");
#endif
    SDL_AudioSpec audioSpec;
    audioSpec.channels = TA_SOUND_CHANNEL_MAX;
    audioSpec.format = MIX_DEFAULT_FORMAT;
    audioSpec.freq = 44100;
    if(!Mix_OpenAudio(0, &audioSpec)) {
        TA::handleSDLError("%s", "Mix_OpenAudio failed");
    }
#ifdef __vita__
    vitaLog("initSDL: Mix_OpenAudio OK");
#endif
    SDL_HideCursor();
}

void TA_Game::createWindow() {
#ifdef __vita__
    vitaLog("createWindow: creating window...");
    TA::window = SDL_CreateWindow("Tails Adventure", 960, 544, 0);
#else
    TA::window = SDL_CreateWindow("Tails Adventure", defaultWindowWidth, defaultWindowHeight, SDL_WINDOW_FULLSCREEN);
#endif
    if(TA::window == nullptr) {
        TA::handleSDLError("%s", "failed to create window");
    }
#ifdef __vita__
    vitaLog("createWindow: window OK, creating renderer...");
#endif
#ifdef __vita__
    TA::renderer = SDL_CreateRenderer(TA::window, NULL);
#else
    TA::renderer = SDL_CreateRenderer(TA::window, NULL);
#endif
    if(TA::renderer == nullptr) {
        TA::handleSDLError("%s", "failed to create renderer");
    }
#ifdef __vita__
    vitaLog("createWindow: renderer OK, calling updateWindowSize...");
#endif
    updateWindowSize();
    SDL_SetRenderDrawBlendMode(TA::renderer, SDL_BLENDMODE_BLEND);
#ifdef __vita__
    vitaLog("createWindow: setting blend mode OK, getting vsync...");
#endif
    int vsync = TA::save::getParameter("vsync");
#ifdef __vita__
    // SDL3 vsync on Vita locks to wrong refresh rate — force it off
    vitaLog("createWindow: forcing vsync off for Vita...");
    SDL_SetRenderVSync(TA::renderer, 0);
#else
    SDL_SetRenderVSync(TA::renderer, (vsync == 2 ? -1 : vsync));
#endif
#ifdef __vita__
    vitaLog("createWindow: done");
#endif
}

void TA_Game::toggleFullscreen() {
#ifndef __vita__
    fullscreen = !fullscreen;
    SDL_SetWindowFullscreen(TA::window, fullscreen);
    updateWindowSize();
#endif
}

void TA_Game::updateWindowSize() {
#ifdef __vita__
    // On Vita the screen size never changes — only create texture once
    if(targetTexture != nullptr) { return; }
    vitaLog("updateWindowSize: start");
    windowWidth  = 960;
    windowHeight = 544;
    TA::screenHeight = 160;
    TA::screenWidth  = 960 * 160 / 544;
    TA::scaleFactor  = 1;

    vitaLog("updateWindowSize: creating target texture...");
    targetTexture = SDL_CreateTexture(
        TA::renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, windowWidth, windowHeight);
    if(targetTexture == nullptr) {
        vitaLog("updateWindowSize: ERROR - targetTexture is null!");
        TA::handleSDLError("%s", "failed to create target texture");
    }
    SDL_SetTextureScaleMode(targetTexture, SDL_SCALEMODE_LINEAR);
    vitaLog("updateWindowSize: done");
#else
    int baseHeight = (TA::getBaseHeight(TA::save::getParameter("base_height")));
    float pixelAR = (TA::save::getParameter("pixel_ar") == 0 ? 1 : float(7) / 8);

    if(!fullscreen) {
        int factor = TA::save::getParameter("window_size");
        int neededWidth = baseHeight * 16 / 9 * factor;
        int neededHeight = baseHeight * factor;
        SDL_SetWindowSize(TA::window, neededWidth, neededHeight);
    }

    SDL_GetWindowSize(TA::window, &windowWidth, &windowHeight);
    TA::screenWidth = baseHeight * windowWidth / windowHeight * pixelAR;
    TA::screenHeight = baseHeight;
    TA::scaleFactor = (windowWidth + TA::screenWidth - 1) / TA::screenWidth;

    if(targetWidth < TA::screenWidth * TA::scaleFactor || targetHeight < TA::screenHeight * TA::scaleFactor) {
        targetWidth = std::max(targetWidth, TA::screenWidth * TA::scaleFactor);
        targetHeight = std::max(targetHeight, TA::screenHeight * TA::scaleFactor);

        if(targetTexture != nullptr) {
            SDL_DestroyTexture(targetTexture);
        }
        targetTexture = SDL_CreateTexture(
            TA::renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, targetWidth, targetHeight);
    }

    SDL_SetTextureScaleMode(
        targetTexture, TA::save::getParameter("scale_mode") ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
#endif
}

bool TA_Game::process() {
    updateWindowSize();

    TA::touchscreen::update();
    TA::keyboard::update();
    TA::gamepad::update();
    TA::sound::update();
    SDL_Event event;

    while(SDL_PollEvent(&event)) {
        if(event.type == SDL_EVENT_QUIT) {
            return false;
        }
        if(event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_MOTION ||
            event.type == SDL_EVENT_FINGER_UP) {
            TA::touchscreen::handleEvent(event.tfinger);
        } else if(event.type == SDL_EVENT_GAMEPAD_ADDED || event.type == SDL_EVENT_GAMEPAD_REMOVED) {
            TA::gamepad::handleEvent(event.gdevice);
        }
    }

#ifndef __vita__
    if(TA::keyboard::isScancodePressed(SDL_SCANCODE_RALT) && TA::keyboard::isScancodePressed(SDL_SCANCODE_RETURN) &&
        (TA::keyboard::isScancodeJustPressed(SDL_SCANCODE_RALT) ||
            TA::keyboard::isScancodeJustPressed(SDL_SCANCODE_RETURN))) {
        toggleFullscreen();
    }
#endif

#ifdef __vita__
    // L + Triangle toggles FPS overlay
    if(TA::gamepad::isControllerButtonPressed(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) &&
       TA::gamepad::isControllerButtonJustPressed(SDL_GAMEPAD_BUTTON_NORTH)) {
        vitaShowFPS = !vitaShowFPS;
    }
#endif

    if(screenStateMachine.isQuitNeeded()) {
        return false;
    }
    return true;
}

void TA_Game::update() {
    currentTime = std::chrono::high_resolution_clock::now();
    TA::elapsedTime =
        static_cast<float>(std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - startTime).count()) /
        1e9F * 60;

    TA::elapsedTime = std::min(TA::elapsedTime, maxElapsedTime);
    startTime = currentTime;

    SDL_SetRenderTarget(TA::renderer, targetTexture);
    SDL_SetRenderDrawColor(TA::renderer, 0, 0, 0, 255);
    SDL_RenderClear(TA::renderer);

    if(screenStateMachine.update()) {
        startTime = std::chrono::high_resolution_clock::now();
    }

    if(TA::save::getParameter("frame_time")) {
        int frameTime = static_cast<int>(std::chrono::duration_cast<std::chrono::microseconds>(
            (std::chrono::high_resolution_clock::now() - startTime))
                .count());
        frameTimeSum += frameTime;
        frame += 1;
        if(frame % 60 == 0) {
            prevFrameTime = frameTimeSum / 60;
            frame = frameTimeSum = 0;
        }
        font.drawText(TA_Point(TA::screenWidth - 36, 24), std::to_string(prevFrameTime));
    }

#ifdef __vita__
    // FPS counter — L+Triangle to toggle
    if(vitaShowFPS) {
        vitaFPSFrameCount++;
        auto now = std::chrono::high_resolution_clock::now();
        float elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - vitaFPSTimer).count() / 1000.0f;
        if(elapsed >= 0.5f) {
            vitaFPS = vitaFPSFrameCount / elapsed;
            vitaFPSFrameCount = 0;
            vitaFPSTimer = now;
        }

        // Draw FPS background box
        SDL_SetRenderDrawColor(TA::renderer, 0, 0, 0, 180);
        SDL_FRect box = {2, 2, 72, 14};
        SDL_RenderFillRect(TA::renderer, &box);

        // Draw FPS text
        char fpsBuf[32];
        snprintf(fpsBuf, sizeof(fpsBuf), "FPS: %.1f", vitaFPS);
        SDL_SetRenderDrawColor(TA::renderer, 255, 255, 0, 255);
        SDL_RenderDebugText(TA::renderer, 4, 4, fpsBuf);
    }
#endif

    SDL_SetRenderTarget(TA::renderer, nullptr);
    SDL_SetRenderDrawColor(TA::renderer, 0, 0, 0, 255);
    SDL_RenderClear(TA::renderer);

    SDL_FRect srcRect{0, 0, (float)TA::screenWidth * TA::scaleFactor, (float)TA::screenHeight * TA::scaleFactor};
    SDL_FRect dstRect{0, 0, (float)windowWidth, (float)windowHeight};
    SDL_RenderTexture(TA::renderer, targetTexture, &srcRect, &dstRect);
    SDL_RenderPresent(TA::renderer);
}

TA_Game::~TA_Game() {
    TA::save::writeToFile();
    TA::gamepad::quit();
    TA::resmgr::quit();

    SDL_DestroyTexture(targetTexture);
    SDL_DestroyRenderer(TA::renderer);
    SDL_DestroyWindow(TA::window);

    Mix_CloseAudio();
    Mix_Quit();
    SDL_Quit();

#ifdef __vita__
    if(gLog) { fprintf(gLog, "Destructor done, exiting.\n"); fclose(gLog); }
#endif
}
