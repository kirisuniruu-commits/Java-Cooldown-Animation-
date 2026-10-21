// Java Cooldown Animation - LeviLauncher native mod (Minecraft Bedrock)
// Author: Chriel
//
// Draws a Java Edition style attack cooldown indicator on the HUD.
// Limits: the SDK has no game hooks, so swings are detected from input (left
// click, quick touch tap, keys 1-9 reset like an item switch) and the weapon
// attack speed comes from a preset in the Mod Menu.
#include "CooldownConfig.hpp"
#include "CooldownLogic.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <pl/Input.hpp>
#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>

namespace {

using jca::CooldownConfig;

constexpr const char *kModuleId = "java_cooldown_animation.hud";

bool parseBool(std::string_view v, bool fb) {
  if (v == "true" || v == "1" || v == "on") return true;
  if (v == "false" || v == "0" || v == "off") return false;
  return fb;
}
int parseInt(std::string_view v, int fb) {
  std::string s(v);
  char *end = nullptr;
  errno = 0;
  const long p = std::strtol(s.c_str(), &end, 10);
  if (end == s.c_str() || *end != '\0' || errno == ERANGE) return fb;
  return static_cast<int>(p);
}
double parseDouble(std::string_view v, double fb) {
  std::string s(v);
  char *end = nullptr;
  errno = 0;
  const double p = std::strtod(s.c_str(), &end);
  if (end == s.c_str() || *end != '\0' || errno == ERANGE) return fb;
  return p;
}
std::string numToString(double v) { return std::to_string(v); }

void normalize(CooldownConfig &c) {
  c.version = 1;
  c.weapon = std::clamp(c.weapon, 0, jca::kCustomWeaponIndex);
  c.customSpeed = jca::clampSpeed(c.customSpeed);
  c.style = std::clamp(c.style, 0, 1);
  c.scale = std::clamp(c.scale, jca::kMinScale, jca::kMaxScale);
  c.offsetX = std::clamp(c.offsetX, -jca::kMaxOffset, jca::kMaxOffset);
  c.offsetY = std::clamp(c.offsetY, -jca::kMaxOffset, jca::kMaxOffset);
  c.opacity = std::clamp(c.opacity, 0, 100);
}

std::uint32_t argb(int baseAlpha, int opacityPct, std::uint32_t rgb) {
  const int a = std::clamp(baseAlpha * opacityPct / 100, 0, 255);
  return (static_cast<std::uint32_t>(a) << 24) | (rgb & 0x00FFFFFFu);
}

// Android constants
constexpr int kActionDown = 0, kActionUp = 1, kActionCancel = 3,
              kActionPointerDown = 5, kActionPointerUp = 6;
constexpr int kMouseButtonPrimary = 1;
constexpr int kKeyCode1 = 8, kKeyCode9 = 16;

// Touch-tap heuristics
constexpr double kTapMaxSeconds = 0.25;
constexpr float kTapMaxMovePx = 30.0f;
constexpr float kPlayAreaMinX = 0.35f;
constexpr float kPlayAreaMaxY = 0.85f;

class JavaCooldownAnimation {
public:
  static JavaCooldownAnimation &instance() {
    static JavaCooldownAnimation mod;
    return mod;
  }

  JavaCooldownAnimation() : mSelf(*ll::mod::NativeMod::current()) {}
  [[nodiscard]] ll::mod::NativeMod &getSelf() const { return mSelf; }

  bool load() {
    std::lock_guard lock(mConfigMutex);
    mConfigFile.emplace();
    if (!mConfigFile->load()) {
      getSelf().getLogger().error("Failed to load config");
      mConfigFile.reset();
      return false;
    }
    normalize(mConfigFile->value());
    mConfigFile->save();
    return true;
  }

