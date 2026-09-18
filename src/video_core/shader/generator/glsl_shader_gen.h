// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

// High precision may or may not be supported in GLES3. If it isn't, use medium precision instead.
static constexpr char fragment_shader_precision_OES[] = R"(
#if GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp int;
precision highp float;
precision highp samplerBuffer;
precision highp uimage2D;
#else
precision mediump int;
precision mediump float;
precision mediump samplerBuffer;
precision mediump uimage2D;
#endif // GL_FRAGMENT_PRECISION_HIGH
#endif
)";

namespace Pica {
struct ShaderSetup;
}

namespace Pica::Shader::Generator {
struct PicaVSConfig;
struct PicaGSExpandVSConfig;
struct ExtraVSConfig;
struct PicaFixedGSConfig;
struct ExtraFixedGSConfig;
} // namespace Pica::Shader::Generator

namespace Pica::Shader::Generator::GLSL {

/**
 * Generates the GLSL vertex shader program source code that accepts vertices from software shader
 * and directly passes them to the fragment shader.
 * @returns String of the shader source code
 */
std::string GenerateTrivialVertexShader(bool use_clip_planes, bool separable_shader);

/**
 * Generates the GLSL vertex shader program source code for the given VS program
 * @returns String of the shader source code; empty on failure
 */
std::string GenerateVertexShader(const Pica::ShaderSetup& setup, const PicaVSConfig& config,
                                 const ExtraVSConfig& extra);

/**
 * Generates a GLSL vertex shader that runs the given VS program and then the given point-mode
 * geometry program for one PICA input vertex per instance. Host vertex index v keeps corner
 * v % 3 of the (v / 3)-th triangle the geometry program emits. Unused host vertices are placed
 * outside the clip volume.
 * @returns String of the shader source code; empty on failure
 */
std::string GenerateGeometryExpandedVertexShader(const Pica::ShaderSetup& vs_setup,
                                                 const Pica::ShaderSetup& gs_setup,
                                                 const PicaGSExpandVSConfig& config,
                                                 const ExtraVSConfig& extra);

/**
 * Generates the GLSL fixed geometry shader program source code for non-GS PICA pipeline
 * @returns String of the shader source code
 */
std::string GenerateFixedGeometryShader(const PicaFixedGSConfig& config,
                                        const ExtraFixedGSConfig& extra_config);

} // namespace Pica::Shader::Generator::GLSL
