#include "CooldownConfig.hpp"

#include <filesystem>
#include <iostream>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: jca_config_gen <mod-package-dir>\n";
    return 2;
  }
  const std::filesystem::path configDir = std::filesystem::path(argv[1]) / "config";
  pl::config::ConfigFile<jca::CooldownConfig> config(
      jca::CooldownConfig{}, configDir / "config.json",
      configDir / "config.schema.json");
  if (!config.save() || !config.writeSchema()) {
    std::cerr << "failed to write config files\n";
    return 1;
  }
  std::cout << "generated " << config.configPath() << '\n';
  return 0;
}
