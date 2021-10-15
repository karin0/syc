#pragma once

#include "util.hpp"
#include "ast.hpp"

#include <set>

// TODO: introduce size multiplier 4 and li insts before syscall into IR?

using ast::Decl;

namespace mips {

struct BB;
struct Builder;

struct Operand {
    static constexpr const int MAX_CONST = 2147483647;
    static constexpr const int MIN_CONST = -2147483648;

    enum Kind {
        Virtual, Pinned, Machine, Const, Void
    } kind;
    int val;

    Operand();
    Operand(Kind kind, int val);
    explicit Operand(int val);

    static Operand make_virtual(uint id);
    static Operand make_pinned(uint id);
    static Operand make_const(int val);
    static Operand make_void();
    static Operand make_machine(uint id);

    bool is_reg() const;
    bool is_const() const;
    bool is_void() const;
    bool is_imm() const;

    bool is_uncolored() const;
    bool is_pinned() const;
    bool is_virtual() const;
    bool is_physical() const;

    bool equiv(const Operand &rhs) const;

    bool operator < (const Operand &rhs) const;
    bool operator == (const Operand &rhs) const;
    bool operator != (const Operand &rhs) const;

    friend std::ostream &operator << (std::ostream &, const Operand &);
};

}

namespace ir {

using OpKind = tkd::TokenKind;

struct Inst;

struct Use : Node<Use> {
    Value *value;
    Inst *user;

    Use(Value *value, Inst *user);
    Use(Use &&) noexcept;
    ~Use();

    Use &operator = (const Use &) = delete;
    Use &operator = (Use &&) noexcept;  // used by STL

    void set(Value *n);

    Value *release();

    friend std::ostream &operator << (std::ostream &, const Use &);
};

struct Value {
    List<Use> uses;

    mips::Operand mach_res = mips::Operand::make_void();

    void add_use(Use *u);
    void kill_use(Use *u);
    void replace_uses(Value *n);

    virtual mips::Operand build_val(mips::Builder *) = 0;

    virtual void print_val(std::ostream &) = 0;

    virtual ~Value();
};

struct BB : Node<BB> {
    List<Inst> insts;

    bool vis;
    std::set<BB *> dom;
    BB *idom;
    vector<BB *> preds, df;

    mips::BB *mbb;

    int id;

    template <class T>
    T *push(T *i) {
        i->bb = this;
        insts.push(i);
        return i;
    }

    template <class T>
    T *push_front(T *i) {
        i->bb = this;
        insts.push_front(i);
        return i;
    }

    void erase(Inst *i);
    void erase_with(Inst *i, Value *v);
    Inst *get_control() const;
    vector<BB *> get_succ() const;
    // this gives wrong results when multiple control insts are ill-formed,
    // i.e. there are insts after the first control inst in one
    // or after br_induce where BinaryBranchInsts occur
};

#define FOR_INST(i, bb) FOR_LIST(i, (bb).insts)

struct Func {
    bool returns_int;
    vector<Decl *> params;
    List<BB> bbs;
    string name;

    int bb_cnt = 0;

    Func(bool returns_int, vector<Decl *> &&params, string &&name);

    virtual ~Func() = default;

    BB *new_bb();

    void push_bb(BB *bb);
};

#define FOR_BB(bb, f) FOR_LIST(bb, (f).bbs)
#define FOR_BB_INST(i, bb, f) FOR_BB(bb, f) FOR_INST(i, *bb)

struct GetIntFunc : Func {
    GetIntFunc();
};

struct PrintfFunc : Func {
    const char *fmt;
    size_t len;

    PrintfFunc(const char *fmt, size_t len);
};

struct Prog {
    vector<Decl *> globals;
    vector<Func> funcs;
    vector<PrintfFunc> printfs;
    GetIntFunc getint;

    explicit Prog(vector<Decl *> &&globals);

    friend std::ostream &operator << (std::ostream &, const Prog &);
};

struct Const : Value {
    static constexpr const int MAX = mips::Operand::MAX_CONST;
    static constexpr const int MIN = mips::Operand::MIN_CONST;

    int val;

    explicit Const(int val);

    static Const ZERO, ONE;

    static Const *of(int val);

    mips::Operand build_val(mips::Builder *) override;

    void print_val(std::ostream &) override;
};

struct Global : Value {
    const Decl *var;

