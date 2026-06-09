#pragma once

#include <SDL3/SDL.h>

namespace Dawn {

// Dawn owns its entire vocabulary and every value. Penumbra never sees these
// names — the resolvers (DawnResolvers) pour these into Penumbra style structs.
// This is the only place in Dawn where colours and pixel sizes live, mirroring
// the role Demo::Theme plays in the Penumbra demo.
struct Theme {
    SDL_Color ColorBackgroundBase    = { 15,  15,  18, 255}; // #0F0F12
    SDL_Color ColorSurface           = { 26,  26,  31, 255}; // #1A1A1F
    SDL_Color ColorSurfaceRaised     = { 36,  36,  41, 255}; // #242429
    SDL_Color ColorSurfaceOverlay    = { 46,  46,  53, 255}; // #2E2E35

    SDL_Color ColorBorderSubtle      = { 58,  58,  68, 255}; // #3A3A44
    SDL_Color ColorBorderFocus       = { 90,  90, 104, 255}; // #5A5A68

    SDL_Color ColorTextPrimary       = {234, 234, 240, 255}; // #EAEAF0
    SDL_Color ColorTextSecondary     = {144, 144, 160, 255}; // #9090A0
    SDL_Color ColorTextDisabled      = { 80,  80,  96, 255}; // #505060

    SDL_Color ColorAccent            = {232, 148,  58, 255}; // #E8943A  amber
    SDL_Color ColorAccentHovered     = {242, 168,  78, 255};
    SDL_Color ColorAccentPressed     = {212, 128,  38, 255};

    SDL_Color ColorWarning           = {232, 200,  74, 255}; // dirty indicator
    SDL_Color ColorDestructive       = {217,  96,  96, 255};

    SDL_Color ColorEntityPlatform    = { 74, 144, 217, 255}; // #4A90D9
    SDL_Color ColorEntityProp        = {126, 211,  33, 255}; // #7ED321
    SDL_Color ColorEntityTrigger     = {245, 166,  35, 255}; // #F5A623
    SDL_Color ColorEntityCharacter   = {189,  16, 224, 255};

    float SpacingSmall   =  6.0f;
    float SpacingMedium  = 10.0f;
    float SpacingLarge   = 16.0f;

    float PanelStripWidth    = 44.0f;
    float PanelContentWidth  = 220.0f;
    float StatusBarHeight    = 24.0f;

    float FontSizeBody   = 13.0f;
    float FontSizeSmall  = 11.0f;

    float BorderRadiusPanel  = 8.0f;
    float BorderRadiusButton = 5.0f;
    float BorderWidthDefault = 1.0f;
    float AnimColorSeconds   = 0.08f;
};

} // namespace Dawn
