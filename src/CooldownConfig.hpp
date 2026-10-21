#pragma once
#include <optional>
#include <string_view>

#include <pl/Config.hpp>

namespace jca {

struct CooldownConfig {
  int version = 1;
  int weapon = 1;
  double customSpeed = 4.0;
  int style = 0;
  double scale = 1.0;
  int offsetX = 0;
  int offsetY = 0;
  int opacity = 100;
  bool swingOnTap = true;
  bool resetOnHotbarKey = true;
};

inline constexpr double kMinScale = 0.5;
inline constexpr double kMaxScale = 4.0;
inline constexpr int kMaxOffset = 600;

} // namespace jca

namespace pl::config {

template <> struct Schema<jca::CooldownConfig> {
  static constexpr std::string_view title = "Java Cooldown Animation";
  static constexpr std::string_view description =
      "Java Edition style attack cooldown indicator.";

  static constexpr FieldSchema field(std::string_view name) {
    if (name == "version")
      return {"Version", "Config schema version.", std::nullopt, std::nullopt,
              true};
    if (name == "weapon")
      return {"Weapon preset", "Index of the Java weapon attack speed (0-10).",
              0, 10, false};
    if (name == "customSpeed")
      return {"Custom attack speed", "Used when the preset is Custom.", 0.5,
              10.0, false};
    if (name == "style")
      return {"Indicator style", "0 = crosshair bar, 1 = hotbar-side box.", 0,
              1, false};
    if (name == "scale")
      return {"Scale", "HUD size multiplier.", jca::kMinScale, jca::kMaxScale,
              false};
    if (name == "offsetX")
      return {"Offset X", "Horizontal pixel offset.", -jca::kMaxOffset,
              jca::kMaxOffset, false};
    if (name == "offsetY")
      return {"Offset Y", "Vertical pixel offset.", -jca::kMaxOffset,
              jca::kMaxOffset, false};
    if (name == "opacity")
      return {"Opacity", "Indicator opacity percentage.", 0, 100, false};
    if (name == "swingOnTap")
      return {"Swing on tap", "Touch taps restart the cooldown.", std::nullopt,
              std::nullopt, false};
    if (name == "resetOnHotbarKey")
      return {"Reset on hotbar key",
              "Keyboard keys 1-9 restart the cooldown (item switch).",
              std::nullopt, std::nullopt, false};
    return {};
  }
};

} // namespace pl::config
