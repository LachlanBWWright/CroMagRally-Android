package com.lachlan.cromagrally;

import org.libsdl.app.SDLActivity;

/**
 * Custom SDL activity for Cro-Mag Rally.
 * Explicitly declares library load order and entry point for SDL3.
 */
public class CroMagRallyActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        // SDL3 must be loaded before the game's native library
        return new String[] { "SDL3", "main" };
    }

    @Override
    protected String getMainFunction() {
        // SDL3 renames main() to SDL_main() via the SDL_main.h macro
        return "SDL_main";
    }
}
