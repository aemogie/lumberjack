#include "wlw.c"
#include <stdio.h>

void
registry_cb (const struct wlw_untyped *msg)
{
  (void) msg;
}

int
main (void)
{
  printf ("start\n");
  wlw_open ();
  wlw_register (wl_registry_i, wl_registry_e_global_op, registry_cb);
  printf ("end\n");
  return 0;
}