  bool enable() {
    const auto cfg = snapshot();
    const bool ok =
        pl::modmenu::ModuleBuilder(kModuleId, "Java Cooldown Animation")
            .modId(getSelf().getId())
            .description("Java Edition style attack cooldown indicator.")
            .defaultEnabled(true)
            .onToggle(
                [](std::string_view, bool on) { instance().mModuleOn = on; })
            .config("weapon", "Weapon preset", pl::modmenu::ConfigType::Radio,
                    std::to_string(cfg.weapon), jca::kWeaponMenuOptions)
            .config("customSpeed", "Custom attack speed",
                    pl::modmenu::ConfigType::SliderFloat,
                    numToString(cfg.customSpeed),
                    numToString(jca::kMinAttackSpeed),
                    numToString(jca::kMaxAttackSpeed))
            .config("style", "Style (0 crosshair, 1 hotbar)",
                    pl::modmenu::ConfigType::SliderInt,
                    std::to_string(cfg.style), "0", "1")
            .config("scale", "Scale", pl::modmenu::ConfigType::SliderFloat,
                    numToString(cfg.scale), numToString(jca::kMinScale),
                    numToString(jca::kMaxScale))
            .config("offsetX", "Offset X", pl::modmenu::ConfigType::SliderInt,
                    std::to_string(cfg.offsetX),
                    std::to_string(-jca::kMaxOffset),
                    std::to_string(jca::kMaxOffset))
            .config("offsetY", "Offset Y", pl::modmenu::ConfigType::SliderInt,
                    std::to_string(cfg.offsetY),
                    std::to_string(-jca::kMaxOffset),
                    std::to_string(jca::kMaxOffset))
            .config("opacity", "Opacity", pl::modmenu::ConfigType::SliderInt,
                    std::to_string(cfg.opacity), "0", "100")
            .config("swingOnTap", "Swing on touch tap",
                    pl::modmenu::ConfigType::Toggle,
                    cfg.swingOnTap ? "true" : "false")
            .config("resetOnHotbarKey", "Reset on keys 1-9",
                    pl::modmenu::ConfigType::Toggle,
                    cfg.resetOnHotbarKey ? "true" : "false")
            .onConfigChanged([](std::string_view, std::string_view key,
                                std::string_view value) {
              instance().handleConfigChanged(key, value);
            })
            .registerModule();
    if (!ok) {
      getSelf().getLogger().error("Mod Menu registration failed");
      return false;
    }

    registerInputOnce();
    mModuleOn = true;
    startHudThread();
    getSelf().getLogger().info("Enabled");
    return true;
  }

  bool disable() {
    stopHudThread();
    pl::modmenu::unregisterModule(kModuleId);
    mModuleOn = false;
    return true;
  }

  bool unload() {
    disable();
    std::lock_guard lock(mConfigMutex);
    mConfigFile.reset();
    return true;
  }

private:
  ll::mod::NativeMod &mSelf;
  std::mutex mConfigMutex;
  std::optional<pl::config::ConfigFile<CooldownConfig>> mConfigFile;

  std::atomic_bool mModuleOn{false};
  std::atomic_bool mInputRegistered{false};
  std::atomic_bool mHudRunning{false};
  std::thread mHudThread;
  jca::SwingTracker mTracker;

  struct Finger {
    bool active = false;
    bool eligible = false;
    float x = 0, y = 0;
    std::chrono::steady_clock::time_point down;
  };
  Finger mFingers[16];
  std::mutex mTouchMutex;

  CooldownConfig snapshot() {
    std::lock_guard lock(mConfigMutex);
    CooldownConfig c = mConfigFile ? mConfigFile->value() : CooldownConfig{};
    normalize(c);
    return c;
  }

  void handleConfigChanged(std::string_view key, std::string_view v) {
    std::lock_guard lock(mConfigMutex);
    if (!mConfigFile) return;
    auto &c = mConfigFile->value();
    if (key == "weapon") c.weapon = parseInt(v, c.weapon);
    else if (key == "customSpeed") c.customSpeed = parseDouble(v, c.customSpeed);
    else if (key == "style") c.style = parseInt(v, c.style);
    else if (key == "scale") c.scale = parseDouble(v, c.scale);
    else if (key == "offsetX") c.offsetX = parseInt(v, c.offsetX);
    else if (key == "offsetY") c.offsetY = parseInt(v, c.offsetY);
    else if (key == "opacity") c.opacity = parseInt(v, c.opacity);
    else if (key == "swingOnTap") c.swingOnTap = parseBool(v, c.swingOnTap);
    else if (key == "resetOnHotbarKey")
      c.resetOnHotbarKey = parseBool(v, c.resetOnHotbarKey);
    else return;
    normalize(c);
    mConfigFile->save();
  }

  // Every callback returns false so the game still receives the event.
  // The SDK has no unregister call, so callbacks are installed once.
  void registerInputOnce() {
    if (mInputRegistered.exchange(true)) return;

    pl::input::registerMouseCallback([](const pl::input::MouseEvent &e) {
      auto &self = instance();
      if (self.mModuleOn && e.isDown && (e.button & kMouseButtonPrimary))
        self.mTracker.swing();
      return false;
    });

    pl::input::registerKeyCallback([](const pl::input::KeyEvent &e) {
      auto &self = instance();
      if (self.mModuleOn && e.isKeyDown && e.keyCode >= kKeyCode1 &&
          e.keyCode <= kKeyCode9 && self.snapshot().resetOnHotbarKey)
        self.mTracker.reset();
      return false;
    });

    pl::input::registerTouchCallback([](const pl::input::TouchEvent &e) {
      instance().onTouch(e);
      return false;
    });
  }

