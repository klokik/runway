
#include <cassert>
#include <iostream>

extern "C" {
#include <wayland-server.h>
#include <wlr/backend.h>
}


struct rwc_server {
  struct wl_display *wl_display;
  struct wl_event_loop *wl_event_loop;

  struct wlr_backend *backend;
};

int main() {
  std::cout << "Hello, Runway compositor!" << std::endl;

  rwc_server server;

  server.wl_display = wl_display_create();
  assert(server.wl_display);
  server.wl_event_loop = wl_display_get_event_loop(server.wl_display);
  assert(server.wl_event_loop);

  server.backend = wlr_backend_autocreate(server.wl_display, nullptr);
  assert(server.backend);

  if (!wlr_backend_start(server.backend)) {
    std::cerr << "Failed to start backend" << std::endl;
    wl_display_destroy(server.wl_display);
    return 1;
  }

  wl_display_run(server.wl_display);
  wl_display_destroy(server.wl_display);

  return 0;
}

