#include "liveness.hpp"

// All kinds must be reg (no const & void) but may not be unique (to be inserted in sets)
std::pair<vector<Reg>, vector<Reg>> get_def_use(Inst *i, Func *f) {
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
        vector<Reg> def, use;
        uint n = std::min(uint(x->func->params.size()), MAX_ARG_REGS);
        for (auto i: Regs::caller_saved)  // sp, ra are not allocation candidates
            def.emplace_back(Reg::Pinned, i);
        for (uint i = 0; i < n; ++i)
            use.emplace_back(Reg::Pinned, Regs::a0 + i);
        return {def, use};
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
            case 1: case 4: case 11:
                return {{}, {Reg::make_pinned(Regs::v0), Reg::make_pinned(Regs::a0)}};
            case 5:
                return {{Reg::make_pinned(Regs::v0)}, {Reg::make_pinned(Regs::v0)}};
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

// Machine & pinned regs are not considered here
std::pair<Reg *, vector<Reg *>> get_owned_def_use(Inst *i) {
    if_a (BinaryInst, x, i) {
        if (x->rhs.is_reg())
            return {&x->dst, {&x->lhs, &x->rhs}};
        return {&x->dst, {&x->lhs}};
    } else if_a (ShiftInst, x, i)
        return {&x->dst, {&x->lhs}};
    else if_a (MoveInst, x, i) {
        if (x->src.is_reg())
            return {&x->dst, {&x->src}};
        return {&x->dst, {}};
    } else if_a (MultInst, x, i)
        return {nullptr, {&x->lhs, &x->rhs}};
    else if_a (DivInst, x, i)
        return {nullptr, {&x->lhs, &x->rhs}};
    else if_a (MFHiInst, x, i)
        return {&x->dst, {}};
    else if_a (MFLoInst, x, i)
        return {&x->dst, {}};
    else if_a (BranchInst, x, i)
        return {nullptr, {&x->lhs, &x->rhs}};
    else if_a (BranchZeroInst, x, i)
        return {nullptr, {&x->lhs}};
    else if_a (LoadInst, x, i)
        return {&x->dst, {&x->base}};
    else if_a (StoreInst, x, i)
        return {nullptr, {&x->src, &x->base}};
    else if_a (LoadStrInst, x, i)
        return {&x->dst, {}};
    return {{}, {}};
}

vector<Reg> get_def(Inst *i) {
    if_a (BinaryInst, x, i)
        return {x->dst};
    else if_a (ShiftInst, x, i)
        return {x->dst};
    else if_a (MoveInst, x, i)
        return {x->dst};
    else if_a (MFHiInst, x, i)
        return {x->dst};
    else if_a (MFLoInst, x, i)
        return {x->dst};
    else if_a (LoadInst, x, i)
        return {x->dst};
    else if_a (LoadStrInst, x, i)
        return {x->dst};
    return {};
}


// TODO: drop ignored, colored, Machine/Pinned
bool is_ignored(const Operand &x) {
    return !(x.is_virtual() || (x.is_pinned() && Regs::inv_allocatable[x.val] < 32));
}

static void remove_colored(vector<Reg> &v) {
    v.erase(std::remove_if(v.begin(), v.end(), is_ignored), v.end());
    for (auto &x: v)
        asserts(x.is_uncolored());
}

std::pair<vector<Reg>, vector<Reg>> get_def_use_uncolored(Inst *i, Func *f) {
    auto r = get_def_use(i, f);
    remove_colored(r.first);
    remove_colored(r.second);
    return r;
}

// done: this may be unreliable, as bbs now are not "real" bbs, but can contain j/brs due to phi resolving
// that's possibly why dce on machine regs cannot be performed
// maybe we should add phi copies at the beginning of the source bb (tested to affect perf) or in a new inserted bb
void build_liveness(Func *f) {
    FOR_BB (bb, *f) {
        bb->use.clear();
        bb->def.clear();
        bb->live_out.clear();

        FOR_INST (i, *bb) {
            auto use_def = get_def_use_uncolored(i, f);
            for (auto &x: use_def.second) if (!bb->def.count(x))
                    bb->use.insert(x);
            for (auto &x: use_def.first) if (!bb->use.count(x))
                    bb->def.insert(x);
        }

        bb->live_in = bb->use;
    }
    bool changed;
    do {
        changed = false;
        FOR_BB (bb, *f) {
            std::set<Reg> out;
            for (auto *t: bb->succ)
                out.insert(t->live_in.begin(), t->live_in.end());
            if (out != bb->live_out) {
                changed = true;
                bb->live_out = std::move(out);
                bb->live_in = bb->use;
                for (auto &x: bb->live_out) if (!bb->def.count(x))
                        bb->live_in.insert(x);
            }
        }
    } while (changed);
}
