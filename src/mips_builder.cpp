#include "mips_builder.hpp"

using namespace mips;

namespace mips {

struct Builder {
    Prog *prog;
    Func *func;
    BB *bb;
    Operand args[MAX_ARG_REGS];

    Inst *push_point = nullptr;  // used by build_val for phi nodes only, make new pushed insts before the point

    Operand make_vreg() const {
        return func->make_vreg();
    }

    template <class T>
    T *push(T *i) {
        if (push_point)
            bb->insert(push_point, i);
        else
            bb->push(i);
        return i;
    }

    Operand ensure_reg(Operand x) {
        asserts(x.kind != Operand::Void);
        if (x.kind == Operand::Const) {
            if (x.val == 0)
                return Operand::make_machine(0);
            if (x.val == DATA_BASE) {
                prog->gp_used = true;
                return Operand::make_machine(Regs::gp);
            }
            return move_to_reg(x);
        }
        return x;
    }

    Operand move_to_reg(Operand x) {
        auto dst = make_vreg();
        push(new MoveInst{dst, x});
        return dst;
    }

    // used when rhs could be an overflowed const
    BinaryInst *new_binary(BinaryInst::Op op, Reg dst, Reg lhs, Operand rhs) {
        asserts(lhs.kind != Operand::Const);
        asserts(lhs.kind != Operand::Void);
        asserts(rhs.kind != Operand::Void);
        if (rhs.kind == Operand::Const && !is_imm(rhs.val))
            rhs = move_to_reg(rhs);
        return push(new BinaryInst(op, dst, lhs, rhs));
    }

    void new_syscall(uint no) {
        push(new MoveInst{Operand::make_machine(Regs::v0), Operand::make_const(int(no))});
        push(new SysInst{no});
    }
};

}

Operand ir::Const::build_val(mips::Builder *) {
    return Operand{val};
}

Operand ir::Global::build_val(mips::Builder *) {
    return Operand{int(var->addr)};
}

Operand ir::Argument::build_val(mips::Builder *ctx) {
    if (pos < MAX_ARG_REGS)
        return ctx->args[pos];
    if (!mach_res.is_void())
        return mach_res;

    // load from sp + 4 * (stack_size + pos - 4)
    // load for only once
    auto dst = ctx->make_vreg();
    auto *load = ctx->push(new mips::LoadInst{dst, Operand::make_machine(Regs::sp), int(pos)});
    ctx->func->arg_loads.push_back(load);

    return mach_res = dst;
}

Operand ir::Undef::build_val(mips::Builder *) {
    warn("undef value is used");
    return Operand::make_machine(0);
}

Operand ir::Inst::build_val(mips::Builder *) {
    asserts(mach_res.kind != Operand::Void);
    return mach_res;
}

#define BUILD_USE(u) (u).value->build_val(ctx)

// Sub, Div, Mod can't be swapped
ir::OpKind swapped_op(ir::OpKind op) {
    using namespace tkd;
    switch (op) {
        case Add: case Mul: case Eq: case Ne:
            return op;
        case Lt: return Gt;
        case Gt: return Lt;
        case Le: return Ge;
        case Ge: return Le;
        default:
            unreachable();
    }
}

// for src = 0/1
BinaryInst *new_bool_not(Reg dst, Reg src) {
    return new BinaryInst{BinaryInst::Xor, dst, src, Operand::make_const(1)};
}

// for any src
BinaryInst *new_not(Reg dst, Reg src) {
    return new BinaryInst{BinaryInst::Ltu, dst, src, Operand::make_const(1)};
}

BinaryInst *new_bool(Reg dst, Reg src) {
    return new BinaryInst{BinaryInst::Ltu, dst, Operand::make_machine(0), src};
}

static Reg build_neg_reg(Reg x, Builder *ctx) {
    Reg dst = ctx->make_vreg();
    ctx->push(new BinaryInst{BinaryInst::Sub, dst, Reg::make_machine(0), x});
    return dst;
}

