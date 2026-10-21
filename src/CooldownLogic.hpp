#pragma once
// CooldownLogic.hpp - dependency-free port of Java Edition's attack cooldown
// math (period = 1 / attackSpeed * 20 ticks; strength = clamp(ticks/period)).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace jca {

struct Weapon {
  const char *name;
  double attackSpeed;
};

// Order MUST match kWeaponMenuOptions below.
inline constexpr Weapon kWeapons[] = {
    {"Fist", 4.0},
    {"Sword", 1.6},
    {"Axe (wood/stone)", 0.8},
    {"Axe (iron)", 0.9},
    {"Axe (gold/diamond/netherite)", 1.0},
    {"Pickaxe", 1.2},
    {"Shovel", 1.0},
    {"Hoe (diamond/netherite)", 4.0},
    {"Trident", 1.1},
    {"Mace", 0.6},
};
inline constexpr int kWeaponCount = sizeof(kWeapons) / sizeof(kWeapons[0]);
inline constexpr int kCustomWeaponIndex = kWeaponCount;

inline constexpr const char *kWeaponMenuOptions =
    "Fist,Sword,Axe (wood/stone),Axe (iron),Axe (gold/diamond/netherite),"
    "Pickaxe,Shovel,Hoe (diamond/netherite),Trident,Mace,Custom";

inline constexpr double kTicksPerSecond = 20.0;
inline constexpr double kMinAttackSpeed = 0.5;
inline constexpr double kMaxAttackSpeed = 10.0;

inline double clampSpeed(double s) {
  return std::clamp(s, kMinAttackSpeed, kMaxAttackSpeed);
}

inline double speedForPreset(int preset, double customSpeed) {
  if (preset >= 0 && preset < kWeaponCount) return kWeapons[preset].attackSpeed;
  return clampSpeed(customSpeed);
}

inline double cooldownTicks(double attackSpeed) {
  return (1.0 / clampSpeed(attackSpeed)) * kTicksPerSecond;
}

inline double attackStrength(double ticksSinceSwing, double attackSpeed) {
  return std::clamp(ticksSinceSwing / cooldownTicks(attackSpeed), 0.0, 1.0);
}

// Java crosshair indicator: (int)(strength * 17), bar is 16 px.
inline int crosshairFillPixels(double strength) {
  return std::clamp(static_cast<int>(strength * 17.0), 0, 16);
}
// Java hotbar indicator: (int)(strength * 19), box is 18 px.
inline int hotbarFillPixels(double strength) {
  return std::clamp(static_cast<int>(strength * 19.0), 0, 18);
}

class SwingTracker {
public:
  using Clock = std::chrono::steady_clock;

  void swing() { mLastSwingNs.store(nowNs(), std::memory_order_relaxed); }
  void reset() { swing(); }
  void clear() { mLastSwingNs.store(kNever, std::memory_order_relaxed); }

  double ticksSinceSwing() const {
    const auto last = mLastSwingNs.load(std::memory_order_relaxed);
    if (last == kNever) return 1.0e9;
    const double seconds = static_cast<double>(nowNs() - last) / 1.0e9;
    return std::max(0.0, seconds) * kTicksPerSecond;
  }

  double strength(double attackSpeed) const {
    return attackStrength(ticksSinceSwing(), attackSpeed);
  }

private:
  static constexpr std::int64_t kNever = INT64_MIN;
  static std::int64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               Clock::now().time_since_epoch())
        .count();
  }
  std::atomic<std::int64_t> mLastSwingNs{kNever};
};

} // namespace jca
