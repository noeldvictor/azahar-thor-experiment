// Copyright 2026 Azahar Thor fork
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <vector>
#include <fmt/format.h>
#include <nihstro/shader_bytecode.h>
#include "video_core/shader/geometry_expand.h"

namespace Pica::Shader {

using nihstro::DestRegister;
using nihstro::Instruction;
using nihstro::OpCode;
using nihstro::RegisterType;
using nihstro::SourceRegister;
using nihstro::SwizzlePattern;

namespace {

/// Registers that an invocation has written so far. One bit per component.
struct WriteSet {
    u64 temps = 0;   ///< 16 temporaries x 4 components
    u64 outputs = 0; ///< 16 outputs x 4 components
    u8 cc = 0;       ///< conditional code x, y
    u8 addr = 0;     ///< address registers x, y, and the loop counter

    static WriteSet All() {
        return WriteSet{~u64{0}, ~u64{0}, 0x3, 0x7};
    }

    WriteSet Intersect(const WriteSet& other) const {
        return WriteSet{temps & other.temps, outputs & other.outputs,
                        static_cast<u8>(cc & other.cc), static_cast<u8>(addr & other.addr)};
    }

    bool Covers(const WriteSet& needed) const {
        return (needed.temps & ~temps) == 0 && (needed.outputs & ~outputs) == 0 &&
               (needed.cc & ~cc) == 0 && (needed.addr & ~addr) == 0;
    }