static Reg build_reg_mult_const(Reg lh, int rh, Builder *ctx) {
    asserts(lh.is_reg());
    if (rh == 0)
        return Operand::make_const(0);
    if (rh == 1)
        return lh;
    if (rh == -1)
        return build_neg_reg(lh, ctx);

    bool neg = rh < 0;
    uint a = rh;
    if (neg)
        a = -a;  // -rh in int can overflow (0x80000000)
    Reg dst = ctx->make_vreg();
    if (!(a & (a - 1))) {
        ctx->push(new ShiftInst{ShiftInst::Ll, dst, lh, uint(__builtin_ctz(a))});
        if (neg && a != uint(rh))
            return build_neg_reg(dst, ctx);
    } else
        ctx->new_binary(BinaryInst::Mul, dst, lh, Operand::make_const(rh));
    return dst;
}

static Operand build_reg_div_const(Reg lh, int d, bool is_mod, Builder *ctx) {
    uint a = d;
    if (d < 0)
        a = -a;

    if (a == 1) {
        if (is_mod)
            return Operand::make_const(0);
        if (d < 0)
            return build_neg_reg(lh, ctx);
        return lh;
    }

    Reg dst;
    if (!(a & (a - 1))) {
        /* this fails negative lhs and fixing's not worth it:
            dst = ctx->make_vreg();
            ctx->new_binary(BinaryInst::And, dst, lh, Operand::make_const(a - 1));
            if (d < 0)
                return build_neg_reg(dst, ctx);
            return dst;
        */
        uint l = __builtin_ctz(a);
        auto v0 = ctx->make_vreg();
        ctx->push(new ShiftInst{ShiftInst::Ra, v0, lh, l - 1});
        auto v1 = ctx->make_vreg();
        ctx->push(new ShiftInst{ShiftInst::Rl, v1, v0, 32 - l});
        auto v2 = ctx->make_vreg();
        ctx->push(new BinaryInst{BinaryInst::Add, v2, lh, v1});
        dst = ctx->make_vreg();
        ctx->push(new ShiftInst{ShiftInst::Ra, dst, v2, l});
    } else {
        using u64 = std::uint64_t;
        u64 t = 1ull << 31;
        u64 n = t - t % a - 1;
        u64 p = 1ull << 32;
        uint s = 0;
        while (p <= n * (a - p % a))
            p <<= 1, ++s;
        u64 m = (p + a - p % a) / a;
        int c = int(m & ((1ull << 32) - 1));

        // v1 = hi(a * m)
        auto v0 = ctx->make_vreg();
        ctx->push(new MoveInst{v0, Reg::make_const(c)});
        ctx->push(new MultInst{lh, v0});

        auto v1 = ctx->make_vreg();
        ctx->push(new MFHiInst{v1});

        if (m >= 1u << 31) {
            auto v = ctx->make_vreg();
            ctx->push(new BinaryInst{BinaryInst::Add, v, v1, lh});
            v1 = v;
        }

        // v2 = v1 sra s
        Reg v2;
        if (s) {
            v2 = ctx->make_vreg();
            ctx->push(new ShiftInst{ShiftInst::Ra, v2, v1, s});
        } else
            v2 = v1;

        // dst = sign(d) * (is_neg(lh) + v2)
        // d > 0: dst = sgn + v2 = v2 - (-sgn)
        // d < 0: dst = -sgn - v2
        dst = ctx->make_vreg();
        auto v3 = ctx->make_vreg();
        ctx->push(new ShiftInst{ShiftInst::Ra, v3, lh, 31});
        if (d > 0)
            ctx->push(new BinaryInst{BinaryInst::Sub, dst, v2, v3});
        else
            ctx->push(new BinaryInst{BinaryInst::Sub, dst, v3, v2});
    }

    if (!is_mod)
        return dst;

    auto v4 = build_reg_mult_const(dst, d, ctx);
    auto r = ctx->make_vreg();
    ctx->push(new BinaryInst{BinaryInst::Sub, r, lh, v4});
    return r;
}

