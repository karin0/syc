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
    bool is_machine() const;

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

    Use(const Use &);   // vector<Use> requires this
    Use(Use &&) noexcept;

    ~Use();

    void set(Value *n);

    Value *release();

    // friend std::ostream &operator << (std::ostream &, const Use &);
};

struct Value {
    List<Use> uses;

    mips::Operand mach_res = mips::Operand::make_void();

    void add_use(Use *u);
    void kill_use(Use *u);
    void replace(Value *n) const;

    virtual mips::Operand build_val(mips::Builder *) = 0;

    virtual void print_val(std::ostream &) = 0;

    virtual ~Value() = default;
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

    void erase_with(Inst *i, Value *n);

    vector<BB *> get_succ() const;
};

#define FOR_INST(i, bb) FOR_LIST(ir::Inst, i, (bb).insts)

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

#define FOR_BB(bb, f) FOR_LIST(ir::BB, bb, (f).bbs)
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
    int val;

    explicit Const(int val);

    static Const ZERO, ONE;

    static Const *of(int val);

    mips::Operand build_val(mips::Builder *) override;

    void print_val(std::ostream &) override;
};

struct Global : Value {
    Decl *var;

    explicit Global(Decl *var);

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
    BB *bb; // set by BB

    uint id;

    Inst() = default;

    bool is_pure() const;
    bool is_branch() const;

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
    Use base, idx;

    AccessInst(Decl *lhs, Value *base, Value *idx);
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

    void push(Value *val, BB *bb);

    mips::Operand build(mips::Builder *) override;

    void print(std::ostream &) override;
};

int eval_bin(OpKind op, int lhs, int rhs);

}
