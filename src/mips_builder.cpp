#include "mips_builder.hpp"

using namespace mips;

namespace mips {

struct Builder {
    Prog *prog;
    Func *func;
    BB *bb;

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
        if (x.kind == Operand::Const)
            return move_to_reg(x);
        return x;
    }

    Operand move_to_reg(Operand x) {
        auto dst = make_vreg();
        push(new MoveInst{dst, x});
        return dst;
    }

    // used when rhs could be an overflowed const
    BinaryInst *new_binary(BinaryInst::Op op, Reg dst, Reg lhs, Reg rhs) {
        asserts(lhs.kind != Operand::Const);
        asserts(lhs.kind != Operand::Void);
        asserts(rhs.kind != Operand::Void);
        if (rhs.kind == Operand::Const && !is_imm(rhs.val))
            rhs = move_to_reg(rhs);
        return push(new BinaryInst(op, dst, lhs, rhs));
    }

    void new_syscall(uint no) {
        push(new MoveInst{Operand::make_pinned(Regs::v0), Operand::make_const(int(no))});
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
    if (mach_res.kind != Operand::Void)
        return mach_res;

    // load from old_sp + 4 * (stack_size + pos - 4)
    auto dst = ctx->make_vreg();
    auto *load = ctx->push(new mips::LoadInst{dst, Operand::make_pinned(Regs::sp), int(pos)});
    // TODO: fix this
    ctx->func->arg_loads.push_back(load);
    // TODO: load every time or only once?

    return dst;
}

Operand ir::Undef::build_val(mips::Builder *) {
    warn("undef value is used");
    return Operand::make_pinned(0);
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
            fatal("unswappable op");
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
    return new BinaryInst{BinaryInst::Ltu, dst, Operand::make_pinned(0), src};
}

void build_mult(Reg dst, Operand lh, Operand rh, Builder *ctx) {  // TODO: opti
    lh = ctx->ensure_reg(lh);
    rh = ctx->ensure_reg(rh);
    ctx->push(new MultInst{lh, rh});
    ctx->push(new MFLoInst{dst});
}

void build_mult_const(Reg dst, Operand lh, int rh, Builder *ctx) {  // TODO
    build_mult(dst, lh, Operand::make_const(rh), ctx);
}

Operand ir::BinaryInst::build(mips::Builder *ctx) {
    auto lh = BUILD_USE(lhs);
    auto rh = BUILD_USE(rhs);
    if (rh.kind == Operand::Const) {
        if (lh.kind == Operand::Const) {
            return Operand::make_const(ir::eval_bin(op, lh.val, rh.val));
        }
    }
    auto dst = ctx->make_vreg();

    if (op == tkd::Div || op == tkd::Mod) {
        lh = ctx->ensure_reg(lh);
        rh = ctx->ensure_reg(rh);
        ctx->push(new DivInst{lh, rh});
        if (op == tkd::Div)
            ctx->push(new MFLoInst{dst});
        else
            ctx->push(new MFHiInst{dst});
        return dst;
    }

    if (op == tkd::Mul) {
        build_mult(dst, lh, rh, ctx);
        return dst;
    }

    using mips::BinaryInst;

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
                ctx->new_binary(BinaryInst::Add, dst, lh, Operand::make_const(-rh.val));
            else
                ctx->push(new BinaryInst{BinaryInst::Sub, dst, lh, rh});
            return dst;

        // TODO: opti for branch
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
            if (rh.kind == Operand::Const)
                ctx->new_binary(BinaryInst::Lt, dst, lh, Operand::make_const(rh.val + 1));
            else {
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
            fatal("unexpected ir binary op");
    }
}

Operand ir::CallInst::build(mips::Builder *ctx) {
    if (is_a<ir::GetIntFunc>(func)) {
        ctx->new_syscall(5);
        // TODO: use v0 or move it to vreg?
        return ctx->move_to_reg(Operand::make_pinned(Regs::v0));
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
                    uint id = ctx->prog->find_str(buf);
                    ctx->push(new LoadStrInst{Operand::make_pinned(Regs::a0), id});
                    ctx->new_syscall(4);
                    buf.clear();
                }
                ctx->push(new MoveInst{Operand::make_pinned(Regs::a0), arg});
                ctx->new_syscall(1);
            }
            p += 2;
        }
        if (!buf.empty()) {
            uint id = ctx->prog->find_str(buf);
            ctx->push(new LoadStrInst{Operand::make_pinned(Regs::a0), id});
            ctx->new_syscall(4);
            buf.clear();
        }
        return Operand::make_void();
    }
    uint n = args.size();
    ctx->func->max_call_arg_num = std::max(ctx->func->max_call_arg_num, n);
    for (uint i = 0; i < n; ++i) {
        auto arg = BUILD_USE(args[i]);
        if (i < MAX_ARG_REGS)
            ctx->push(new MoveInst{Operand::make_pinned(Regs::a0 + i), arg});
        else
            ctx->push(new mips::StoreInst{ctx->ensure_reg(arg), Operand::make_pinned(Regs::sp), int((i - 4) * 4)});
            // sw to sp + (i-4) * 4
    }
    ctx->push(new mips::CallInst{func});
    if (func->returns_int)
        return ctx->move_to_reg(Operand::make_pinned(Regs::v0));
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
            ctx->push(new BranchInst{BranchInst::Eq, BUILD_USE(cond), Operand::make_pinned(0), bb_else->mbb});
        else {
            ctx->push(new BranchInst{BranchInst::Ne, BUILD_USE(cond), Operand::make_pinned(0), bb_then->mbb});
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
        ctx->push(new MoveInst{Operand::make_pinned(Regs::v0), BUILD_USE(val)});
    }
    ctx->push(new mips::ReturnInst);
    return Operand::make_void();
}

