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
struct QuadTree;
void do_checks(SDL_Surface *);
void draw_text_widget(SDL_Surface *, int, int, Uint32);
void draw(SDL_Surface *, const std::vector<UINode<Uint32>> &, const QuadTree &,
          float, float, float);

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

struct Rect {
  float x1, y1, x2, y2;
  bool intersects(const Rect &other) const {
    return !(x2 < other.x1 || x1 > other.x2 || y2 < other.y1 || y1 > other.y2);
  }
  bool contains(float x, float y) const {
    return x >= x1 && x <= x2 && y >= y1 && y <= y2;
  }
};

struct QuadTree {
  static const int CAPACITY = 16;
  Rect boundary;
  std::vector<int> indices;
  QuadTree *nw, *ne, *sw, *se;
  const std::vector<UINode<Uint32>> &nodes_ref;

  QuadTree(Rect b, const std::vector<UINode<Uint32>> &nodes)
      : boundary(b), nw(nullptr), ne(nullptr), sw(nullptr), se(nullptr),
        nodes_ref(nodes) {}
  ~QuadTree() {
    delete nw;
    delete ne;
    delete sw;
    delete se;
  }

  void subdivide() {
    float mx = (boundary.x1 + boundary.x2) / 2.0f;
    float my = (boundary.y1 + boundary.y2) / 2.0f;
    nw = new QuadTree({boundary.x1, boundary.y1, mx, my}, nodes_ref);
    ne = new QuadTree({mx, boundary.y1, boundary.x2, my}, nodes_ref);
    sw = new QuadTree({boundary.x1, my, mx, boundary.y2}, nodes_ref);
    se = new QuadTree({mx, my, boundary.x2, boundary.y2}, nodes_ref);
  }

  bool insert(int idx) {
    float x = nodes_ref[idx].x;
    float y = nodes_ref[idx].y;
    if (!boundary.contains(x, y))
      return false;

    if (nw == nullptr) {
      if (indices.size() < CAPACITY) {
        indices.push_back(idx);
        return true;
      }
      subdivide();
      for (int stored_idx : indices) {
        if (!nw->insert(stored_idx))
          if (!ne->insert(stored_idx))
            if (!sw->insert(stored_idx))
              se->insert(stored_idx);
      }
      indices.clear();
    }

    if (nw->insert(idx))
      return true;
    if (ne->insert(idx))
      return true;
    if (sw->insert(idx))
      return true;
    if (se->insert(idx))
      return true;
    return false;
  }