    explicit Global(const Decl *var);

    mips::Operand build_val(mips::Builder *) override;

    void print_val(std::ostream &) override;
};

struct Argument : Value {
    Decl *var;
    uint pos;

    Argument(Decl *var, uint pos);

    mips::Operand build_val(mips::Builder *) override;

    void print_val(std::ostream &) override;
};

struct Undef : Value {
    static Undef VAL;

    mips::Operand build_val(mips::Builder *) override;

    void print_val(std::ostream &) override;
};

struct Inst : Value, Node<Inst> {
    BB *bb; // must be set by BB after new Inst!

    uint id;

    Inst() = default;

    bool is_pure() const;
    bool is_control() const;

    virtual mips::Operand build(mips::Builder *) = 0;
    mips::Operand build_val(mips::Builder *) override;

    virtual void print(std::ostream &) = 0;

    // friend std::ostream &operator<<(std::ostream &, Inst &);

    void print_val(std::ostream &) override;
};

struct BinaryInst : Inst {
    OpKind op;  // no And / Or
    Use lhs, rhs;

    BinaryInst(OpKind op, Value *lhs, Value *rhs);

    mips::Operand build(mips::Builder *) override;

    void print(std::ostream &) override;
};

struct CallInst : Inst {
    Func *func;
    vector<Use> args;

    explicit CallInst(Func *func);

    CallInst(Func *func, const vector<Value *> &argv);

    mips::Operand build(mips::Builder *) override;

    void print(std::ostream &) override;
};

struct BranchInst : Inst {
    Use cond;
    BB *bb_then, *bb_else;

    BranchInst(Value *cond, BB *bb_then, BB *bb_else);

    mips::Operand build(mips::Builder *) override;

    void print(std::ostream &) override;
};

struct JumpInst : Inst {
    BB *bb_to;

    explicit JumpInst(BB *bb);

    mips::Operand build(mips::Builder *) override;

    void print(std::ostream &) override;
};

struct ReturnInst : Inst {
    Use val; // nullable

    explicit ReturnInst(Value *val);

    mips::Operand build(mips::Builder *) override;

    void print(std::ostream &) override;
};

/*

struct GetIntInst : Inst {
    Decl *lhs;
};

struct PrintfInst : Inst {
    const char *fmt;
    std::size_t len;
    std::vector<Use> args;
};

 */

struct AccessInst : Inst {
    Decl *lhs;
    Use base, off;

    AccessInst(Decl *lhs, Value *base, Value *off);
};

struct LoadInst : AccessInst {
    LoadInst(Decl *lhs, Value *base, Value *idx);

    mips::Operand build(mips::Builder *) override;

    void print(std::ostream &) override;
};

struct StoreInst : AccessInst {
    Use val;

    StoreInst(Decl *lhs, Value *base, Value *idx, Value *val);

    mips::Operand build(mips::Builder *) override;

    void print(std::ostream &) override;
};

struct GEPInst : AccessInst {
    int size;

    GEPInst(Decl *lhs, Value *base, Value *idx, int size);

    mips::Operand build(mips::Builder *) override;

    void print(std::ostream &) override;
};

struct AllocaInst : Inst {
    Decl *var;

    int aid;  // mem2reg

    explicit AllocaInst(Decl *var);

    mips::Operand build(mips::Builder *) override;

    void print(std::ostream &) override;
};

struct PhiInst : Inst {
    vector<std::pair<Use, BB *>> vals;  // TBD

    int aid = -1; // mem2reg

    PhiInst();

    void push(Value *val, BB *bb);

    mips::Operand build(mips::Builder *) override;

    void print(std::ostream &) override;
};

int eval_bin(OpKind op, int lhs, int rhs);

// This should only occur after the conv pass

namespace rel {

enum RelOp {
    Eq = 0, Ne = 1, Lt = 2, Ge = 3, Le = 4, Gt = 5
};

int eval(RelOp, int, int);

}

using rel::RelOp;

struct BinaryBranchInst : Inst {
    using Op = RelOp;
    Op op;
    Use lhs, rhs;
    BB *bb_then, *bb_else;

    BinaryBranchInst(Op op, BinaryInst *old_bin, BranchInst *old_br);  // old must be dropped later

    static Op swap_op(Op op);

    mips::Operand build(mips::Builder *) override;

    void print(std::ostream &) override;
};

}
