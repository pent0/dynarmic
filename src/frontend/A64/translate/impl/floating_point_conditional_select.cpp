/* This file is part of the dynarmic project.
 * Copyright (c) 2018 MerryMage
 * This software may be used and distributed according to the terms of the GNU
 * General Public License version 2 or any later version.
 */

#include <optional>

#include "frontend/A64/translate/impl/impl.h"

namespace Dynarmic::A64 {

bool TranslatorVisitor::FCSEL_float(Imm<2> type, Vec Vm, Cond cond, Vec Vn, Vec Vd) {
    const auto datasize = FPGetDataSize(type);
    if (!datasize || *datasize == 16) {
        return UnallocatedEncoding();
    }

    const IR::U32U64 operand1 = V_scalar(*datasize, Vn);
    const IR::U32U64 operand2 = V_scalar(*datasize, Vm);
    const IR::U32U64 result = ir.ConditionalSelect(cond, operand1, operand2);
    V_scalar(*datasize, Vd, result);

    return true;
}

} // namespace Dynarmic::A64
