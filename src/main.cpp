#include "SDL3/SDL_pixels.h"
#include "SDL3/SDL_stdinc.h"
#define DEBUG

#include <SDL3/SDL.h>
#include <assert.h>
#include <stdarg.h>

#define PROG_NAME "Graph Viewer"
#define WIDTH (4 * 200)
#define HEIGHT (5 * 120)

#ifdef DEBUG
#include <stdio.h>
#define log(...) printf("[LOG] :: " __VA_ARGS__)
#define assert_eq(x, y, ...)                                                   \
  if ((x) != (y)) {                                                            \
    fprintf(stderr, "!!! assertion failed %s != %s\n", #x, #y);                \
    printf("[FAILED] :: " __VA_ARGS__);                                        \
    assert(0);                                                                 \
  }
#define log_once(...)                                                          \
  {                                                                            \
    static bool once = false;                                                  \
    if (!once) {                                                               \
      log(__VA_ARGS__);                                                        \
      once = true;                                                             \
    }                                                                          \
  }
#else
#define log(...) ((void)0)
#endif

void do_checks(SDL_Surface *);
void draw(SDL_Surface *);

int main() {
  assert(SDL_Init(SDL_INIT_VIDEO));

  SDL_Window *window = SDL_CreateWindow(PROG_NAME, WIDTH, HEIGHT, 0);
  assert(window);
  log("Created: %s %dx%d\n", PROG_NAME, WIDTH, HEIGHT);

  // We have only 1 thread and resizing disabled. So 1 call to get surface it
  // fine
  SDL_Surface *surface = SDL_GetWindowSurface(window);
  assert(surface);
  do_checks(surface);

  bool quit = false;
  while (!quit) {
    SDL_Event event;
    if (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        quit = true;
      }
    }

    draw(surface);

    SDL_UpdateWindowSurface(window);
  }

  SDL_DestroyWindow(window);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
  SDL_Quit();
}

void do_checks(SDL_Surface *surface) {
  static bool once = false;
  if (!once) {
    SDL_PixelFormatDetails const *pixel_details =
        SDL_GetPixelFormatDetails(surface->format);
    assert_eq(pixel_details->bytes_per_pixel, 4, "%d\n",
              pixel_details->bytes_per_pixel);
    log("PixelFormat: %s : %d\n", SDL_GetPixelFormatName(surface->format),
        SDL_BITSPERPIXEL(surface->format));
    log("Surface: w:%d h:%d\n", surface->w, surface->h);
    once = true;
  }
  return;
}

void draw(SDL_Surface *surface) {
#ifndef DEBUG
  void *pixels = surface->pixels;
  SDL_PixelFormatDetails const *pixel_details =
      SDL_GetPixelFormatDetails(surface->format);
  Sint32 stride = pixel_details->bytes_per_pixel;
#endif
  for (int y = 0; y < surface->h; ++y) {
    for (int x = 0; x < surface->w; ++x) {
      Uint8 val = ((x / 32) + (y / 32)) % 2 == 0 ? 0xFF : 0x00;
#ifndef DEBUG
      Uint8 *target_pixel =
          ((Uint8 *)pixels + (y * surface->pitch) + (x * stride));
      target_pixel[0] = val;
      target_pixel[1] = val;
      target_pixel[2] = val;
#else
      log_once("Using SDL api for filling\n");
      SDL_WriteSurfacePixel(surface, x, y, val, val, val, 0xff);
#endif
    }
  }
}
