#include "ir_builder.hpp"
#include "common.hpp"

using namespace ir;

namespace ir {

struct Builder {
    Func *func;
    BB *bb;
    GetIntFunc *getint;
    bool is_main;

    vector<std::pair<BB *, BB *>> loops;

    explicit Builder(GetIntFunc *gi) : getint(gi) {}

    void init(Func *func, BB *bb) {
        this->func = func;
        this->bb = bb;
        is_main = func->name == "main";
        asserts(loops.empty());
    }

    template <class T>
    T *push(T *i) const {
        return bb->push(i);
    }

    BB *new_bb() {
        return bb = func->new_bb();
    }

    void push_bb(BB *bb) {
        func->push_bb(bb);
        this->bb = bb;
    }

    /* void seek(BB *bb) {
        this->bb = bb;
    } */

    void push_loop(BB *bb_break, BB *bb_continue) {
        loops.emplace_back(bb_break, bb_continue);
    }

    void pop_loop() {
        loops.pop_back();
    }

    const std::pair<BB *, BB *> &get_loop() {
        return loops.back();
    }
};

}

Value *build_2x(Value *v, Builder *ctx) {
    if_a (Const, x, v)
        return Const::of(x->val << 2);
    else
        return ctx->push(new BinaryInst{tkd::Mul, v, Const::of(4)});
}

GEPInst *unfold(Decl *lhs, vector<int> &dims, vector<ast::Expr *> &idx, Builder *ctx) {
    return ctx->push(new GEPInst{lhs, lhs->value, build_2x(idx.front()->build(ctx), ctx), dims.back()});
}

// only for idx.size() == dims.size()
std::pair<Value *, Value *> resolve_idx(
    Decl *lhs, vector<int> &dims, vector<ast::Expr *> &idx, Builder *ctx
) {
    asserts(idx.size() == dims.size());
    if (idx.empty())
        return {lhs->value, &Const::ZERO};

    auto *base = idx.size() > 1 ? unfold(lhs, dims, idx, ctx) : lhs->value;
    return {base, build_2x(idx.back()->build(ctx), ctx)};
}

// idx.size() < dims.size() only when LVal
// TODO: const promote
Value *ast::LVal::build(Builder *ctx) {
    auto &idx = dims;
    auto &dims = var->dims;
    // TODO: handle const

    if (idx.size() == dims.size()) {
        auto res = resolve_idx(var, dims, idx, ctx);
        return ctx->push(new LoadInst{var, res.first, res.second});
    }
    asserts(idx.size() < dims.size());
    asserts(dims.size() <= 2);

    if (idx.empty())
        return var->value; // TODO: keep dependency info
    asserts(idx.size() == 1 && dims.size() == 2);
    return unfold(var, dims, idx, ctx);
}

Value *ast::Number::build(Builder *) {
    return Const::of(val);
}

Value *ast::Binary::build(Builder *ctx) {
    auto *lh = lhs->build(ctx);

    if (op == tkd::And || op == tkd::Or) {
        /*
     And:
         bb:
         lh := lhs
         br lh ? crh : end

         crh:
         rh := rhs
         j end

         end:
         res := phi(bb: 0, crh: rh)
     Or:
         bb:
         lh := lhs
         br lh ? end : crh

         crh:
         rh := rhs
         j end

         end:
         res := phi(bb: 1, crh: rh)
         */
        auto *bb = ctx->bb;
        auto *bb_rh = ctx->new_bb();
        auto *rh = rhs->build(ctx);
        auto *bb_rh_end = ctx->bb;
        auto *bb_end = ctx->new_bb();
        bb_rh_end->push(new JumpInst{bb_end});
        auto *phi = bb_end->push(new PhiInst{});
        if (op == tkd::And) {
            bb->push(new BranchInst{lh, bb_rh, bb_end});
            phi->push(&Const::ZERO, bb);
        } else {
            bb->push(new BranchInst{lh, bb_end, bb_rh});
            phi->push(&Const::ONE, bb);
        }
        phi->push(rh, bb_rh_end);
        return phi;
    }

    auto *rh = rhs->build(ctx);
    if (auto *l = as_a<Const>(lh))
        if (auto *r = as_a<Const>(rh))
            return Const::of(eval_bin(op, l->val, r->val));

#define CHK(xh, v, res) \
        if (auto *x = as_a<Const>(xh)) { \
            if (x->val == (v))           \
                return (res);            \
        }

    // regardless of side effects
    if (op == tkd::Add) {
        CHK(lh, 0, rh)
        else CHK(rh, 0, lh);
    } else if (op == tkd::Sub) {
        CHK(rh, 0, lh);
    } else if (op == tkd::Mul) {
        if (auto *x = as_a<Const>(lh)) {
            if (x->val == 0)
                return &Const::ZERO;
            if (x->val == 1)
                return rh;
        } else if (auto *x = as_a<Const>(rh)) {
            if (x->val == 0)
                return &Const::ZERO;
            if (x->val == 1)
                return lh;
        }
    } else if (op == tkd::Div) {
        CHK(lh, 0, &Const::ZERO)
        else CHK(rh, 1, lh);
    } else if (op == tkd::Mod) {
        CHK(lh, 0, &Const::ZERO)
        else CHK(rh, 1, &Const::ZERO); // TODO: right?
    }

    return ctx->push(new BinaryInst{op, lh, rh});
}

CallInst *build_call(Func *func, const vector<ast::Expr *> &args, Builder *ctx) {
    vector<Value *> argv;
    argv.reserve(args.size());
    for (auto *arg: args)
        argv.push_back(arg->build(ctx));
    return ctx->push(new CallInst{func, argv});
}

