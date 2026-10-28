#include <rex/cvar.h>
#include <rex/thread.h>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <thread>

using namespace std::chrono_literals;
static double cpu_ms() {
  timespec t{};
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1000000.0;
}

int main() {
  for (bool precise : {false, true}) {
    if (!rex::cvar::SetFlagByName("posix_precise_wait_sleep", precise ? "true" : "false"))
      return 1;
    auto first = rex::thread::Event::CreateAutoResetEvent(false);
    auto second = rex::thread::Event::CreateAutoResetEvent(false);
    rex::thread::WaitHandle* handles[] = {first.get(), second.get()};
    for (bool alertable : {false, true}) {
      const auto start = std::chrono::steady_clock::now();
      const auto cpu_start = cpu_ms();
      for (int i = 0; i < 200; ++i) {
        if (rex::thread::WaitAny(handles, 2, alertable, 1ms).first !=
            rex::thread::WaitResult::kTimeout) return 2;
      }
      const double cpu = cpu_ms() - cpu_start;
      const auto wall = std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - start).count();
      if (wall < 190.0 || wall > 2000.0) return 3;
      second->Set();
      const auto ready = rex::thread::WaitAny(handles, 2, alertable, 20ms);
      if (ready.first != rex::thread::WaitResult::kSuccess || ready.second != 1) return 4;
      if (rex::thread::WaitAny(handles, 2, alertable, 1ms).first !=
          rex::thread::WaitResult::kTimeout) return 5;
      std::thread signaler([&] { std::this_thread::sleep_for(5ms); first->Set(); });
      const auto delayed = rex::thread::WaitAny(handles, 2, alertable, 100ms);
      signaler.join();
      if (delayed.first != rex::thread::WaitResult::kSuccess || delayed.second != 0) return 6;
      std::printf("precise=%d alertable=%d timeout_wall_ms=%.2f cpu_ms=%.2f event_delivery=PASS\n",
                  precise, alertable, wall, cpu);
    }
  }
  return 0;
}
