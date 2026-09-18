// Copyright 2026 Azahar Thor fork
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include "common/common_types.h"
#include "video_core/pica/shader_setup.h"

namespace Pica::Shader {

/**
 * Result of the static check that decides whether a point-mode PICA geometry program can run
 * inside an instanced host vertex shader. The host shader starts every invocation with clean
 * registers. The PICA geometry unit keeps its registers between invocations. A program that reads
 * a register before it writes it in the same invocation therefore depends on state the host
 * shader does not have, and it stays on the software path.
 */
struct GeometryExpandInfo {
    bool ok = false;
    /// Upper bound of vertices one invocation emits: three per reachable EMIT instruction.
    u32 max_vertices = 0;
    /// Reason for rejection, for the log.
    std::string reason;
};

/**
 * Checks a geometry program with a forward must-write dataflow over its control flow graph.
 * @param code Program code shared by all geometry programs of the title.
 * @param swizzle Swizzle patterns of the program.
 * @param main_offset Entry point of the program.
 * @param code_size Number of valid words in the program code.
 * @param required_output_comps For each output register, a mask of components the output map
 *        reads (bit 0 = x). Every EMIT must find these components written in the invocation.
 */
GeometryExpandInfo AnalyzeGeometryProgram(const ProgramCode& code, const SwizzleData& swizzle,
                                          u32 main_offset, u32 code_size,
                                          const std::array<u8, 16>& required_output_comps);

} // namespace Pica::Shader