Operand ir::BinaryInst::build(mips::Builder *ctx) {
    auto lh = BUILD_USE(lhs);
    auto rh = BUILD_USE(rhs);
    if (rh.kind == Operand::Const)
        if (lh.kind == Operand::Const)
            return Operand::make_const(ir::eval_bin(op, lh.val, rh.val));

    if (op == tkd::Div || op == tkd::Mod) {
        if (rh.is_const())
            return build_reg_div_const(lh, rh.val, op == tkd::Mod, ctx);
        auto dst = ctx->make_vreg();
        lh = ctx->ensure_reg(lh);
        rh = ctx->ensure_reg(rh);
        ctx->push(new DivInst{lh, rh});
        if (op == tkd::Div)
            ctx->push(new MFLoInst{dst});
        else
            ctx->push(new MFHiInst{dst});
        return dst;
    }

    using mips::BinaryInst;

    if (op == tkd::Mul) {
        if (lh.is_const())
            return build_reg_mult_const(rh, lh.val, ctx);
        if (rh.is_const())
            return build_reg_mult_const(lh, rh.val, ctx);
        auto dst = ctx->make_vreg();
        ctx->push(new BinaryInst{BinaryInst::Mul, dst, lh, rh});
        return dst;
    }

    auto dst = ctx->make_vreg();
    if (lh.kind == Operand::Const) {
        // rh must be a reg now
        switch (op) {
            case tkd::Sub: {
                auto t = ctx->move_to_reg(lh);
                ctx->push(new BinaryInst{BinaryInst::Sub, dst, t, rh});
                return dst;
            }
            case tkd::Gt:
                ctx->new_binary(BinaryInst::Lt, dst, rh, lh);
                return dst;
            default:
                break;
        }
        std::swap(lh, rh);
        op = swapped_op(op);
        // now lh is a reg and rh is a const
    }

    // lh must be a reg now
    switch (op) {
        case tkd::Add:
            ctx->new_binary(BinaryInst::Add, dst, lh, rh);
            return dst;

        case tkd::Sub:
            // subiu is not there
            if (rh.kind == Operand::Const)
                ctx->new_binary(BinaryInst::Add, dst, lh,
                                rh.val == Operand::MIN_CONST ? ctx->move_to_reg(Operand::make_const(Operand::MIN_CONST))
                                    : Operand::make_const(-rh.val)
                                );  // - INT_MIN
            else
                ctx->push(new BinaryInst{BinaryInst::Sub, dst, lh, rh});
            return dst;

        case tkd::Lt:
            ctx->new_binary(BinaryInst::Lt, dst, lh, rh);
            return dst;

        case tkd::Gt: {
            auto rt = ctx->ensure_reg(rh);
            ctx->push(new BinaryInst{BinaryInst::Lt, dst, rt, lh});
            return dst;
        }

        case tkd::Le:
            // r1 <= r2  : !(r2 < r1)
            // r1 <= imm : r1 < imm + 1
            if (rh.kind == Operand::Const) {
                if (rh.val == Operand::MAX_CONST)
                    return Operand::make_const(1); // dst is discarded
                ctx->new_binary(BinaryInst::Lt, dst, lh, Operand::make_const(rh.val + 1));
            } else {
                auto nt = ctx->make_vreg();
                ctx->push(new BinaryInst{BinaryInst::Lt, nt, rh, lh});
                ctx->push(new_bool_not(dst, nt));
            }
            return dst;

        case tkd::Ge: {
            // r1 >= r2  : !(r1 < r2)
            // r1 >= imm : !(r1 < imm)
            auto nt = ctx->make_vreg();
            ctx->new_binary(BinaryInst::Lt, nt, lh, rh);
            ctx->push(new_bool_not(dst, nt));
            return dst;
        }

        case tkd::Eq: {
            auto nt = ctx->make_vreg();
            ctx->new_binary(BinaryInst::Xor, nt, lh, rh);
            ctx->push(new_not(dst, nt));
            return dst;
        }

        case tkd::Ne: {
            auto nt = ctx->make_vreg();
            ctx->new_binary(BinaryInst::Xor, nt, lh, rh);
            ctx->push(new_bool(dst, nt));
            return dst;
        }

        default:
            unreachable();
    }
}