    bool operator==(const WriteSet& other) const {
        return temps == other.temps && outputs == other.outputs && cc == other.cc &&
               addr == other.addr;
    }
};

constexpr u32 PROGRAM_END = MAX_PROGRAM_CODE_LENGTH;

u64 RegBits(u32 reg, u8 comp_mask) {
    return static_cast<u64>(comp_mask & 0xF) << (reg * 4);
}

/// Components of the source that the instruction reads, given the destination mask.
template <SwizzlePattern::Selector (SwizzlePattern::*getter)(int) const>
u8 SelectedComponents(const SwizzlePattern& swizzle, u8 dest_lanes) {
    u8 mask = 0;
    for (int i = 0; i < 4; ++i) {
        if ((dest_lanes & (1u << i)) != 0) {
            mask |= static_cast<u8>(1u << static_cast<u32>((swizzle.*getter)(i)));
        }
    }
    return mask;
}

u8 DestLanes(const SwizzlePattern& swizzle) {
    u8 lanes = 0;
    for (int i = 0; i < 4; ++i) {
        if (swizzle.DestComponentEnabled(i)) {
            lanes |= static_cast<u8>(1u << i);
        }
    }
    return lanes;
}

void AddSourceRead(WriteSet& reads, const SourceRegister& source, u8 comps) {
    if (source.GetRegisterType() == RegisterType::Temporary) {
        reads.temps |= RegBits(static_cast<u32>(source.GetIndex()), comps);
    }
}

void AddDestWrite(WriteSet& writes, const DestRegister& dest, u8 lanes) {
    const u32 index = static_cast<u32>(dest.GetIndex());
    switch (dest.GetRegisterType()) {
    case RegisterType::Temporary:
        writes.temps |= RegBits(index, lanes);
        break;
    case RegisterType::Output:
        writes.outputs |= RegBits(index, lanes);
        break;
    default:
        break;
    }
}

u8 ConditionReads(const Instruction::FlowControlType& flow) {
    using Op = Instruction::FlowControlType::Op;
    switch (flow.op) {
    case Op::JustX:
        return 0x1;
    case Op::JustY:
        return 0x2;
    default:
        return 0x3;
    }
}

struct Decoded {
    WriteSet reads;
    WriteSet writes;
    std::vector<u32> successors;
    bool emit = false;
    bool reject = false;
    std::string reason;
};

struct IfRange {
    u32 pc;
    u32 then_begin;
    u32 else_begin;
    u32 after;
};

} // Anonymous namespace

GeometryExpandInfo AnalyzeGeometryProgram(const ProgramCode& code, const SwizzleData& swizzle_data,
                                          u32 main_offset, u32 code_size,
                                          const std::array<u8, 16>& required_output_comps) {
    GeometryExpandInfo info{};
    if (main_offset >= code_size || code_size > PROGRAM_END) {
        info.reason = "entry point outside the program";
        return info;
    }

    // First pass: collect IF ranges and CALL ranges so that edges can be resolved.
    std::vector<IfRange> ifs;
    std::map<u32, std::vector<u32>> returns; // sub end (exclusive) -> return targets
    std::map<u32, u32> sub_begins;           // sub end (exclusive) -> sub begin
    for (u32 pc = 0; pc < code_size; ++pc) {
        Instruction instr;
        instr.hex = code[pc];
        const auto op = instr.opcode.Value().EffectiveOpCode();
        const u32 dest = instr.flow_control.dest_offset;
        const u32 num = instr.flow_control.num_instructions;
        switch (op) {
        case OpCode::Id::IFU:
        case OpCode::Id::IFC:
            ifs.push_back(IfRange{pc, pc + 1, dest, dest + num});
            break;
        case OpCode::Id::CALL:
        case OpCode::Id::CALLC:
        case OpCode::Id::CALLU:
            returns[dest + num].push_back(pc + 1);
            sub_begins[dest + num] = dest;
            break;
        default:
            break;
        }
    }

    // Resolve a fall-through or jump from `from` to `to` against the IF structure: leaving a
    // then-block at its end skips the else-block, and leaving a subroutine at its end returns.
    const auto resolve = [&](u32 from, u32 to, std::vector<u32>& out) {
        for (int guard = 0; guard < 16; ++guard) {
            bool changed = false;
            for (const IfRange& range : ifs) {
                if (to == range.else_begin && from >= range.then_begin && from < range.else_begin &&
                    range.after != range.else_begin) {
                    from = to;
                    to = range.after;
                    changed = true;
                }
            }
            if (!changed) {
                break;
            }
        }
        const auto ret = returns.find(to);
        if (ret != returns.end() && from >= sub_begins[to] && from < to) {
            for (const u32 target : ret->second) {
                out.push_back(target);
            }
            return;
        }
        out.push_back(to);
    };

    const auto decode = [&](u32 pc) -> Decoded {
        Decoded d;
        Instruction instr;
        instr.hex = code[pc];
        const auto op = instr.opcode.Value().EffectiveOpCode();
        const auto opinfo = instr.opcode.Value().GetInfo();
        const bool is_inverted = (opinfo.subtype & OpCode::Info::SrcInversed) != 0;

        const auto arithmetic = [&](u8 src1_lanes_from_dest, bool use_src2, u8 forced_comps) {
            const SwizzlePattern swizzle{swizzle_data[instr.common.operand_desc_id]};
            const u8 lanes = DestLanes(swizzle);
            const u8 comps1 = forced_comps ? forced_comps
                                           : SelectedComponents<&SwizzlePattern::GetSelectorSrc1>(
                                                 swizzle, src1_lanes_from_dest ? lanes : 0xF);
            AddSourceRead(d.reads, instr.common.GetSrc1(is_inverted), comps1);
            if (use_src2) {
                const u8 comps2 =
                    forced_comps ? forced_comps
                                 : SelectedComponents<&SwizzlePattern::GetSelectorSrc2>(
                                       swizzle, src1_lanes_from_dest ? lanes : 0xF);
                AddSourceRead(d.reads, instr.common.GetSrc2(is_inverted), comps2);
            }
            const u32 addr_index = instr.common.address_register_index;
            if (addr_index != 0) {
                d.reads.addr |= static_cast<u8>(1u << (addr_index - 1));
            }
            return lanes;
        };

        switch (op) {
        case OpCode::Id::ADD:
        case OpCode::Id::MUL:
        case OpCode::Id::SGE:
        case OpCode::Id::SLT:
        case OpCode::Id::MAX:
        case OpCode::Id::MIN:
        case OpCode::Id::SGEI:
        case OpCode::Id::SLTI: {
            const u8 lanes = arithmetic(true, true, 0);
            AddDestWrite(d.writes, instr.common.dest.Value(), lanes);
            break;
        }
        case OpCode::Id::MOV:
        case OpCode::Id::FLR: {
            const u8 lanes = arithmetic(true, false, 0);
            AddDestWrite(d.writes, instr.common.dest.Value(), lanes);
            break;
        }
        case OpCode::Id::DP3:
        case OpCode::Id::DP4:
        case OpCode::Id::DPH:
        case OpCode::Id::DST:
        case OpCode::Id::DPHI:
        case OpCode::Id::DSTI:
        case OpCode::Id::LIT: {
            const u8 lanes = arithmetic(false, true, 0);
            AddDestWrite(d.writes, instr.common.dest.Value(), lanes);
            break;
        }
        case OpCode::Id::EX2:
        case OpCode::Id::LG2:
        case OpCode::Id::RCP:
        case OpCode::Id::RSQ: {
            const SwizzlePattern swizzle{swizzle_data[instr.common.operand_desc_id]};
            const u8 comps = static_cast<u8>(1u << static_cast<u32>(swizzle.GetSelectorSrc1(0)));
            const u8 lanes = arithmetic(false, false, comps);
            AddDestWrite(d.writes, instr.common.dest.Value(), lanes);
            break;
        }
        case OpCode::Id::MOVA: {
            const SwizzlePattern swizzle{swizzle_data[instr.common.operand_desc_id]};
            const u8 lanes = static_cast<u8>(DestLanes(swizzle) & 0x3);
            const u8 comps = SelectedComponents<&SwizzlePattern::GetSelectorSrc1>(swizzle, lanes);
            AddSourceRead(d.reads, instr.common.GetSrc1(false), comps);
            if (instr.common.address_register_index != 0) {
                d.reads.addr |= static_cast<u8>(1u << (instr.common.address_register_index - 1));
            }
            d.writes.addr |= lanes;
            break;
        }
        case OpCode::Id::CMP: {
            const SwizzlePattern swizzle{swizzle_data[instr.common.operand_desc_id]};
            const u8 comps1 = SelectedComponents<&SwizzlePattern::GetSelectorSrc1>(swizzle, 0x3);
            const u8 comps2 = SelectedComponents<&SwizzlePattern::GetSelectorSrc2>(swizzle, 0x3);
            AddSourceRead(d.reads, instr.common.GetSrc1(false), comps1);
            AddSourceRead(d.reads, instr.common.GetSrc2(false), comps2);
            if (instr.common.address_register_index != 0) {
                d.reads.addr |= static_cast<u8>(1u << (instr.common.address_register_index - 1));
            }
            d.writes.cc = 0x3;
            break;
        }
        case OpCode::Id::MAD:
        case OpCode::Id::MADI: {
            const SwizzlePattern swizzle{swizzle_data[instr.mad.operand_desc_id]};
            const u8 lanes = DestLanes(swizzle);
            AddSourceRead(d.reads, instr.mad.GetSrc1(is_inverted),
                          SelectedComponents<&SwizzlePattern::GetSelectorSrc1>(swizzle, lanes));
            AddSourceRead(d.reads, instr.mad.GetSrc2(is_inverted),
                          SelectedComponents<&SwizzlePattern::GetSelectorSrc2>(swizzle, lanes));
            AddSourceRead(d.reads, instr.mad.GetSrc3(is_inverted),
                          SelectedComponents<&SwizzlePattern::GetSelectorSrc3>(swizzle, lanes));
            if (instr.mad.address_register_index != 0) {
                d.reads.addr |= static_cast<u8>(1u << (instr.mad.address_register_index - 1));
            }
            AddDestWrite(d.writes, instr.mad.dest.Value(), lanes);
            break;
        }
        case OpCode::Id::NOP:
        case OpCode::Id::SETEMIT:
            break;
        case OpCode::Id::EMIT:
            d.emit = true;
            for (u32 reg = 0; reg < 16; ++reg) {
                d.reads.outputs |= RegBits(reg, required_output_comps[reg]);
            }
            break;
        case OpCode::Id::END:
            return d;
        case OpCode::Id::IFC:
        case OpCode::Id::JMPC:
        case OpCode::Id::CALLC:
            d.reads.cc = ConditionReads(instr.flow_control);
            break;
        case OpCode::Id::IFU:
        case OpCode::Id::JMPU:
        case OpCode::Id::CALLU:
        case OpCode::Id::CALL:
            break;
        case OpCode::Id::LOOP:
        case OpCode::Id::BREAK:
        case OpCode::Id::BREAKC:
            d.reject = true;
            d.reason = fmt::format("loop instruction at {}", pc);
            return d;
        default:
            d.reject = true;
            d.reason = fmt::format("unhandled opcode 0x{:02X} at {}", static_cast<u32>(op), pc);
            return d;
        }

        // Successors.
        const u32 dest = instr.flow_control.dest_offset;
        const u32 num = instr.flow_control.num_instructions;
        switch (op) {
        case OpCode::Id::JMPC:
        case OpCode::Id::JMPU:
            resolve(pc, dest, d.successors);
            resolve(pc, pc + 1, d.successors);
            break;
        case OpCode::Id::IFC:
        case OpCode::Id::IFU:
            // The then-block starts after the IF. An empty then-block arrives at the else-block
            // start, which the PICA skips to the merge point.
            if (pc + 1 == dest) {
                resolve(dest, dest + num, d.successors);
            } else {
                d.successors.push_back(pc + 1);
            }
            d.successors.push_back(dest);
            break;
        case OpCode::Id::CALL:
            d.successors.push_back(dest);
            break;
        case OpCode::Id::CALLC:
        case OpCode::Id::CALLU:
            d.successors.push_back(dest);
            resolve(pc, pc + 1, d.successors);
            break;
        default:
            resolve(pc, pc + 1, d.successors);
            break;
        }
        return d;
    };

    std::vector<WriteSet> in(code_size + 1, WriteSet::All());
    std::vector<u8> visited(code_size + 1, 0);
    std::vector<u32> worklist;
    in[main_offset] = WriteSet{};
    visited[main_offset] = 1;
    worklist.push_back(main_offset);

    u32 emits = 0;
    std::vector<u8> emit_counted(code_size + 1, 0);
    u32 steps = 0;
    while (!worklist.empty()) {
        if (++steps > 200000) {
            info.reason = "dataflow did not converge";
            return info;
        }
        const u32 pc = worklist.back();
        worklist.pop_back();
        if (pc >= code_size) {
            info.reason = fmt::format("control flow leaves the program at {}", pc);
            return info;
        }
        const Decoded d = decode(pc);
        if (d.reject) {
            info.reason = d.reason;
            return info;
        }
        if (!in[pc].Covers(d.reads)) {
            info.reason = fmt::format("reads a register before writing it at {}", pc);
            return info;
        }
        if (d.emit && !emit_counted[pc]) {
            emit_counted[pc] = 1;
            ++emits;
        }
        WriteSet out = in[pc];
        out.temps |= d.writes.temps;
        out.outputs |= d.writes.outputs;
        out.cc |= d.writes.cc;
        out.addr |= d.writes.addr;
        for (const u32 next : d.successors) {
            if (next > code_size) {
                info.reason = fmt::format("jump target {} outside the program", next);
                return info;
            }
            const WriteSet merged = visited[next] ? in[next].Intersect(out) : out;
            if (!visited[next] || !(merged == in[next])) {
                in[next] = merged;
                visited[next] = 1;
                worklist.push_back(next);
            }
        }
    }

    if (emits == 0) {
        info.reason = "no reachable emit";
        return info;
    }
    info.ok = true;
    info.max_vertices = emits * 3;
    return info;
}

} // namespace Pica::Shader
