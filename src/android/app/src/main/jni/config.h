// Copyright 2014-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "common/settings.h"

class INIReader;

class Config {
private:
    std::unique_ptr<INIReader> android_config;
    std::string android_config_loc;

    bool LoadINI(const std::string& default_contents = "", bool retry = true);
    void ReadValues();

    /// True while ReadValues() is overlaying a per-title file: keys that the file does not
    /// contain leave the already-loaded global value untouched instead of resetting to defaults.
    bool sparse_overlay = false;

    /// Whether the currently loaded INI contains group/name at all.
    bool Has(const std::string& group, const std::string& name) const;

public:
    Config();
    ~Config();

    void Reload();

    /// Overlays GameSettings/<title_id>.ini from the user directory onto the loaded global
    /// settings for the current session. Returns false when no such file exists or it fails to
    /// parse; global settings are left as they were.
    bool ApplyGameSettings(u64 title_id);

    /// Load ShaderRules/<title id>.txt into Settings::values::shader_shading_rules. The ini
    /// cannot hold the list because inih caps a line at 200 characters.
    void ApplyShaderRules(u64 title_id);

    /// Path of the per-title settings file, without checking that it exists.
    static std::string GetGameSettingsPath(u64 title_id);

private:
    /**
     * Applies a value read from the android_config to a Setting.
     *
     * @param group The name of the INI group
     * @param setting The yuzu setting to modify
     */
    template <typename Type, bool ranged>
    void ReadSetting(const std::string& group, Settings::Setting<Type, ranged>& setting);
};
