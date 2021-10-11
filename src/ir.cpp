#include "ir.hpp"
#include "common.hpp"

#include <unordered_map>

using namespace ir;

Use::Use(Value *value, Inst *user) : value(value), user(user) {
    // trace("adding use %p to %p", this, value);
    if (value)
        value->add_use(this);
}

Use::Use(const Use &u) : Use(u.value, u.user) {}

Use::Use(Use &&u) noexcept : Use(u.value, u.user) {}

Use::~Use() {
    // debug("destructing Use %p, val = %p", this, value);
    if (value)
        value->kill_use(this);
}

void Use::set(Value *n) {
    if (value)
        value->kill_use(this);
    if ((value = n))
        value->add_use(this);
}

Value *Use::release() {
    value->kill_use(this);
    auto *r = value;
    value = nullptr;
    return r;
}

void Value::add_use(Use *u) {
    // info("adding use %p, val = %p", u, this);
    uses.push(u);
}

void Value::kill_use(Use *u) {
    // info("killing use %p, val = %p", u, this);
    uses.erase(u);
}

void Value::replace(Value *n) const {
    while (uses.front)
        uses.front->set(n);
}

void BB::erase(Inst *i) {
    insts.erase(i);
}

void BB::erase_with(Inst *i, Value *n) {
    i->replace(n);
    insts.erase(i);
}

Prog::Prog(vector<Decl *> &&globals) : globals(globals) {}

Func::Func(bool returns_int, vector<Decl *> &&params, string &&name) :
    returns_int(returns_int), params(params), name(name) {}

BB *Func::new_bb() {
    auto *bb = new BB;
    push_bb(bb);
    return bb;
}

void Func::push_bb(BB *bb) {
    // debug("%s: pushing bb_to %d", name.data(), bb_cnt);
    bb->id = bb_cnt++;
    bbs.push(bb);
}

GetIntFunc::GetIntFunc() : Func(true, {}, "getint?") {}

PrintfFunc::PrintfFunc(const char *fmt, std::size_t len) :
    Func(false, {}, "printf?"), fmt(fmt), len(len) {}

Const::Const(int val) : val(val) {}

Const Const::ZERO{0}, Const::ONE{1};

Const *Const::of(int val) {
    static std::unordered_map<int, Const *> memo = {
        { 0, &Const::ZERO },
        { 1, &Const::ONE }
    };
    auto it = memo.find(val);
    if (it != memo.end())
        return it->second;
    return memo[val] = new Const{val};
}

Global::Global(Decl *var) : var(var) {}

Argument::Argument(Decl *var, uint pos) : var(var), pos(pos) {}

Undef Undef::VAL;

BinaryInst::BinaryInst(OpKind op, Value *lhs, Value *rhs) :
        op(op), lhs(lhs, this), rhs(rhs, this) {}

AccessInst::AccessInst(Decl *lhs, Value *base, Value *idx) :
    lhs(lhs), base(base, this), idx(idx, this) {}

LoadInst::LoadInst(Decl *lhs, Value *base, Value *idx) :
    AccessInst(lhs, base, idx) {}

StoreInst::StoreInst(Decl *lhs, Value *base, Value *idx, Value *val) :
    AccessInst(lhs, base, idx), val(val, this) {}

GEPInst::GEPInst(Decl *lhs, Value *base, Value *idx, int size) :
    AccessInst(lhs, base, idx), size(size) {}

BranchInst::BranchInst(Value *cond, BB *bb_then, BB *bb_else) :
    cond(cond, this), bb_then(bb_then), bb_else(bb_else) {}

JumpInst::JumpInst(BB *bb) : bb_to(bb) {}

ReturnInst::ReturnInst(Value *val) : val(val, this) {}

CallInst::CallInst(Func *func) : func(func) {}

CallInst::CallInst(Func *func, const vector<Value *> &argv) : func(func) {
    args.reserve(argv.size());
    for (auto *arg : argv)
        args.emplace_back(arg, this);
}

AllocaInst::AllocaInst(Decl *var) : var(var) {}

void PhiInst::push(Value *val, BB *bb) {
    vals.emplace_back(Use(val, this), bb);
}

vector<BB *> BB::get_succ() const {
    auto *i = insts.back;
    if_a (BranchInst, x, i)
        return {x->bb_then, x->bb_else};
    if_a (JumpInst, x, i)
        return {x->bb_to};
    if (as_a<ReturnInst>(i))
        return {};
    fatal("bb_%d (%p) ends with non-branch inst %p", id, this, i);
}

bool Inst::is_pure() const {
    return is_a<BinaryInst>(this) || is_a<LoadInst>(this) || is_a<GEPInst>(this)
            || is_a<PhiInst>(this);
}

bool Inst::is_branch() const {
    return is_a<BranchInst>(this) || is_a<JumpInst>(this) || is_a<ReturnInst>(this);
}

int ir::eval_bin(OpKind op, int lh, int rh) {
    using namespace tkd;
    switch (op) {
        case Add: return lh + rh;
        case Sub: return lh - rh;
        case Mul: return lh * rh;
        case Div: return lh / rh;
        case Mod: return lh % rh;
        case Lt:  return lh < rh;
        case Gt:  return lh > rh;
        case Le:  return lh <= rh;
        case Ge:  return lh >= rh;
        case And: return lh && rh;
        case Or:  return lh || rh;
        case Eq:  return lh == rh;
        case Ne:  return lh != rh;
        default:
            fatal("evaluating on unexpected operator %d", op);
    }
}