Operand ir::LoadInst::build(mips::Builder *ctx) {
    auto base = BUILD_USE(this->base), off = BUILD_USE(idx);
    auto dst = ctx->make_vreg();
    using mips::LoadInst;
    using mips::BinaryInst;
    if (off.kind == Operand::Const) {
        off.val <<= 2;
        if (base.kind == Operand::Const) {
            // TODO: what if imm overflow?
            ctx->push(new LoadInst{dst, Operand::make_pinned(0), base.val + off.val});
        } else
            ctx->push(new LoadInst{dst, base, off.val});
    } else {
        auto ot = ctx->make_vreg();
        ctx->push(new ShiftInst{ShiftInst::Ll, ot, off, 2});  // TODO: opti
        if (base.kind == Operand::Const) {
            ctx->push(new LoadInst{dst, off, base.val});
            // MARS will do the trick when imm overflows
            // To allocate $at, we must do it explicitly
        } else {
            auto at = ctx->make_vreg();
            ctx->push(new BinaryInst{BinaryInst::Add, at, base, ot});
            ctx->push(new LoadInst{dst, at, 0});
        }
    }
    return dst;
}

Operand ir::StoreInst::build(mips::Builder *ctx) {
    auto base = BUILD_USE(this->base), off = BUILD_USE(idx);
    auto src = ctx->ensure_reg(BUILD_USE(val));
    using mips::StoreInst;
    using mips::BinaryInst;
    if (off.kind == Operand::Const) {
        off.val <<= 2;
        if (base.kind == Operand::Const)
            ctx->push(new StoreInst{src, Operand::make_pinned(0), base.val + off.val});
        else
            ctx->push(new StoreInst{src, base, off.val});
    } else {
        auto ot = ctx->make_vreg();
        ctx->push(new ShiftInst{ShiftInst::Ll, ot, off, 2});
        if (base.kind == Operand::Const)
            ctx->push(new StoreInst{src, off, base.val});
        else {
            auto at = ctx->make_vreg();
            ctx->push(new BinaryInst{BinaryInst::Add, at, base, ot});
            ctx->push(new StoreInst{src, at, 0});
        }
    }
    return Operand::make_void();
}

Operand ir::GEPInst::build(mips::Builder *ctx) {
    // dst = base + off * size * 4
    auto base = BUILD_USE(this->base), off = BUILD_USE(idx);
    using mips::BinaryInst;
    if (off.kind == Operand::Const) {
        off.val = (off.val * size) << 2;
        if (base.kind == Operand::Const)
            return Operand::make_const(off.val + base.val);
        auto dst = ctx->make_vreg();
        ctx->new_binary(BinaryInst::Add, dst, base, off);
        return dst;
    }
    auto ot = ctx->make_vreg();
    build_mult_const(ot, off, size << 2, ctx);
    auto dst = ctx->make_vreg();
    ctx->new_binary(BinaryInst::Add, dst, ot, base);
    return dst;
}

