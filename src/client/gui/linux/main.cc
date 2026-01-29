#include "my_application.h"
#include <dlfcn.h>

int main(int argc, char** argv) {
  void* x11_lib = dlopen("libX11.so.6", RTLD_LAZY | RTLD_GLOBAL);

  if (x11_lib) {
    // Define the function signature for XInitThreads
    typedef int (*XInitThreadsFunc)();

    // Look up the symbol
    XInitThreadsFunc x_init_threads =
        (XInitThreadsFunc)dlsym(x11_lib, "XInitThreads");

    // If the function exists, call it.
    if (x_init_threads) {
      x_init_threads();
    }
  }
  g_autoptr(MyApplication) app = my_application_new();
  return g_application_run(G_APPLICATION(app), argc, argv);
}
