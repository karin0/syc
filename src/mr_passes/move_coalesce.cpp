#include "mips.hpp"

using namespace mips;

struct S {
    std::unordered_map<Reg, int> m;
    std::unordered_map<int, vector<Reg>> rm;

    void pop(Reg x) {
        auto it = m.find(x);
        if (it != m.end()) {
            auto &v = rm.find(it->second)->second;
            vec_erase_if(v, [x](Reg y) {
                return y.equiv(x);
            });
        }
    }

    void push(Reg x, int i) {
        pop(x);
        m[x] = i;
        rm[i].push_back(x);
    }

    bool test(Reg x, int i) {
        auto it = rm.find(i);
        if (it != rm.end()) {
            auto &v = it->second;
            return std::find_if(v.begin(), v.end(), [x](Operand y) {
                return y.equiv(x);
            }) != v.end();
        }
        return false;
    }

    Operand find(int i) {
        auto it = rm.find(i);
        if (it != rm.end() && !it->second.empty())
            return it->second.back();
        return Operand::make_void();
    }

    void clear() {
        m.clear();
        rm.clear();
    }
};

vector<Reg> get_redef(Inst *i) {
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
    else if (is_a<CallInst>(i)) {
        vector<Reg> res;
        res.reserve(Regs::callee_saved.size());
        for (auto i: Regs::caller_saved)
            res.emplace_back(Reg::Machine, i);
        return res;
    } else if_a (LoadInst, x, i)
        return {x->dst};
    else if_a (SysInst, x, i) {
        switch (x->no) {
            case 1: case 4: case 11:
                return {};
            case 5:
                return {Reg::make_machine(Regs::v0)};
            default:
                unreachable();
        }
    } else if_a (LoadStrInst, x, i)
        return {x->dst};
    return {};
}

void li_coalesce(Func *f) {
    static S s;

    FOR_BB (bb, *f) {
        s.clear();
        FOR_LIST_MUT (i, bb->insts) {
            if_a (MoveInst, x, i) {
                if (x->src.is_const()) {
                    auto c = x->src.val;
                    if (s.test(x->dst, c)) {
                        bb->insts.erase(x);
                        delete x;
                    } else {
                        auto r = s.find(c);
                        if (!r.is_void())
                            x->src = r;
                        s.push(x->dst, c);
                    }
                } else
                    s.pop(x->dst);
            } else for (auto r: get_redef(i))
                s.pop(r);
        }
    }
}

void move_coalesce(Func *f) {
    FOR_BB (bb, *f) {
        FOR_LIST_MUT (i, bb->insts) {
            if_a (BinaryInst, x, i) {
                if ((x->op == BinaryInst::Add || x->op == BinaryInst::Sub || x->op == BinaryInst::Xor) &&
                    x->rhs == Operand::make_const(0)) {
                    if (x->dst.equiv(x->lhs))
                        bb->insts.erase(x);
                    else
                        bb->insts.replace(x, new MoveInst{x->dst, x->lhs});
                    delete i;
                }
            } else if_a (MoveInst, x, i) {
                if (x->dst.equiv(x->src)) {
                    bb->insts.erase(x);
                    delete i;
                }
            }
        }
    }
    li_coalesce(f);
}
