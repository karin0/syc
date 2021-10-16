#include "ast.hpp"
#include "common.hpp"
#include "util.hpp"

using namespace ast;

GetInt::GetInt(LVal *l) : lhs(l) {}

Assign::Assign(LVal *l, Expr *r) : lhs(l), rhs(r) {}

ExprStmt::ExprStmt(Expr *e) : val(e) {}

Decl::Decl(bool is_const, const string &name, bool has_init) :
        Symbol(name),
        is_const(is_const), has_init(has_init) {}

uint Decl::size() const {
    if (dims.empty())
        return 1;
    uint r = dims[0];
    asserts(dims.size() <= 2);
    if (dims.size() > 1)
        r *= dims[1];
    return r;
}

Symbol::Symbol(string name) : name(std::move(name)) {}

ast::Func::Func(const string &name, bool returns_int, Block body) :
    Symbol(name), returns_int(returns_int), body(std::move(body)) {}

ast::Func::Func(const string &name, bool returns_int) :
    Symbol(name), returns_int(returns_int) {}

Number::Number(int v) : val(v) {}

Binary::Binary(TokenKind op, Expr *lh, Expr *rh):
    op(op), lhs(lh), rhs(rh) {}

int Expr::eval() {
    unreachable();
}

int ast::evals(Expr **e) {
    if_a (Number, x, *e)
        return x->val;
    else {
        int v = (*e)->eval();
        *e = new Number{v};
        return v;
    }
}

int Binary::eval() { // no or, and, eq here
    int lh = evals(&lhs), rh = evals(&rhs);
    using namespace tkd;
    switch (op) {
        case Add: return lh + rh;
        case Sub: return lh - rh;
        case Mul: return lh * rh;
        case Div: return lh / rh;
        case Mod: return lh % rh;
        default:
            fatal("evaluating on unexpected operator %d", op);
    }
}

int Number::eval() {
    return val;
}

int LVal::eval() {
    if (!var->is_const)
        fatal("evaluating on non-const variable %s", var->name.data());
    auto vds = var->dims.size(), ds = dims.size();
    if (vds != ds)
        fatal("mismatched index dims: %zu, %zu on %s", vds, ds, var->name.data());
    if (var->dims.empty()) {
        if (var->init.empty())
            fatal("uninitialized const variable %s ??", var->name.data());
        return evals(&var->init.front());
    } else {
        std::size_t idx = dims.back()->eval();
        if (ds > 1)
            idx += dims[0]->eval() * var->dims[1];
        if (idx >= var->init.size())
            fatal("index %d overflows", idx);
        return evals(&var->init[idx]);
    }
}