  void query(const Rect &range, std::vector<int> &found) const {
    if (!boundary.intersects(range))
      return;
    if (nw == nullptr) {
      for (int idx : indices) {
        float x = nodes_ref[idx].x;
        float y = nodes_ref[idx].y;
        if (range.contains(x, y)) {
          found.push_back(idx);
        }
      }
      return;
    }
    nw->query(range, found);
    ne->query(range, found);
    sw->query(range, found);
    se->query(range, found);
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
      generate_random_nodes(20000, WIDTH, HEIGHT);

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

  // Populate Quadtree for spatial querying
  float min_x = 0, min_y = 0, max_x = WIDTH, max_y = HEIGHT;
  for (const auto &n : nodes) {
    min_x = n.x < min_x ? n.x : min_x;
    min_y = n.y < min_y ? n.y : min_y;
    max_x = n.x > max_x ? n.x : max_x;
    max_y = n.y > max_y ? n.y : max_y;
  }
  QuadTree qtree({min_x - 100, min_y - 100, max_x + 100, max_y + 100}, nodes);
  for (size_t i = 0; i < nodes.size(); ++i) {
    qtree.insert((int)i);
  }

  bool quit = false;
  float pan_x = 0.0f;
  float pan_y = 0.0f;
  float zoom = 1.0f;
  bool is_dragging = false;
  bool has_selection = false;
  Uint32 selected_data = 0;

  Uint32 frame_count = 0;
  Uint32 last_time = SDL_GetTicks();
  Uint32 current_fps = 0;

  while (!quit) {
    Uint32 frame_start = SDL_GetTicks();

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

          float click_orig_x = (mx / zoom) - pan_x;
          float click_orig_y = (my / zoom) - pan_y;
          std::vector<int> hit_candidates;
          float search_pad = 200.0f; // maximum width of node / zoom
          qtree.query({click_orig_x - search_pad, click_orig_y - search_pad,
                       click_orig_x + search_pad, click_orig_y + search_pad},
                      hit_candidates);

          for (int i : hit_candidates) {
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
              has_selection = true;
              selected_data = nodes[i].data;
              break;
            }
          }

          if (!clicked_on_node) {
            for (auto &n : nodes)
              n.selected = false;
            has_selection = false;
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
      draw(surface, nodes, qtree, pan_x, pan_y, zoom);

      if (has_selection) {
        draw_text_widget(surface, 10, 10, selected_data);
      }

      frame_count++;
      Uint32 current_time = SDL_GetTicks();
      if (current_time > last_time + 1000) {
        current_fps = frame_count;
        frame_count = 0;
        last_time = current_time;
      }

      // Render FPS meter in top right
      draw_text_widget(surface, surface->w - 100, 10, current_fps);

      SDL_UpdateWindowSurface(window);
    }

    Uint32 frame_time = SDL_GetTicks() - frame_start;
    if (frame_time < 16) {
      SDL_Delay(16 - frame_time);
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
          const QuadTree &qtree, float pan_x, float pan_y, float zoom) {
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

  float max_w = 200.0f; // Padding larger than max expected node size
  float orig_x1 = -pan_x - (max_w / zoom);
  float orig_y1 = -pan_y - (max_w / zoom);
  float orig_x2 = orig_x1 + (surface->w / zoom) + 2.0f * (max_w / zoom);
  float orig_y2 = orig_y1 + (surface->h / zoom) + 2.0f * (max_w / zoom);

  std::vector<int> visible;
  qtree.query({orig_x1, orig_y1, orig_x2, orig_y2}, visible);

  static size_t last_visible_count = -1;
  if (visible.size() != last_visible_count) {
    if (visible.size() > 10000) {
      log("Rendering Point Cloud Blob: %zu / %zu nodes (%.1f%%)\n",
          visible.size(), nodes.size(),
          (float)visible.size() / nodes.size() * 100.0f);
    } else {
      log("Rendering Detailed Nodes: %zu / %zu nodes (%.1f%%)\n",
          visible.size(), nodes.size(),
          (float)visible.size() / nodes.size() * 100.0f);
    }
    last_visible_count = visible.size();
  }

  if (visible.size() > 10000) {
#ifndef DEBUG
    SDL_PixelFormatDetails const *pixel_details =
        SDL_GetPixelFormatDetails(surface->format);
    Sint32 stride = pixel_details->bytes_per_pixel;
#endif
    for (int idx : visible) {
      const auto &n = nodes[idx];
      float scaled_x = (n.x + pan_x) * zoom;
      float scaled_y = (n.y + pan_y) * zoom;
      int cx = (int)scaled_x;
      int cy = (int)scaled_y;

      if (cx >= 0 && cx < surface->w && cy >= 0 && cy < surface->h) {
        Uint8 out_r = n.selected ? 255 : n.r;
        Uint8 out_g = n.selected ? 255 : n.g;
        Uint8 out_b = n.selected ? 0 : n.b;
#ifndef DEBUG
        Uint8 *target_pixel =
            ((Uint8 *)surface->pixels + (cy * surface->pitch) + (cx * stride));
        target_pixel[0] = out_r;
        target_pixel[1] = out_g;
        target_pixel[2] = out_b;
#else
        SDL_WriteSurfacePixel(surface, cx, cy, out_r, out_g, out_b, n.a);
#endif
      }
    }
  } else {
    for (int idx : visible) {
      nodes[idx].render(surface, pan_x, pan_y, zoom);
    }
  }
}

void draw_text_widget(SDL_Surface *surface, int x, int y, Uint32 data) {
  const SDL_PixelFormatDetails *format =
      SDL_GetPixelFormatDetails(surface->format);
  Uint32 bg_color = SDL_MapRGB(format, NULL, 50, 50, 50);
  Uint32 fg_color = SDL_MapRGB(format, NULL, 255, 255, 255);

  char buf[32];
  snprintf(buf, sizeof(buf), "%u", data);

  // Create a tight background box behind the number
  int scale = 4;
  int num_chars = 0;
  for (int i = 0; buf[i] != '\0'; ++i)
    num_chars++;

  SDL_Rect bg = {x, y, 20 + num_chars * (4 * scale), 20 + (5 * scale)};
  SDL_FillSurfaceRect(surface, &bg, bg_color);

  const Uint8 font[10][15] = {
      {1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1}, // 0
      {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0}, // 1
      {1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1}, // 2
      {1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1}, // 3
      {1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1}, // 4
      {1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1}, // 5
      {1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1}, // 6
      {1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}, // 7
      {1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1}, // 8
      {1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1}  // 9
  };

  int cursor_x = x + 10;
  int cursor_y = y + 10;

  for (int i = 0; buf[i] != '\0'; ++i) {
    if (buf[i] < '0' || buf[i] > '9')
      continue;
    int d = buf[i] - '0';
    for (int y = 0; y < 5; ++y) {
      for (int x = 0; x < 3; ++x) {
        if (font[d][y * 3 + x]) {
          SDL_Rect pixel = {cursor_x + x * scale, cursor_y + y * scale, scale,
                            scale};
          SDL_FillSurfaceRect(surface, &pixel, fg_color);
        }
      }
    }
    cursor_x += 4 * scale;
  }
}
