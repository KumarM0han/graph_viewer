#include "SDL3/SDL_events.h"
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

template <typename T> struct UINode {
  T data;
  float x, y;
  float width, height;
  float border_thickness;
  Uint8 r, g, b, a;

  void render(SDL_Surface *surface) const {
    if (!surface)
      return;

    float half_w = width / 2.0f;
    float half_h = height / 2.0f;

    float inner_half_w = half_w - border_thickness;
    float inner_half_h = half_h - border_thickness;

    if (inner_half_w < 0.0f)
      inner_half_w = 0.0f;
    if (inner_half_h < 0.0f)
      inner_half_h = 0.0f;

    int min_x = (int)(x - half_w - 1);
    int max_x = (int)(x + half_w + 1);
    int min_y = (int)(y - half_h - 1);
    int max_y = (int)(y + half_h + 1);

    if (min_x < 0)
      min_x = 0;
    if (max_x >= surface->w)
      max_x = surface->w - 1;
    if (min_y < 0)
      min_y = 0;
    if (max_y >= surface->h)
      max_y = surface->h - 1;

#ifndef DEBUG
    SDL_PixelFormatDetails const *pixel_details =
        SDL_GetPixelFormatDetails(surface->format);
    Sint32 stride = pixel_details->bytes_per_pixel;
#endif

    for (int cy = min_y; cy <= max_y; ++cy) {
      for (int cx = min_x; cx <= max_x; ++cx) {
        float dx = (float)cx - x;
        float dy = (float)cy - y;

        // Use inclusive bounds for outer, exclusive for inner to create the
        // border
        bool in_outer =
            (dx >= -half_w && dx <= half_w && dy >= -half_h && dy <= half_h);
        bool in_inner = (dx > -inner_half_w && dx < inner_half_w &&
                         dy > -inner_half_h && dy < inner_half_h);

        if (in_outer && !in_inner) {
#ifndef DEBUG
          Uint8 *target_pixel = ((Uint8 *)surface->pixels +
                                 (cy * surface->pitch) + (cx * stride));
          // Note: Hardcoded RGB channel order, might need adaptation for
          // specific formats like BGRA
          target_pixel[0] = r;
          target_pixel[1] = g;
          target_pixel[2] = b;
#else
          SDL_WriteSurfacePixel(surface, cx, cy, r, g, b, a);
#endif
        }
      }
    }
  }
};

int main() {
  assert(SDL_Init(SDL_INIT_VIDEO));

  SDL_Window *window =
      SDL_CreateWindow(PROG_NAME, WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE);
  assert(window);
  log("Created: %s %dx%d\n", PROG_NAME, WIDTH, HEIGHT);

  SDL_Surface *surface = SDL_GetWindowSurface(window);
  assert(surface);
  do_checks(surface);

  bool quit = false;
  while (!quit) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        quit = true;
      }
    }

    surface = SDL_GetWindowSurface(window);
    if (surface) {
      draw(surface);
      do_checks(surface);
      SDL_UpdateWindowSurface(window);
    }
  }

  SDL_DestroyWindow(window);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
  SDL_Quit();
}

void do_checks(SDL_Surface *surface) {
  SDL_PixelFormatDetails const *pixel_details =
      SDL_GetPixelFormatDetails(surface->format);
  assert_eq(pixel_details->bytes_per_pixel, 4, "%d\n",
            pixel_details->bytes_per_pixel);
  // log("PixelFormat: %s : %d\n", SDL_GetPixelFormatName(surface->format),
  //     SDL_BITSPERPIXEL(surface->format));
  // log("Surface: w:%d h:%d\n", surface->w, surface->h);
  return;
}

void draw(SDL_Surface *surface) {
#if 0
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
#endif

  // Demo representation:
  UINode<int> node1 = {1,    200.0f, 200.0f, 160.0f, 80.0f,
                       8.0f, 255,    50,     50,     255};
  node1.render(surface);

  UINode<const char *> node2 = {"test", 450.0f, 300.0f, 120.0f, 60.0f,
                                15.0f,  50,     255,    50,     255};
  node2.render(surface);
}