void build_buf_output(const string &s, Builder *ctx) {
    if (s.size() == 1 || s == "\\n") {
        int c = s.size() == 1 ? s[0] : '\n';
        ctx->push(new MoveInst{Operand::make_machine(Regs::a0), Operand::make_const(c)});
        ctx->new_syscall(11);
    } else {
        uint id = ctx->prog->find_str(s);
        ctx->push(new LoadStrInst{Operand::make_machine(Regs::a0), id});
        ctx->new_syscall(4);
    }
}

Operand ir::CallInst::build(mips::Builder *ctx) {
    if (is_a<ir::GetIntFunc>(func)) {
        ctx->new_syscall(5);
        // TODO: use v0 or move it to vreg?
        return ctx->move_to_reg(Operand::make_machine(Regs::v0));
    }
    if_a (ir::PrintfFunc, f, func) {
        // <NormalChar> → ⼗进制编码为32,33,40-126的ASCII字符，'\'（编码92）出现当且仅当为'\n'
        // so all %-s are %d
        const char *p = f->fmt + 1;
        const char *end = f->fmt + f->len - 1;  // within the quotes
        const Use *argu = &args.front();
        string buf;
        while (true) {
            const char *s = p;
            while (p < end && *p != '%')
                ++p;
            if (p > s)
                buf.append(s, p - s);
            if (p >= end)
                break;
            auto arg = BUILD_USE(*argu++);
            if (arg.kind == Operand::Const)
                buf += std::to_string(int(arg.val));
            else {
                if (!buf.empty()) {
                    build_buf_output(buf, ctx);
                    buf.clear();
                }
                ctx->push(new MoveInst{Operand::make_machine(Regs::a0), arg});
                ctx->new_syscall(1);
            }
            p += 2;
        }
        if (!buf.empty())
            build_buf_output(buf, ctx);
        return Operand::make_void();
    }
    uint n = args.size();
    if (n > MAX_ARG_REGS)
        ctx->func->max_call_arg_num = std::max(ctx->func->max_call_arg_num, n - MAX_ARG_REGS);
    // ctx->func->max_call_arg_num = std::max(ctx->func->max_call_arg_num, n);
    for (uint i = 0; i < n; ++i) {
        auto arg = BUILD_USE(args[i]);
        if (i < MAX_ARG_REGS)
            ctx->push(new MoveInst{Operand::make_machine(Regs::a0 + i), arg});
        else
            ctx->push(new mips::StoreInst{
                ctx->ensure_reg(arg), Operand::make_machine(Regs::sp),
                int((i - MAX_ARG_REGS) * 4)
            });
            // sw to sp + (i-4) * 4
    }
    ctx->push(new mips::CallInst{func});
    if (func->returns_int)
        return ctx->move_to_reg(Operand::make_machine(Regs::v0));
    return Operand::make_void();
}

Operand ir::BranchInst::build(mips::Builder *ctx) {
    using mips::BranchInst;
    asserts(next == nullptr);
    auto con = BUILD_USE(cond);
    if (con.is_const()) {
        auto *to = con.val ? bb_then : bb_else;
        if (to->mbb != ctx->bb->next)
            ctx->push(new mips::JumpInst{to->mbb});
    } else {
        if (ctx->bb->next == bb_then->mbb)
            ctx->push(new BranchInst{BranchInst::Eq, BUILD_USE(cond), Operand::make_machine(0), bb_else->mbb});
        else {
            ctx->push(new BranchInst{BranchInst::Ne, BUILD_USE(cond), Operand::make_machine(0), bb_then->mbb});
            if (ctx->bb->next != bb_else->mbb)
                ctx->push(new mips::JumpInst{bb_else->mbb});  // TODO: opti
        }
    }
    return Operand::make_void();
}

