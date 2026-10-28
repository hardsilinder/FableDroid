#include <dlfcn.h>
#include <cstdio>
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  void* game = dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL);
  if (!game) { std::fprintf(stderr, "game dlopen failed: %s\n", dlerror()); return 1; }
  if (!dlsym(game, "SDL_main")) { std::fprintf(stderr, "SDL_main missing\n"); return 1; }
  void* graphics = dlopen("librexgpu-xenos.so", RTLD_NOW | RTLD_GLOBAL);
  if (!graphics) { std::fprintf(stderr, "GPU plugin dlopen failed: %s\n", dlerror()); return 1; }
  std::puts("Android dynamic linker: game, SDL_main, and Xenos plugin loaded successfully");
  return 0;
}
