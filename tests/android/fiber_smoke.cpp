#include <rex/thread/fiber.h>
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

using rex::thread::Fiber;
struct State { Fiber* root; Fiber* worker; int steps = 0; bool failed = false; };
static void worker(void* argument) {
  auto& s = *static_cast<State*>(argument);
  volatile uint64_t stack_canary = 0x123456789abcdef0ull;
  double accumulator = 1.25;
  for (;;) {
    if (Fiber::Current() != s.worker || stack_canary != 0x123456789abcdef0ull || accumulator != 1.25 + s.steps)
      s.failed = true;
    ++s.steps;
    accumulator += 1.0;
    Fiber::SwitchTo(s.root);
  }
}
static bool exercise() {
  State s{};
  s.root = Fiber::ConvertCurrentThread();
  s.worker = Fiber::Create(128 * 1024, worker, &s);
  for (int i = 1; i <= 10000; ++i) {
    Fiber::SwitchTo(s.worker);
    if (s.steps != i || Fiber::Current() != s.root) s.failed = true;
  }
  s.worker->Destroy();
  s.root->Destroy();
  return !s.failed && Fiber::Current() == nullptr;
}
int main() {
  std::atomic<int> passed{0};
  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) threads.emplace_back([&] { if (exercise()) ++passed; });
  for (auto& thread : threads) thread.join();
  std::printf("fiber smoke: %d/4 threads passed, 40000 round trips\n", passed.load());
  return passed == 4 ? 0 : 1;
}