Operand ir::JumpInst::build(mips::Builder *ctx) {
    asserts(next == nullptr);
    if (ctx->bb->next != bb_to->mbb)
        ctx->push(new mips::JumpInst{bb_to->mbb});
    return Operand::make_void();
}

Operand ir::ReturnInst::build(mips::Builder *ctx) {
    if (ctx->func->ir->returns_int) {
        asserts(val.value);
        ctx->push(new MoveInst{Operand::make_machine(Regs::v0), BUILD_USE(val)});
    }
    ctx->push(new mips::ReturnInst);
    return Operand::make_void();
}

static int int_cast(uint x) {
    return uint(x) <= Operand::MAX_CONST ? int(x) :
        int(x - Operand::MAX_CONST - 1) + Operand::MIN_CONST;
}

std::pair<Reg, int> resolve_mem(Operand base, Operand off, Builder *ctx) {
    // TODO: use sp when base is alloca
    if (off.kind == Operand::Const) {
        if (base.kind == Operand::Const) {
            // TODO: what if imm overflows?
            int d = base.val + off.val, imm;
            if (!is_imm(d) && is_imm(imm = int_cast(uint(d) - DATA_BASE))) {
                ctx->prog->gp_used = true;
                return {Operand::make_machine(Regs::gp), imm};
            }
            return {Operand::make_machine(0), base.val + off.val};
        }
        return {base, off.val};
    }
    // TODO: MARS will do the trick when the offset overflows imm (one more lui $at)
    // To allocate $at, we must do it explicitly
    if (base.kind == Operand::Const)
        return {off, base.val};
    auto t = ctx->make_vreg();
    ctx->push(new BinaryInst{BinaryInst::Add, t, base, off});
    return {t, 0};
}

Operand ir::LoadInst::build(mips::Builder *ctx) {
    auto base = BUILD_USE(this->base), off = BUILD_USE(this->off);
    auto dst = ctx->make_vreg();
    auto res = resolve_mem(base, off, ctx);
    ctx->push(new mips::LoadInst{dst, res.first, res.second});
    return dst;
}

Operand ir::StoreInst::build(mips::Builder *ctx) {
    auto base = BUILD_USE(this->base), off = BUILD_USE(this->off);
    auto src = ctx->ensure_reg(BUILD_USE(val));
    auto res = resolve_mem(base, off, ctx);
    ctx->push(new mips::StoreInst{src, res.first, res.second});
    return Operand::make_void();
}

Operand ir::GEPInst::build(mips::Builder *ctx) {
    // dst = base + off * size
    auto base = BUILD_USE(this->base), off = BUILD_USE(this->off);
    using mips::BinaryInst;
    if (off.kind == Operand::Const) {
        off.val *= size;
        if (base.kind == Operand::Const)
            return Operand::make_const(off.val + base.val);
        auto dst = ctx->make_vreg();
        ctx->new_binary(BinaryInst::Add, dst, base, off);
        return dst;
    }
    // TODO: if off is from reg mult imm, this can be better optimized, maybe before building mips on the IR
    auto ot = build_reg_mult_const(off, size, ctx);
    auto dst = ctx->make_vreg();
    ctx->new_binary(BinaryInst::Add, dst, ot, base);
    return dst;
}

Operand ir::AllocaInst::build(mips::Builder *ctx) {
    auto dst = ctx->make_vreg();
    auto *add = ctx->new_binary(mips::BinaryInst::Add, dst,
        Operand::make_machine(Regs::sp), Operand::make_const(int(ctx->func->alloca_num)));
    infof("alloca val", add->rhs.val);
    // will be fixed with max_call_arg_num
    ctx->func->allocas.push_back(add);
    ctx->func->alloca_num += var->size();
    return dst;
}