  void onTouch(const pl::input::TouchEvent &e) {
    if (!mModuleOn) return;
    const int action = e.action & 0xFF;
    if (e.pointerId < 0 || e.pointerId >= 16) return;
    const auto size = pl::modmenu::getHudSurfaceSize();
    if (size.width <= 0 || size.height <= 0) return;

    std::lock_guard lock(mTouchMutex);
    Finger &f = mFingers[e.pointerId];
    const auto now = std::chrono::steady_clock::now();

    if (action == kActionDown || action == kActionPointerDown) {
      f.active = true;
      f.x = e.x;
      f.y = e.y;
      f.down = now;
      f.eligible = e.x > size.width * kPlayAreaMinX &&
                   e.y < size.height * kPlayAreaMaxY;
    } else if (action == kActionUp || action == kActionPointerUp) {
      if (f.active && f.eligible && snapshot().swingOnTap) {
        const double held = std::chrono::duration<double>(now - f.down).count();
        const float moved = std::hypot(e.x - f.x, e.y - f.y);
        if (held <= kTapMaxSeconds && moved <= kTapMaxMovePx)
          mTracker.swing();
      }
      f.active = false;
    } else if (action == kActionCancel) {
      f.active = false;
    }
  }

  void startHudThread() {
    if (mHudRunning.exchange(true)) return;
    mHudThread = std::thread([this] { hudLoop(); });
  }

  void stopHudThread() {
    if (!mHudRunning.exchange(false)) return;
    if (mHudThread.joinable()) mHudThread.join();
  }

  void hudLoop() {
    bool drawn = false;
    while (mHudRunning) {
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
      const CooldownConfig cfg = snapshot();
      const double speed = jca::speedForPreset(cfg.weapon, cfg.customSpeed);
      const double strength = mTracker.strength(speed);
      const auto size = pl::modmenu::getHudSurfaceSize();

      // Java only shows the indicator while strength < 1.
      if (!mModuleOn || strength >= 1.0 || size.width <= 0 || size.height <= 0) {
        if (drawn) {
          pl::modmenu::submitDrawCommands(kModuleId, {});
          drawn = false;
        }
        continue;
      }
      const auto cmds = buildCommands(cfg, strength, size.width, size.height);
      pl::modmenu::submitDrawCommands(kModuleId, cmds);
      drawn = true;
    }
    pl::modmenu::submitDrawCommands(kModuleId, {});
  }

  static std::vector<pl::modmenu::DrawCommand>
  buildCommands(const CooldownConfig &cfg, double strength, float w, float h) {
    using pl::modmenu::DrawCommand;
    using pl::modmenu::DrawCommandType;
    const float u = std::max(1.0f, h / 270.0f) * static_cast<float>(cfg.scale);
    const int op = cfg.opacity;
    std::vector<DrawCommand> out;

    auto rect = [&](float x, float y, float rw, float rh, std::uint32_t c) {
      DrawCommand d;
      d.type = DrawCommandType::RectFilled;
      d.x = x; d.y = y; d.w = rw; d.h = rh; d.color = c;
      out.push_back(d);
    };

    if (cfg.style == 0) {
      // Java crosshair indicator: 16x4 bar below the crosshair.
      const float x = w / 2.0f - 8.0f * u + cfg.offsetX;
      const float y = h / 2.0f + 9.0f * u + cfg.offsetY;
      rect(x, y, 16 * u, 4 * u, argb(200, op, 0x000000));
      rect(x + u, y + u, 14 * u, 2 * u, argb(200, op, 0x3A3A3A));
      const int fill = jca::crosshairFillPixels(strength);
      if (fill > 0) {
        const float fw = std::min(14.0f, fill * 14.0f / 16.0f);
        rect(x + u, y + u, fw * u, 2 * u, argb(255, op, 0xFFFFFF));
      }
    } else {
      // Java hotbar indicator: 18x18 box, fills bottom-up.
      const float x = w / 2.0f + 100.0f * u + cfg.offsetX;
      const float y = h - 20.0f * u + cfg.offsetY;
      rect(x, y, 18 * u, 18 * u, argb(200, op, 0x000000));
      rect(x + u, y + u, 16 * u, 16 * u, argb(200, op, 0x3A3A3A));
      const int fill = jca::hotbarFillPixels(strength);
      if (fill > 0) {
        const float fh = std::min(16.0f, fill * 16.0f / 18.0f);
        rect(x + u, y + u + (16.0f - fh) * u, 16 * u, fh * u,
             argb(255, op, 0xFFFFFF));
      }
    }
    return out;
  }
};

} // namespace

PL_REGISTER_MOD(JavaCooldownAnimation, JavaCooldownAnimation::instance())
