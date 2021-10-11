#pragma once

#include "ir.hpp"
#include <unordered_map>

#define FOR_IBB FOR_BB
#define FOR_IINST FOR_INST
#define FOR_IBB_IINST FOR_BB_INST

const size_t MAX_ARG_REGS = 4;

namespace mips {

bool is_imm(int x);

struct Inst;
struct LoadInst;
struct BinaryInst;

// Operand is defined in ir.hpp
// Too tiny to put on the heap, so not using an abstract class
typedef Operand Reg;

struct BB : Node<BB> {
    List<Inst> insts;
    vector<BB *> succ;
    std::set<Reg> use, def, live_in, live_out;

    uint id;

    template <class T>
    T *push(T *i) {
        // i->bb = this;
        insts.push(i);
        return i;
    }

    void insert(Inst *i, Inst *before_i) {
        // i->bb = this;
        insts.insert(i, before_i);
    }

    template <class T>
    T *push_front(T *i) {
        // i->bb = this;
        insts.push_front(i);
        return i;
    }
};

#define FOR_MINST(i, bb) FOR_LIST(mips::Inst, i, (bb).insts)

struct Func {
    List<BB> bbs;
    ir::Func *ir;
    uint bb_cnt = 0;
    uint vreg_cnt = 0;
    vector<LoadInst *> arg_loads;
    vector<BinaryInst *> allocas;
    uint max_call_arg_num = 0;
    uint spill_num = 0;  // without 4; spill & allocas (arrays)

    explicit Func(ir::Func *ir);

    BB *new_bb();
    Operand make_vreg();
};

#define FOR_MBB(bb, f) FOR_LIST(mips::BB, bb, (f).bbs)
#define FOR_MBB_MINST(i, bb, f) FOR_MBB(bb, f) FOR_MINST(i, *bb)

#define FMT_PRE "_syc_fmt_"

struct Prog {
    vector<Func> funcs;
    ir::Prog *ir;
    std::unordered_map<string, uint> strs;
    // fmts should be put after globs in generated asm

    explicit Prog(ir::Prog *ir);

    uint find_str(const string &s);

    friend std::ostream &operator << (std::ostream &, const Prog &);
};

/*
Registers for O32 calling convention
Name	Number	Use	Callee must preserve?
$zero	$0	constant 0	N/A
$at	$1	assembler temporary	No
$v0–$v1	$2–$3	values for function returns and expression evaluation	No
$a0–$a3	$4–$7	function arguments	No
$t0–$t7	$8–$15	temporaries	No
$s0–$s7	$16–$23	saved temporaries	Yes
$t8–$t9	$24–$25	temporaries	No
$k0–$k1	$26–$27	reserved for OS kernel	N/A
$gp	$28	global pointer	Yes (except PIC code)
$sp	$29	stack pointer	Yes
$fp	$30	frame pointer	Yes
$ra	$31	return address	N/A
*/

namespace Regs {
    constexpr uint v0 = 2, v1 = 3, a0 = 4, t0 = 8, s0 = 16, t8 = 24, t9 = 25,
        gp = 28, sp = 29, fp = 30, ra = 31;

    constexpr std::array<uint, 16> caller_saved{
        v0, v1,
        a0,  5,  6,  7,
        t0,  9, 10, 11, 12,
        13, 14, 15,
        t8, t9
    };

    constexpr std::array<uint, 8> callee_saved{
        s0, 17, 18, 19, 20,
        21, 22, 23
    };

    constexpr std::array<uint, 24> allocatable{
        v0, v1,
        a0,  5,  6,  7,
        t0,  9, 10, 11, 12,
        13, 14, 15,
        t8, t9,
        s0, 17, 18, 19, 20,
        21, 22, 23
    };

    extern std::array<uint, 32> inv_allocatable;

    string to_name(uint id);

    void init();
}

struct Inst : Node<Inst> {
    virtual void print(std::ostream &) const = 0;
};

struct BinaryInst : Inst {  // add, sub, slt ?
    enum Op {
        Add, Sub, Lt, Ltu, Xor
    } op;
    Reg dst, lhs;
    Operand rhs;  // value range is ignored

    BinaryInst(Op op, Reg dst, Reg lhs, Operand rhs);

    void print(std::ostream &) const override;
};

struct ShiftInst : Inst {
    enum Op {
        Ll, Rl
    } op;
    Reg dst, lhs;
    uint rhs;

    ShiftInst(Op op, Reg dst, Reg lhs, uint rhs);

    void print(std::ostream &) const override;
};

struct MoveInst : Inst {  // move or li
    Reg dst;
    Operand src;

    bool active = false;  // active_moves

    MoveInst(Reg dst, Operand src);

    void print(std::ostream &) const override;
};

struct MultInst : Inst {
    Reg lhs, rhs;

    MultInst(Reg lhs, Reg rhs);

    void print(std::ostream &) const override;
};

struct DivInst : Inst {
    Reg lhs, rhs;

    DivInst(Reg lhs, Reg rhs);

    void print(std::ostream &) const override;
};

struct MFHiInst : Inst {
    Reg dst;

    explicit MFHiInst(Reg dst);

    void print(std::ostream &) const override;
};

struct MFLoInst : Inst {
    Reg dst;

    explicit MFLoInst(Reg dst);

    void print(std::ostream &) const override;
};

struct CallInst : Inst {
    ir::Func *func;

    explicit CallInst(ir::Func *func);

    void print(std::ostream &) const override;
};

struct ControlInst : Inst {
    BB *to;

protected:
    explicit ControlInst(BB *to);
};

struct BranchInst : ControlInst {
    enum Op {
        Eq, Ne
    } op;
    Reg lhs, rhs;

    BranchInst(Op op, Reg lhs, Reg rhs, BB *to);

    void print(std::ostream &) const override;
};

struct BranchZeroInst : ControlInst {
    enum Op {
        Lt, Gt, Le, Ge
    } op;
    Reg lhs;

    void print(std::ostream &) const override;
};

struct JumpInst : ControlInst {
    explicit JumpInst(BB *to);

    void print(std::ostream &) const override;
};

struct ReturnInst : Inst {
    void print(std::ostream &) const override;
};

struct AccessInst : Inst {
    Reg base;
    int off;

    AccessInst(Reg base, int off);
};

struct LoadInst : AccessInst {
    Reg dst;

    LoadInst(Reg dst, Reg base, int off);

    void print(std::ostream &) const override;
};

struct StoreInst : AccessInst {
    Reg src;

    StoreInst(Reg src, Reg base, int off);

    void print(std::ostream &) const override;
};

struct SysInst : Inst {
    uint no;  // 1, 4, 5

    explicit SysInst(uint no);

    void print(std::ostream &) const override;
};

struct LoadStrInst : Inst {
    Reg dst;
    uint id;

    explicit LoadStrInst(Reg dst, uint id);

    void print(std::ostream &) const override;
};

}
