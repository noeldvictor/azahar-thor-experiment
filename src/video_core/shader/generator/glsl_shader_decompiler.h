// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>
#include <string_view>
#include "video_core/pica/shader_setup.h"

namespace Pica::Shader::Generator::GLSL {

using RegGetter = std::function<std::string(u32)>;

struct DecompileOptions {
    /// Emulate PICA multiplication of infinity by zero.
    bool sanitize_mul = false;
    /// Prefix for every global identifier the program defines: registers, subroutines, helpers,
    /// and the entry point. Two programs with different prefixes can share one shader.
    std::string_view prefix = "";
    /// Instance name of the uniform block that holds the program's float, int, and bool uniforms.
    std::string_view uniform_block = "uniforms";
    /// Translate EMIT into `<prefix>emit()` and SETEMIT into `<prefix>setemit(id, prim, winding)`.
    /// The caller defines both functions before the program source.
    bool geometry = false;
};

std::string DecompileProgram(const Pica::ProgramCode& program_code,
                             const Pica::SwizzleData& swizzle_data, u32 main_offset,
                             const RegGetter& inputreg_getter, const RegGetter& outputreg_getter,
                             const DecompileOptions& options);

std::string DecompileProgram(const Pica::ProgramCode& program_code,
                             const Pica::SwizzleData& swizzle_data, u32 main_offset,
                             const RegGetter& inputreg_getter, const RegGetter& outputreg_getter,
                             bool sanitize_mul);

} // namespace Pica::Shader::Generator::GLSL
