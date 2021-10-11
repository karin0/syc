#include "use_def.hpp"

using namespace mips;

// All kinds must be reg (no const & void) but may not be unique (to be inserted in sets)
std::pair<vector<Reg>, vector<Reg>> get_use_def(Inst *i, Func *f) {
    if_a (BinaryInst, x, i) {
        if (x->rhs.is_reg())
            return {{x->dst}, {x->lhs, x->rhs}};
        return {{x->dst}, {x->lhs}};
    } else if_a (ShiftInst, x, i)
        return {{x->dst}, {x->lhs}};
    else if_a (MoveInst, x, i) {
        if (x->src.is_reg())
            return {{x->dst}, {x->src}};
        return {{x->dst}, {}};
    } else if_a (MultInst, x, i)
        return {{}, {x->lhs, x->rhs}};
    else if_a (DivInst, x, i)
        return {{}, {x->lhs, x->rhs}};
    else if_a (MFHiInst, x, i)
        return {{x->dst}, {}};
    else if_a (MFLoInst, x, i)
        return {{x->dst}, {}};
    else if_a (CallInst, x, i) {
        vector<Reg> use, def;
        uint n = std::min(x->func->params.size(), MAX_ARG_REGS);
        for (uint i = 0; i < n; ++i)
            use.emplace_back(Reg::Pinned, Regs::a0 + i);
        for (auto i: Regs::caller_saved)  // sp, ra are not allocation candidates
            def.emplace_back(Reg::Pinned, i);
        return {use, def};
    } else if_a (BranchInst, x, i)
        return {{}, {x->lhs, x->rhs}};
    else if_a (BranchZeroInst, x, i)
        return {{}, {x->lhs}};
    else if (is_a<ReturnInst>(i)) {
        if (f->ir->returns_int)
            return {{}, {Reg::make_pinned(Regs::v0)}};
        return {{}, {}};
    } else if_a (LoadInst, x, i)
        return {{x->dst}, {x->base}};
    else if_a (StoreInst, x, i)
        return {{}, {x->src, x->base}};
    else if_a (SysInst, x, i) {
        switch (x->no) {
            case 1: case 4:
                return {{}, {Reg::make_pinned(Regs::a0)}};
            case 5:
                return {{Reg::make_pinned(Regs::v0)}, {}};
            default:
                unreachable();
        }
    } else if_a (LoadStrInst, x, i)
        return {{x->dst}, {}};
    return {{}, {}};  // j
}

vector<Reg *> get_owned_regs(Inst *i) {
    if_a (BinaryInst, x, i) {
        if (x->rhs.is_reg())
            return {&x->dst, &x->lhs, &x->rhs};
        return {&x->dst, &x->lhs};
    } else if_a (ShiftInst, x, i)
        return {&x->dst, &x->lhs};
    else if_a (MoveInst, x, i) {
        if (x->src.is_reg())
            return {&x->dst, &x->src};
        return {&x->dst};
    } else if_a (MultInst, x, i)
        return {&x->lhs, &x->rhs};
    else if_a (DivInst, x, i)
        return {&x->lhs, &x->rhs};
    else if_a (MFHiInst, x, i)
        return {&x->dst};
    else if_a (MFLoInst, x, i)
        return {&x->dst};
    else if_a (BranchInst, x, i)
        return {&x->lhs, &x->rhs};
    else if_a (BranchZeroInst, x, i)
        return {&x->lhs};
    else if_a (LoadInst, x, i)
        return {&x->dst, &x->base};
    else if_a (StoreInst, x, i)
        return {&x->src, &x->base};
    else if_a (LoadStrInst, x, i)
        return {&x->dst};
    else
        return {};
}