Operand ir::AllocaInst::build(mips::Builder *ctx) {
    auto dst = ctx->make_vreg();
    auto *add = ctx->new_binary(mips::BinaryInst::Add, dst,
                    Operand::make_pinned(Regs::sp), Operand::make_const(int(ctx->func->stack_size)));
    // TODO: fix with max_call_arg_num
    ctx->func->allocas.push_back(add);
    ++ctx->func->stack_size;
    return dst;
}

Operand ir::PhiInst::build(mips::Builder *ctx) {
    // TODO
    return ctx->make_vreg();
}

Prog build_mr(ir::Prog &ir) {
    Prog res{&ir};

    uint data = 0;  // TODO: check it in MARS
    // TODO: remove unused globs before this
    for (auto *glob: ir.globals) {
        glob->addr = data;
        data += glob->size() << 2;
    }

    Builder ctx;
    ctx.prog = &res;
    res.funcs.reserve(ir.funcs.size());
    for (auto &fun: ir.funcs) {
        res.funcs.emplace_back(&fun);
        auto *func = &res.funcs.back();

        FOR_IBB (ibb, fun)
            ibb->mbb = func->new_bb();


        auto *bb_start = func->bbs.front;
        uint n = std::min(fun.params.size(), MAX_ARG_REGS);
        for (size_t i = 0; i < n; ++i) {
            auto src = Operand::make_pinned(Regs::a0 + i);
            auto dst = func->make_vreg();
            bb_start->push(new MoveInst{dst, src});
            auto *value = fun.params[i]->value;
            if (value)  // Argument, instead of removed Alloca
                fun.params[i]->value->mach_res = dst;
        }

        ctx.func = func;
        FOR_IBB (ibb, fun) {
            ctx.bb = ibb->mbb;
            FOR_IINST (i, *ibb)
                i->mach_res = i->build(&ctx);
            for (auto *t: ibb->get_succ())
                ibb->mbb->succ.push_back(t->mbb);
        }

        FOR_IBB (ibb, fun) {
            auto *bb = ibb->mbb;
            auto *head = bb->insts.front;
            FOR_IINST (i, *ibb) {
                if_a (ir::PhiInst, x, i) {
                    auto t = func->make_vreg();
                    if (head)
                        bb->insts.insert(head, new MoveInst{x->mach_res, t});
                    else
                        bb->insts.push(new MoveInst{x->mach_res, t});
                    for (auto &u: x->vals) {
                        auto *uv = u.first.value;
                        auto *ubb = u.second->mbb;

                        bool br = false;
                        ctx.bb = ubb;
                        FOR_MINST (j, *ubb) {
                            if_a (mips::ControlInst, y, j) {
                                info("got br to mbb_%u", y->to->id);
                                if (y->to == bb) {
                                    br = true;
                                    ctx.push_point = y;
                                    ctx.push(new MoveInst{t, uv->build_val(&ctx)});
                                    ctx.push_point = nullptr;
                                    break;
                                }
                            }
                        }
                        if (!br && bb->prev == ubb)  // prev != ubb when branch to bb is removed during codegen
                            ubb->push(new MoveInst{t, uv->build_val(&ctx)});

                        /*
                        if (bb->prev == ubb) {
                            ctx.bb = ubb;
                            ubb->push(new MoveInst{t, uv->build_val(&ctx)});
                            // TODO: Is this right? why not branch to the next bb earlier to skip the remaining insts?
                            // Due to this, a bb may not ends with a branch/jump/return
                            // Not sure if any passes requires this
                            // Maybe we should simply find the first inst that branches to the current bb
                        } else {
                            // FIXME: after inserting moves, we can't start finding from the back.
                            mips::Inst *br = nullptr;
                            info("phi in bb_%d, arg from bb_%d", ibb->id, u.second->id);
                            for (auto *j = ubb->insts.back; j; j = j->prev) {
                                if_a (mips::ControlInst, y, j) {
                                    info("got br to mbb_%u", y->to->id);
                                    if (y->to == bb)
                                        br = y;
                                } else
                                    break;
                            }
                            asserts(br);
                            ctx.bb = ubb;
                            ctx.push_point = br;
                            ctx.push(new MoveInst{t, uv->build_val(&ctx)});
                            ctx.push_point = nullptr;
                        }
                        */
                    }
                } else
                    break;
            }
        }
    }

    return res;
}