Value *ast::Call::build(Builder *ctx) {
    return build_call(func->ir, args, ctx);
}

void build_assign(Builder *ctx, ast::LVal *lhs, Value *rhs) {
    auto *var = lhs->var;
    debug("assigning %zu dims to %s (%zu dims)", lhs->dims.size(), var->name.data(), var->dims.size());
    auto res = resolve_idx(var, var->dims, lhs->dims, ctx);
    ctx->push(new StoreInst{var, res.first, res.second, rhs});
}

void ast::Assign::build(Builder *ctx) {
    build_assign(ctx, lhs, rhs->build(ctx));
}

void ast::DeclStmt::build(Builder *ctx) {
    for (auto *var: vars) {
        auto *alloca = var->value = ctx->push(new AllocaInst{var});
        if (var->has_init) {
            auto &init = var->init;
            if (var->dims.empty())
                ctx->push(new StoreInst{var, alloca, &Const::ZERO, init.front()->build(ctx)});
            else {
                for (uint i = 0; i < init.size(); ++i)
                    ctx->push(new StoreInst{var, alloca, Const::of(int(i << 2)), init[i]->build(ctx)});
            }
        }
    }
}

void ast::ExprStmt::build(Builder *ctx) {
    val->build(ctx);
}

void ast::Dummy::build(Builder *) {
}

void ast::Block::build(Builder *ctx) {
    using namespace ast;
    for (auto *st : stmts) {
        st->build(ctx);
        if (as_a<Break>(st) || as_a<Continue>(st) || as_a<Return>(st))
            break;
    }
}

void ast::If::build(Builder *ctx) {
    auto *con = cond->build(ctx);

    BB *bb = ctx->bb;
    BB *bb_then = ctx->new_bb();
    body_then->build(ctx);
    BB *bb_then_end = ctx->bb;

    if (body_else) {
        BB *bb_else = ctx->new_bb();
        body_else->build(ctx);
        BB *bb_else_end = ctx->bb;
        bb->push(new BranchInst{con, bb_then, bb_else});

        BB *bb_end = ctx->new_bb();
        bb_then_end->push(new JumpInst{bb_end});
        bb_else_end->push(new JumpInst{bb_end});
    } else {
        BB *bb_end = ctx->new_bb();
        bb_then_end->push(new JumpInst{bb_end});
        bb->push(new BranchInst{con, bb_then, bb_end});
    }
}

void ast::While::build(Builder *ctx) {
    /*
     br cond, loop, end

     loop:
        body

        cont:
        br cond, loop, end

     end:
     */
    auto *con = cond->build(ctx);
    BB *bb = ctx->bb;

    BB *bb_loop = ctx->new_bb();
    BB *bb_cont = new BB;
    BB *bb_end = new BB;

    ctx->push_loop(bb_end, bb_cont);
    body->build(ctx);
    ctx->pop_loop();
    BB *bb_loop_end = ctx->bb;

    ctx->push_bb(bb_cont);
    auto con2 = cond->build(ctx);
    BB *bb_cont_end = ctx->bb;
    ctx->push_bb(bb_end);

    bb->push(new BranchInst{con, bb_loop, bb_end});
    bb_loop_end->push(new JumpInst{bb_cont});
    bb_cont_end->push(new BranchInst{con2, bb_loop, bb_end});
}

void ast::Break::build(Builder *ctx) {
    ctx->push(new JumpInst{ctx->get_loop().first});
}

void ast::Continue::build(Builder *ctx) {
    ctx->push(new JumpInst{ctx->get_loop().second});
}

void ast::Return::build(Builder *ctx) {
    auto *v = val ? val->build(ctx) : nullptr;
    ctx->push(new ReturnInst{ctx->is_main ? nullptr : v});
}

void ast::GetInt::build(Builder *ctx) {
    build_assign(ctx, lhs, ctx->push(new CallInst{ctx->getint}));
}

void ast::Printf::build(Builder *ctx) {
    build_call(func, args, ctx);
}

Prog build_ir(ast::Prog &&ast) {
    Prog res{std::move(ast.globals)};

    for (auto *global : res.globals)
        global->value = new Global{global};

    res.funcs.reserve(ast.funcs.size());
    for (auto *fun: ast.funcs)
        res.funcs.emplace_back(fun->returns_int, std::move(fun->params), std::move(fun->name));

    std::size_t i = 0;
    for (auto *fun: ast.funcs)
        fun->ir = &res.funcs[i++];

    res.printfs.reserve(ast.printfs.size());
    for (auto *pr: ast.printfs)
        res.printfs.emplace_back(pr->fmt, pr->len);

    i = 0;
    for (auto *pr: ast.printfs)
        pr->func = &res.printfs[i++];

    Builder ctx{&res.getint};
    ctx.getint = &res.getint;
    for (auto *fun: ast.funcs) {
        auto *func = fun->ir;
        auto *bb = func->new_bb();

        uint pos = 0;
        for (auto *param: func->params) {
            auto *arg = new Argument{param, pos++};
            if (param->dims.empty()) {
                auto *alloca = param->value = bb->push(new AllocaInst{param});
                bb->push(new StoreInst{param, alloca, &Const::ZERO, arg});
            } else
                param->value = arg; // Array names as parameters will never be assigned to
        }

        ctx.init(func, bb);
        if (ctx.is_main)
            func->returns_int = false;
        fun->body.build(&ctx);

        auto *bb_last = func->bbs.back;
        auto *last = bb_last->insts.back;
        if (!last || !last->is_control())
            bb_last->push(new ReturnInst{nullptr});
    }
    return res;
}