Operand ir::PhiInst::build(mips::Builder *ctx) {
    return ctx->make_vreg();
}

Operand ir::BinaryBranchInst::build(mips::Builder *ctx) {
    using mips::BinaryInst;
    using mips::BranchInst;
    using mips::BranchZeroInst;
    using namespace rel;
    asserts(next == nullptr);
    auto lh = BUILD_USE(lhs), rh = BUILD_USE(rhs);
    auto op = this->op;
    infof("bbi building", lh, rh, lhs, rhs);
    if (lh.is_const() || (lh.is_machine() && lh.val == 0)) {
        std::swap(lh, rh);
        op = ir::BinaryBranchInst::swap_op(op);
    }
    if (lh.is_const() || (lh.is_machine() && lh.val == 0)) {
        error("uncoalesced const br!");
        auto *to = ir::rel::eval(op, lh.val, rh.val) ? bb_then : bb_else;
        ctx->push(new mips::JumpInst{to->mbb});
        return Operand::make_void();
    }

    mips::BaseBranchInst *br;
    auto *to = bb_then->mbb;
    auto r0 = Reg::make_machine(0);
    if (rh.val == 0 && (rh.is_const() || rh.is_machine()))
        br = new BranchZeroInst{op, lh, to};
    else if (rh.is_reg()) {
        switch (op) {
            case Eq:
                br = new BranchInst{BranchInst::Eq, lh, rh, to};
                break;
            case Ne:
                br = new BranchInst{BranchInst::Ne, lh, rh, to};
                break;
            default: {
                // Lt: lh < rh
                // Le: !(rh < lh)
                // Gt: rh < lh
                // Ge: !(lh < rh)
                auto t = ctx->make_vreg();
                if (op == Lt || op == Ge)
                    ctx->push(new BinaryInst{BinaryInst::Lt, t, lh, rh});
                else
                    ctx->push(new BinaryInst{BinaryInst::Lt, t, rh, lh});
                br = new BranchInst{
                    op == Lt || op == Gt ? BranchInst::Ne : BranchInst::Eq,
                    t, r0, to
                };
            }
        }
    } else {
        asserts(rh.is_const());
        if (rh.val == 1 && op == Lt)
            br = new BranchZeroInst(Le, lh, to);
        else if (rh.val == 1 && op == Ge)
            br = new BranchZeroInst(Gt, lh, to);
        else if (rh.val == -1 && op == Le)
            br = new BranchZeroInst(Lt, lh, to);
        else if (rh.val == -1 && op == Gt)
            br = new BranchZeroInst(Ge, lh, to);
        else {
            // lh < c  : lh < c
            // lh <= c : lh < c + 1
            // lh > c  : !(lh < c + 1)
            // lh >= c : !(lh < c)
            auto t = ctx->make_vreg();
            if (op == Eq || op == Ne) { // li, b vs xor, b
                ctx->push(new MoveInst{t, rh});
                br = new BranchInst(op == Eq ? BranchInst::Eq : BranchInst::Ne, lh, t, to);
            } else {
                if (op == Lt || op == Ge)
                    ctx->new_binary(BinaryInst::Lt, t, lh, Operand::make_const(rh.val));
                else {
                    asserts(rh.val != Operand::MAX_CONST);  // dbe
                    ctx->new_binary(BinaryInst::Lt, t, lh, Operand::make_const(rh.val + 1));
                }
                br = new BranchInst(op == Lt || op == Le ? BranchInst::Ne : BranchInst::Eq, t, r0, to);
            }
        }
    }

    if (ctx->bb->next == bb_then->mbb) {
        br->invert();
        br->to = bb_else->mbb;
        ctx->push(br);
    } else {
        ctx->push(br);
        if (ctx->bb->next != bb_else->mbb)
            ctx->push(new mips::JumpInst{bb_else->mbb});  // TODO: opti
    }
    return Operand::make_void();
}

