#include "CooldownLogic.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <thread>

static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

int main() {
  assert(near(jca::cooldownTicks(1.6), 12.5));
  assert(near(jca::cooldownTicks(1.0), 20.0));
  assert(near(jca::cooldownTicks(4.0), 5.0));
  assert(std::fabs(jca::cooldownTicks(0.6) - 33.3333333) < 1e-3);
  assert(near(jca::attackStrength(0, 1.6), 0.0));
  assert(near(jca::attackStrength(6.25, 1.6), 0.5));
  assert(near(jca::attackStrength(12.5, 1.6), 1.0));
  assert(near(jca::attackStrength(999, 1.6), 1.0));
  assert(jca::crosshairFillPixels(0.0) == 0);
  assert(jca::crosshairFillPixels(0.5) == 8);
  assert(jca::crosshairFillPixels(1.0) == 16);
  assert(jca::hotbarFillPixels(0.5) == 9);
  assert(jca::hotbarFillPixels(1.0) == 18);
  assert(near(jca::speedForPreset(1, 3.0), 1.6));
  assert(near(jca::speedForPreset(jca::kCustomWeaponIndex, 3.0), 3.0));
  assert(near(jca::speedForPreset(jca::kCustomWeaponIndex, 99.0), 10.0));
  jca::SwingTracker t;
  assert(near(t.strength(1.6), 1.0));
  t.swing();
  assert(t.strength(1.6) < 0.2);
  std::this_thread::sleep_for(std::chrono::milliseconds(700));
  assert(t.strength(1.6) >= 1.0 - 1e-9);
  t.reset();
  assert(t.strength(1.6) < 0.2);
  t.clear();
  assert(near(t.strength(1.6), 1.0));
  std::puts("CooldownLogic: all tests passed");
  return 0;
}
