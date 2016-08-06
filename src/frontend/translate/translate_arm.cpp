/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * This software may be used and distributed according to the terms of the GNU
 * General Public License version 2 or any later version.
 */

#include "common/assert.h"
#include "frontend/arm_types.h"
#include "frontend/decoder/arm.h"
#include "frontend/decoder/vfp2.h"
#include "frontend/ir/ir.h"
#include "frontend/translate/translate.h"
#include "frontend/translate/translate_arm/translate_arm.h"

namespace Dynarmic {
namespace Arm {

IR::Block TranslateArm(LocationDescriptor descriptor, MemoryRead32FuncType memory_read_32) {
    ArmTranslatorVisitor visitor{descriptor};

    bool should_continue = true;
    while (should_continue && visitor.cond_state == ConditionalState::None) {
        const u32 arm_pc = visitor.ir.current_location.PC();
        const u32 arm_instruction = (*memory_read_32)(arm_pc);

        if (auto vfp_decoder = DecodeVFP2<ArmTranslatorVisitor>(arm_instruction)) {
            should_continue = vfp_decoder->call(visitor, arm_instruction);
        } else if (auto decoder = DecodeArm<ArmTranslatorVisitor>(arm_instruction)) {
            should_continue = decoder->call(visitor, arm_instruction);
        } else {
            should_continue = visitor.arm_UDF();
        }

        if (visitor.cond_state == ConditionalState::Break) {
            break;
        }

        visitor.ir.current_location = visitor.ir.current_location.AdvancePC(4);
        visitor.ir.block.cycle_count++;
    }

    if (visitor.cond_state == ConditionalState::Translating) {
        if (should_continue) {
            visitor.ir.SetTerm(IR::Term::LinkBlock{visitor.ir.current_location});
        }
        visitor.ir.block.cond_failed = { visitor.ir.current_location };
    }

    return std::move(visitor.ir.block);
}

bool ArmTranslatorVisitor::ConditionPassed(Cond cond) {
    ASSERT_MSG(cond_state != ConditionalState::Translating,
               "In the current impl, ConditionPassed should never be called again once a non-AL cond is hit. "
                       "(i.e.: one and only one conditional instruction per block)");
    ASSERT_MSG(cond_state != ConditionalState::Break,
               "This should never happen. We requested a break but that wasn't honored.");

    if (cond == Cond::AL) {
        // Everything is fine with the world
        return true;
    }

    if (cond == Cond::NV) {
        // NV conditional is obsolete, but still seems to be used in some places!
        return false;
    }

    // non-AL cond

    if (!ir.block.instructions.IsEmpty()) {
        // We've already emitted instructions. Quit for now, we'll make a new block here later.
        cond_state = ConditionalState::Break;
        ir.SetTerm(IR::Term::LinkBlock{ir.current_location});
        return false;
    }

    // We've not emitted instructions yet.
    // We'll emit one instruction, and set the block-entry conditional appropriately.

    cond_state = ConditionalState::Translating;
    ir.block.cond = cond;
    return true;
}

bool ArmTranslatorVisitor::InterpretThisInstruction() {
    ir.SetTerm(IR::Term::Interpret(ir.current_location));
    return false;
}

bool ArmTranslatorVisitor::UnpredictableInstruction() {
    ASSERT_MSG(false, "UNPREDICTABLE");
    return false;
}

bool ArmTranslatorVisitor::LinkToNextInstruction() {
    auto next_location = ir.current_location.AdvancePC(4);
    ir.SetTerm(IR::Term::LinkBlock{next_location});
    return false;
}

IREmitter::ResultAndCarry ArmTranslatorVisitor::EmitImmShift(IR::Value value, ShiftType type, Imm5 imm5, IR::Value carry_in) {
    IREmitter::ResultAndCarry result_and_carry;
    switch (type)
    {
        case ShiftType::LSL:
            result_and_carry = ir.LogicalShiftLeft(value, ir.Imm8(imm5), carry_in);
            break;
        case ShiftType::LSR:
            imm5 = imm5 ? imm5 : 32;
            result_and_carry = ir.LogicalShiftRight(value, ir.Imm8(imm5), carry_in);
            break;
        case ShiftType::ASR:
            imm5 = imm5 ? imm5 : 32;
            result_and_carry = ir.ArithmeticShiftRight(value, ir.Imm8(imm5), carry_in);
            break;
        case ShiftType::ROR:
            if (imm5)
                result_and_carry = ir.RotateRight(value, ir.Imm8(imm5), carry_in);
            else
                result_and_carry = ir.RotateRightExtended(value, carry_in);
            break;
    }
    return result_and_carry;
}

IREmitter::ResultAndCarry ArmTranslatorVisitor::EmitRegShift(IR::Value value, ShiftType type, IR::Value amount, IR::Value carry_in) {
    IREmitter::ResultAndCarry result_and_carry;
    switch (type)
    {
        case ShiftType::LSL:
            result_and_carry = ir.LogicalShiftLeft(value, amount, carry_in);
            break;
        case ShiftType::LSR:
            result_and_carry = ir.LogicalShiftRight(value, amount, carry_in);
            break;
        case ShiftType::ASR:
            result_and_carry = ir.ArithmeticShiftRight(value, amount, carry_in);
            break;
        case ShiftType::ROR:
            result_and_carry = ir.RotateRight(value, amount, carry_in);
            break;
    }
    return result_and_carry;
}

} // namespace Arm
} // namespace Dynarmic
