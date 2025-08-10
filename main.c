#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

struct wl_compositor* compositor;
// struct xdg_wm_base* wm_base;
struct wl_seat* seat;
struct wl_shm* shm;

void registry_global_handler(
    void* data,
    struct wl_registry* registry,
    uint32_t name,
    const char* interface,
    uint32_t version
) {
  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    // } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    //   wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
  } else if (strcmp(interface, wl_seat_interface.name) == 0) {
    seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
  } else if (strcmp(interface, wl_shm_interface.name) == 0) {
    shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
  }
}
void registry_global_remove_handler(
    void* data, struct wl_registry* registry, uint32_t name
) {}

const struct wl_registry_listener registry_listener = {
    .global = registry_global_handler,
    .global_remove = registry_global_remove_handler,
};

int main(int argc, char** argv) {
  printf("hello, lumberjack\n");

  struct wl_display* display = wl_display_connect(NULL);
  assert(display);

  struct wl_registry* registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);

  wl_display_roundtrip(display);

  printf("compositor: %p\n", compositor);
  // printf("wm_base: %p\n", wm_base);
  printf("seat: %p\n", seat);
  printf("shm: %p\n", shm);

  struct wl_surface* surface = wl_compositor_create_surface(compositor);

  wl_display_disconnect(display);
}
