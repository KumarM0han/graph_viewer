#include "SDL3/SDL_events.h"
#include "SDL3/SDL_pixels.h"
#include "SDL3/SDL_stdinc.h"
#define DEBUG

#include <SDL3/SDL.h>
#include <assert.h>
#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphAttributes.h>
#include <ogdf/energybased/NodeRespecterLayout.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

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

template <typename T> struct UINode;
void do_checks(SDL_Surface *);
void draw(SDL_Surface *, const std::vector<UINode<Uint32>> &, float, float,
          float);

template <typename T> struct UINode {
  T data;
  float x, y;
  float width, height;
  float border_thickness;
  Uint8 r, g, b, a;
  bool selected = false;

  void render(SDL_Surface *surface, float offset_x, float offset_y,
              float zoom) const {
    if (!surface)
      return;

    float scaled_x = (x + offset_x) * zoom;
    float scaled_y = (y + offset_y) * zoom;
    float scaled_w = width * zoom;
    float scaled_h = height * zoom;
    float scaled_border = border_thickness * zoom;

    float half_w = scaled_w / 2.0f;
    float half_h = scaled_h / 2.0f;

    float inner_half_w = half_w - scaled_border;
    float inner_half_h = half_h - scaled_border;

    if (inner_half_w < 0.0f)
      inner_half_w = 0.0f;
    if (inner_half_h < 0.0f)
      inner_half_h = 0.0f;

    int min_x = (int)(scaled_x - half_w - 1);
    int max_x = (int)(scaled_x + half_w + 1);
    int min_y = (int)(scaled_y - half_h - 1);
    int max_y = (int)(scaled_y + half_h + 1);

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
        float dx = (float)cx - scaled_x;
        float dy = (float)cy - scaled_y;

        // Use inclusive bounds for outer, exclusive for inner to create the
        // border
        bool in_outer =
            (dx >= -half_w && dx <= half_w && dy >= -half_h && dy <= half_h);
        bool in_inner = (dx > -inner_half_w && dx < inner_half_w &&
                         dy > -inner_half_h && dy < inner_half_h);

        if (in_outer && !in_inner) {
          Uint8 out_r = selected ? 255 : r;
          Uint8 out_g = selected ? 255 : g;
          Uint8 out_b = selected ? 0 : b;
#ifndef DEBUG
          Uint8 *target_pixel = ((Uint8 *)surface->pixels +
                                 (cy * surface->pitch) + (cx * stride));
          // Note: Hardcoded RGB channel order, might need adaptation for
          // specific formats like BGRA
          target_pixel[0] = out_r;
          target_pixel[1] = out_g;
          target_pixel[2] = out_b;
#else
          SDL_WriteSurfacePixel(surface, cx, cy, out_r, out_g, out_b, a);
#endif
        }
      }
    }
  }
};

std::vector<UINode<Uint32>> generate_random_nodes(int count, int max_w,
                                                  int max_h) {
  std::vector<UINode<Uint32>> nodes;
  nodes.reserve(count);
  for (int i = 0; i < count; ++i) {
    Uint32 data = (Uint32)rand();
    float x = (float)(rand() % max_w);
    float y = (float)(rand() % max_h);
    float w = 20.0f + (float)(rand() % 80);
    float h = 20.0f + (float)(rand() % 80);
    float t = 2.0f + (float)(rand() % 5);
    Uint8 r = 255;
    Uint8 g = 255;
    Uint8 b = 255;

    nodes.push_back({data, x, y, w, h, t, r, g, b, 255, false});
  }
  return nodes;
}

int main() {
  assert(SDL_Init(SDL_INIT_VIDEO));

  SDL_Window *window =
      SDL_CreateWindow(PROG_NAME, WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE);
  assert(window);
  log("Created: %s %dx%d\n", PROG_NAME, WIDTH, HEIGHT);

  SDL_Surface *surface = SDL_GetWindowSurface(window);
  assert(surface);
  do_checks(surface);

  srand((unsigned int)time(NULL));
  std::vector<UINode<Uint32>> nodes =
      generate_random_nodes(100000, WIDTH, HEIGHT);

  // Apply OGDF overlap removal layout
  ogdf::Graph G;
  ogdf::GraphAttributes GA(G, ogdf::GraphAttributes::nodeGraphics);

  std::vector<ogdf::node> ogdf_nodes;
  for (size_t i = 0; i < nodes.size(); ++i) {
    ogdf::node n = G.newNode();
    GA.x(n) = nodes[i].x;
    GA.y(n) = nodes[i].y;
    GA.width(n) = nodes[i].width + nodes[i].border_thickness * 2.0;
    GA.height(n) = nodes[i].height + nodes[i].border_thickness * 2.0;
    ogdf_nodes.push_back(n);
  }

  ogdf::NodeRespecterLayout layout;
  // Some padding between components
  layout.setMinDistCC(20.0);
  layout.call(GA);

  for (size_t i = 0; i < nodes.size(); ++i) {
    nodes[i].x = GA.x(ogdf_nodes[i]);
    nodes[i].y = GA.y(ogdf_nodes[i]);
  }

  bool quit = false;
  float pan_x = 0.0f;
  float pan_y = 0.0f;
  float zoom = 1.0f;
  bool is_dragging = false;

  while (!quit) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        quit = true;
      } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        if (event.wheel.y > 0)
          zoom *= 1.1f;
        else if (event.wheel.y < 0)
          zoom /= 1.1f;

        if (zoom < 0.1f)
          zoom = 0.1f;
        if (zoom > 10.0f)
          zoom = 10.0f;
      } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
          is_dragging = true;

          float mx = event.button.x;
          float my = event.button.y;
          bool clicked_on_node = false;

          for (int i = (int)nodes.size() - 1; i >= 0; --i) {
            float scaled_x = (nodes[i].x + pan_x) * zoom;
            float scaled_y = (nodes[i].y + pan_y) * zoom;
            float scaled_w = nodes[i].width * zoom;
            float scaled_h = nodes[i].height * zoom;

            float hw = scaled_w / 2.0f;
            float hh = scaled_h / 2.0f;

            if (mx >= scaled_x - hw && mx <= scaled_x + hw &&
                my >= scaled_y - hh && my <= scaled_y + hh) {

              for (auto &n : nodes)
                n.selected = false;
              nodes[i].selected = true;
              clicked_on_node = true;
              break;
            }
          }

          if (!clicked_on_node) {
            for (auto &n : nodes)
              n.selected = false;
          }
        }
      } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (event.button.button == SDL_BUTTON_LEFT)
          is_dragging = false;
      } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
        if (is_dragging) {
          pan_x += event.motion.xrel / zoom;
          pan_y += event.motion.yrel / zoom;
        }
      }
    }

    surface = SDL_GetWindowSurface(window);
    if (surface) {
      SDL_FillSurfaceRect(surface, NULL, 0); // Clear to black
      do_checks(surface);
      draw(surface, nodes, pan_x, pan_y, zoom);
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

void draw(SDL_Surface *surface, const std::vector<UINode<Uint32>> &nodes,
          float pan_x, float pan_y, float zoom) {
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

  for (size_t i = 0; i < nodes.size(); ++i) {
    nodes[i].render(surface, pan_x, pan_y, zoom);
  }
}
