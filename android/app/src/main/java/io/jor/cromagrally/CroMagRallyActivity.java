package io.jor.cromagrally;

import org.libsdl.app.SDLActivity;

/**
 * CroMagRallyActivity - Custom SDL3 activity for Cro-Mag Rally Android port.
 *
 * Declares which native libraries to load and which symbol is the entry point.
 * SDL3 renames main() to SDL_main() via the SDL_main.h macro, so we must
 * explicitly declare "SDL_main" here.
 */
public class CroMagRallyActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        // SDL3 must be loaded before main.
        return new String[] { "SDL3", "main" };
    }

    @Override
    protected String getMainFunction() {
        // SDL3 renames main() to SDL_main() via the SDL_main.h macro.
        return "SDL_main";
    }
}