Prog build_mr(ir::Prog &ir) {
    Prog res{&ir};

    uint data = DATA_BASE;
    // big data base address affects global access, use gp as shared base reg
    // TODO: remove unused (and const) globs and arrange them (put smaller ones first?) before this
    for (auto *glob: ir.globals) {
        infof("addr of", glob->name, "is", data);
        glob->addr = data;
        data += glob->size() << 2;
    }
    res.str_base_addr = data;

    Builder ctx;
    ctx.prog = &res;
    res.funcs.reserve(ir.funcs.size());
    for (auto &fun: ir.funcs) {
        res.funcs.emplace_back(&fun);
        auto *func = &res.funcs.back();

        FOR_BB (ibb, fun) {
            auto *bb = ibb->mbb = func->new_bb();
            bb->loop_depth = ibb->loop ? ibb->loop->depth : 0;
        }

        auto *bb_start = func->bbs.front;
        uint n = std::min(uint(fun.params.size()), MAX_ARG_REGS);
        for (size_t i = 0; i < n; ++i) {
            auto src = Operand::make_machine(Regs::a0 + i);
            auto dst = func->make_vreg();
            bb_start->push(new MoveInst{dst, src});
            // auto *value = fun.params[i]->value;  Argument, or removed Alloca
            ctx.args[i] = dst;
        }

        ctx.func = func;
        FOR_BB (ibb, fun) {
            ctx.bb = ibb->mbb;
            FOR_INST (i, *ibb)
                i->mach_res = i->build(&ctx);
            // for (auto *t: ibb->get_succ())
            //     ibb->mbb->succ.push_back(t->mbb);
            // do this later since bbs may be split
        }

        // resolving phi makes bbs not so basic, requiring a bb_normalization pass
        FOR_BB (ibb, fun) {
            auto *bb = ibb->mbb;
            infof("ibb", ibb);
            FOR_INST (i, *ibb) {
                infof("ibb inst", ibb, i);
                if_a (ir::PhiInst, x, i) {
                    auto t = func->make_vreg();
                    bb->insts.push_front(new MoveInst{x->mach_res, t});
                    for (auto &u: x->vals) {
                        auto *uv = u.first.value;
                        if (uv == &ir::Undef::VAL)
                            continue;
                        auto *ubb = u.second->mbb;

                        bool found = false;
                        bool no_fall = false;
                        ctx.bb = ubb;
                        FOR_LIST_MUT (j, ubb->insts) {
                            if (is_a<mips::ReturnInst>(j)) {
                                no_fall = true;
                                break;
                            }
                            if_a (mips::ControlInst, y, j) {
                                // debug("%p, got br to mbb_%u, i am %u", j, y->to->id, bb->id);
                                if (y->to == bb) {
                                    found = true;
                                    ctx.push_point = y;
                                    ctx.push(new MoveInst{t, uv->build_val(&ctx)});
                                    ctx.push_point = nullptr;
                                    break;
                                } else if (is_a<mips::JumpInst>(y)) {
                                    no_fall = true;
                                    break;
                                }
                            }
                        }
                        // prev != ubb when branch to bb is removed during codegen and no need to copy
                        if (!found && !no_fall && bb->prev == ubb)
                            ubb->push(new MoveInst{t, uv->build_val(&ctx)});
                    }
                } else
                    break;
            }
        }

        for (auto *x: func->allocas) {
            asserts(x->rhs.is_const());
            // TODO: what if imm overflows?
            infof("fixing", func->ir->name, "idx =", x->rhs.val);
            x->rhs.val = int((func->max_call_arg_num + uint(x->rhs.val)) << 2);
            infof("now val =", x->rhs.val);
        }
    }

    return res;
}
