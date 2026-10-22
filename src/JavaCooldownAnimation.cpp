// Java Combat Animation - LeviLauncher mod entry point
// Author: Chriel
//
// The animations and the cooldown bar live in the bundled packs:
//   resources/minecraft_resource_packs/JavaCombatAnimation_RP  (animations)
//   resources/minecraft_behavior_packs/JavaCombatAnimation_BP  (cooldown bar)
// LeviLauncher installs bundled packs only for mods that load successfully, so
// this tiny library exists to make the launcher load the mod. It draws nothing,
// hooks nothing and has no settings.
#include <pl/Mod.hpp>

namespace {

class JavaCombatAnimation {
public:
  static JavaCombatAnimation &instance() {
    static JavaCombatAnimation mod;
    return mod;
  }

  JavaCombatAnimation() : mSelf(*ll::mod::NativeMod::current()) {}
  [[nodiscard]] ll::mod::NativeMod &getSelf() const { return mSelf; }

  bool load() { return true; }
  bool enable() {
    getSelf().getLogger().info("Java Combat Animation enabled");
    return true;
  }
  bool disable() { return true; }
  bool unload() { return true; }

private:
  ll::mod::NativeMod &mSelf;
};

} // namespace

PL_REGISTER_MOD(JavaCombatAnimation, JavaCombatAnimation::instance())
