#include "ir.hpp"
#include "common.hpp"

#include <unordered_map>

using namespace ir;

Use::Use(Value *value, Inst *user) : value(value), user(user) {
    // trace("adding Use %p to val %p", this, value);
    if (value)
        value->add_use(this);
}

Use::Use(Use &&u) noexcept : Use(u.value, u.user) {}

Use::~Use() {
    // trace("destructing Use %p, val = %p", this, value);
    if (value)
        value->kill_use(this);
}

Use &Use::operator = (Use &&u) noexcept {
    user = u.user;
    set(u.value);
    return *this;
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

bool ir::no_value_check;

Value::~Value() {
    if (!ir::no_value_check)
        asserts(uses.empty());
    // asserts(uses.empty()); // this prevents final collecting
    // replace_uses(nullptr);
    // FOR_LIST (u, uses)
    //     u->value = nullptr;  // guard
}

void Value::add_use(Use *u) {
    uses.push(u);
}

void Value::kill_use(Use *u) {
    uses.erase(u);
}

void Value::replace_uses(Value *n) {
    asserts(n != this);
    while (uses.front)
        uses.front->set(n);
}

void BB::erase(Inst *i) {
    insts.erase(i);
}

void BB::erase_with(Inst *i, Value *v) {
    i->replace_uses(v);
    insts.erase(i);
}

Inst *BB::get_control() const {
    auto *i = insts.back;
    asserts(i->is_control());
    return i;
}

vector<BB *> BB::get_succ() const {
    auto *i = get_control();
    if_a (BranchInst, x, i)
        return {x->bb_then, x->bb_else};
    if_a (BinaryBranchInst, x, i)
        return {x->bb_then, x->bb_else};
    if_a (JumpInst, x, i)
        return {x->bb_to};
    if (as_a<ReturnInst>(i))
        return {};
    unreachable();
}

vector<BB **> BB::get_succ_mut() const {
    auto *i = get_control();
    if_a (BranchInst, x, i)
        return {&x->bb_then, &x->bb_else};
    if_a (BinaryBranchInst, x, i)
        return {&x->bb_then, &x->bb_else};
    if_a (JumpInst, x, i)
        return {&x->bb_to};
    if (as_a<ReturnInst>(i))
        return {};
    unreachable();
}

Prog::Prog(vector<Decl *> &&globals) : globals(globals) {}

Func::Func(bool returns_int, const char *name) :
    returns_int(returns_int), name(name) {
    has_side_effects = true;
    has_global_loads = true;
    has_param_loads = false;
    is_pure = false;
}

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
    bb->func = this;
    bbs.push(bb);
}

GetIntFunc::GetIntFunc() : Func(true, "getint") {}
PrintfFunc::PrintfFunc(const char *fmt, std::size_t len) :
    Func(false, "printf"), fmt(fmt), len(len) {}


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

Global::Global(const Decl *var) : var(var) {}

Argument::Argument(Decl *var, uint pos) : var(var), pos(pos) {}

Undef Undef::VAL;

BinaryInst::BinaryInst(OpKind op, Value *lhs, Value *rhs) :
        op(op), lhs(lhs, this), rhs(rhs, this) {}

bool BinaryInst::is_op_mirror(OpKind a, OpKind b) {
    using namespace tkd;
    switch (a) {
        case Add:
        case Mul:
        case Eq:
        case Ne:
            return a == b;
        case Lt: return b == Gt;
        case Gt: return b == Lt;
        case Le: return b == Ge;
        case Ge: return b == Le;
        default:
            return false;
    }
}

MemInst::MemInst(Decl *lhs, Value *base, Value *off) :
    lhs(lhs), base(base, this), off(off, this) {}

LoadInst::LoadInst(Decl *lhs, Value *base, Value *off) :
    MemInst(lhs, base, off) {}

StoreInst::StoreInst(Decl *lhs, Value *base, Value *off, Value *val) :
    MemInst(lhs, base, off), val(val, this) {}

GEPInst::GEPInst(Decl *lhs, Value *base, Value *off, int size) :
    MemInst(lhs, base, off), size(size) {}

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

PhiInst::PhiInst() {
    vals.reserve(2);
}

BinaryBranchInst::BinaryBranchInst(Op op, BinaryInst *old_bin, BranchInst *old_br) : op(op),
    lhs(old_bin->lhs.value, this), rhs(old_bin->rhs.value, this),
    bb_then(old_br->bb_then), bb_else(old_br->bb_else) {}

RelOp BinaryBranchInst::swap_op(Op op) {
    using namespace rel;
    switch (op) {
        case Eq: case Ne: return op;
        case Lt: return Gt;
        case Le: return Ge;
        case Gt: return Lt;
        case Ge: return Le;
        default:
            unreachable();
    }
}

void PhiInst::push(Value *val, BB *bb) {
    vals.emplace_back(Use{val, this}, bb);
}

// impure Call, Control, Store
bool Inst::has_side_effects() const {
    if_a (const CallInst, x, this)
        return x->func->has_side_effects;
    return !(is_a<BinaryInst>(this) || is_a<LoadInst>(this) || is_a<GEPInst>(this)
            || is_a<PhiInst>(this) || is_a<AllocaInst>(this));
}

bool Inst::is_control() const {
    return is_a<BranchInst>(this) || is_a<JumpInst>(this) || is_a<ReturnInst>(this)
            || is_a<BinaryBranchInst>(this);
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
            unreachable();
    }
}

int rel::eval(RelOp op, int lh, int rh) {
    switch (op) {
        case Lt: return lh < rh;
        case Gt: return lh > rh;
        case Le: return lh <= rh;
        case Ge: return lh >= rh;
        case Eq: return lh == rh;
        case Ne: return lh != rh;
        default:
            unreachable();
    }
}
