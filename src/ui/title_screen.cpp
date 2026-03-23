#include "title_screen.h"
#include <algorithm>
#include <cmath>
#include "error.h"
#include "save.h"
#include "tools.h"

#ifdef __vita__
#include "SDL3/SDL.h"
#endif

void TA_TitleScreen::init() {
    backgroundSprite.load("title_screen/title_screen.png");

#ifdef __ANDROID__
    pressStartSprite.load("title_screen/touch_to_start.png");
#else
    pressStartSprite.load("title_screen/press_start.png");
#endif

    TA::sound::playMusic("sound/title.vgm");
    enterSound.load("sound/enter.ogg", TA_SOUND_CHANNEL_SFX1, 0);

    controller.load();
    button.setRectangle(TA_Point(0, 0), TA_Point(TA::screenWidth, TA::screenHeight));
}

TA_ScreenState TA_TitleScreen::update() {
    backgroundSprite.setPosition(
        (TA::screenWidth - backgroundSprite.getWidth()) / 2, (TA::screenHeight - backgroundSprite.getHeight()) / 2);
    pressStartSprite.setPosition(TA::screenWidth / 2 - pressStartSprite.getWidth() / 2,
        104 + (TA::screenHeight - backgroundSprite.getHeight()) / 2);

    TA::drawScreenRect(0, 0, 102, 255);
    backgroundSprite.draw();
    controller.update();
    button.update();

#ifdef __vita__
    // "Ported by izzler" credit — bottom right corner, semi-transparent white
    const char* credit = "Ported by izzler";
    // SDL_RenderDebugText uses 8px wide chars
    int creditX = TA::screenWidth - (int)(strlen(credit) * 8) - 4;
    int creditY = TA::screenHeight - 12;
    SDL_SetRenderDrawColor(TA::renderer, 0, 0, 0, 120);
    SDL_FRect bg = { (float)(creditX - 2), (float)(creditY - 1),
                     (float)(strlen(credit) * 8 + 4), 11 };
    SDL_RenderFillRect(TA::renderer, &bg);
    SDL_SetRenderDrawColor(TA::renderer, 255, 255, 255, 200);
    SDL_RenderDebugText(TA::renderer, creditX, creditY, credit);
#endif

    switch(state) {
        case STATE_PRESS_START:
            updatePressStart();
            break;
        case STATE_HIDE_PRESS_START:
            updateHidePressStart();
            break;
        case STATE_EXIT:
            updateExit();
            break;
        default:
            break;
    }

    if(shouldExit) {
        return TA_SCREENSTATE_MAIN_MENU;
    }
    return TA_SCREENSTATE_CURRENT;
}

void TA_TitleScreen::updatePressStart() {
    const float idleTime = 30;
    const float transitionTime = 5;

    timer += TA::elapsedTime;
    timer = std::fmod(timer, (idleTime + transitionTime) * 2);

    if(timer < transitionTime) {
        alpha = 255 * timer / transitionTime;
    } else if(timer < transitionTime + idleTime) {
        alpha = 255;
    } else if(timer < transitionTime * 2 + idleTime) {
        alpha = 255 - 255 * (timer - (idleTime + transitionTime)) / transitionTime;
    } else {
        alpha = 0;
    }

    pressStartSprite.setAlpha(alpha);
    pressStartSprite.draw();

    if(controller.isJustPressed(TA_BUTTON_PAUSE) || button.isPressed()) {
        state = STATE_HIDE_PRESS_START;
        enterSound.play();
    }
}

void TA_TitleScreen::updateHidePressStart() {
    const float disappearTime = 5;
    alpha += 255 * (disappearTime / TA::elapsedTime);

    pressStartSprite.setAlpha(alpha);
    pressStartSprite.draw();

    if(alpha >= 255) {
        state = STATE_EXIT;
        timer = 0;
        alpha = 255;
    }
}

void TA_TitleScreen::updateExit() {
    const float exitTime = 14;
    pressStartSprite.draw();

    timer += TA::elapsedTime;
    if(timer > exitTime) {
        shouldExit = true;
    }
}

void TA_TitleScreen::quit() {}
