#pragma once

#include "core/FileSystem.h"

namespace Paths {
    inline const fs::path dataDir = "data";

    inline const fs::path overlaysDir = dataDir / "overlays";
    inline const fs::path romsDir = dataDir / "roms";
    inline const fs::path userDir = dataDir / "user";
    inline const fs::path biosDir = dataDir / "bios";
    inline const fs::path devDir = dataDir / "dev";

    inline const fs::path biosRomFile = biosDir / "System.bin";
    inline const fs::path biosRomFastFile = biosDir / "FastBoot.bin";
    inline const fs::path biosRomSkipFile = biosDir / "SkipBoot.bin";
    // Startup logo border drawn as continuous lines instead of dashes, and only once instead of
    // twice - see tools/patch_bios_border.py. Intended for laser output, where the dashes read
    // as broken geometry.
    inline const fs::path biosRomLaserFile = biosDir / "LaserBoot.bin";

    inline const fs::path optionsFile = userDir / "options.txt";
    inline const fs::path imguiIniFile = userDir / "imgui.ini";
} // namespace Paths
