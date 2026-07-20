#include "platform.h"
// on ios sdl renames main to SDL_main and provides its own uikit entry point
#if GB_IOS
#include <SDL.h>
#endif
#include "app.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    App app;
    app.run();
    return 0;
}
