#include "mips.hpp"
#include "ir.hpp"


namespace mips {

namespace Regs {

string to_name(uint i) {
    if (i <= 1) {
        return std::to_string(i);
    }
    int p;
    if (i <= 3)
        p = 'v', i -= 2;
    else if (i <= 7)
        p = 'a', i -= 4;
    else if (i <= 15)
        p = 't', i -= 8;
    else if (i <= 23)
        p = 's', i -= 16;
    else if (i <= 25)
        p = 't', i = 8 + i - 24;
    else if (i <= 27)
        p = 'k', i -= 26;
    else if (i == 28)
        return "gp";
    else if (i == 29)
        return "sp";
    else if (i == 30)
        return "fp";
    else if (i == 31)
        return "ra";
    else
        unreachable();
    string r;
    r.push_back(p);
    r.push_back('0' + int(i));
    return r;
}

std::array<uint, 32> inv_allocatable;

void init() {
    std::fill(inv_allocatable.begin(), inv_allocatable.end(), -1u);
    uint n = allocatable.size();
    for (uint i = 0; i < n; ++i)
        inv_allocatable[allocatable[i]] = i;
}

bool is_s(Reg r) {
    return r.is_physical() && uint(r.val) >= s0 && uint(r.val) <= s7;
}

}

bool is_imm(int x) {
    return -32768 <= x && x <= 32767;
}

Func::Func(ir::Func *ir) : ir(ir), is_main(ir->name == "main") {}

BB *Func::new_bb() {
    auto *bb = new BB;
    bb->id = bb_cnt++;
    bbs.push(bb);
    return bb;
}

Operand Func::make_vreg() {
    return Operand::make_virtual(vreg_cnt++);
}

Prog::Prog(ir::Prog *ir) : ir(ir) {}

uint Prog::find_str(const string &s) {
    auto it = strs.find(s);
    if (it != strs.end())
        return it->second;
    uint res = strs.size();
    strs[s] = res;
    return res;
}

Operand::Operand() : kind(Operand::Void), val(0) {}

Operand::Operand(Kind kind, int val) : kind(kind), val(val) {}

Operand::Operand(int val) : kind(Kind::Const), val(val) {}

Operand Operand::make_const(int v) {
    return Operand{v};
}

Operand Operand::make_virtual(uint id) {
    return Operand{Kind::Virtual, int(id)};
}

Operand Operand::make_pinned(uint id) {
    return Operand{Kind::Pinned, int(id)};
}

Operand Operand::make_void() {
    return Operand{Kind::Void, 0};
}

Operand Operand::make_machine(uint id) {
    return Operand{Kind::Machine, int(id)};
}

bool Operand::is_reg() const {
    return !is_const() && !is_void();
}

bool Operand::is_const() const {
    return kind == Const;
}

bool Operand::is_void() const {
    return kind == Void;
}

bool Operand::is_imm() const {
    return is_const() && mips::is_imm(val);
}

bool Operand::is_uncolored() const {
    return kind == Virtual || kind == Pinned;
}

bool Operand::is_pinned() const {
    return kind == Pinned;
}

bool Operand::is_virtual() const {
    return kind == Virtual;
}

bool Operand::is_physical() const {
    return kind == Machine || kind == Pinned;
}

bool Operand::equiv(const Operand &rhs) const {
    return (is_physical() && rhs.is_physical() && val == rhs.val) ||
        (is_virtual() && rhs.is_virtual() && val == rhs.val);
}

bool Operand::operator < (const Operand &rhs) const {
    if (kind != rhs.kind)
        return kind < rhs.kind;
    return val < rhs.val;
}

bool Operand::operator == (const Operand &rhs) const {
    return kind == rhs.kind && val == rhs.val;
}

bool Operand::operator != (const Operand &rhs) const {
    return !(*this == rhs);
}

BinaryInst::BinaryInst(BinaryInst::Op op, Reg dst, Reg lhs, Operand rhs) :
    op(op), dst(dst), lhs(lhs), rhs(rhs) {}

ShiftInst::ShiftInst(Op op, Reg dst, Reg lhs, uint rhs) : op(op), dst(dst), lhs(lhs), rhs(rhs) {}

MoveInst::MoveInst(Reg dst, Operand src) : dst(dst), src(src) {}

MultInst::MultInst(Reg lhs, Reg rhs) : lhs(lhs), rhs(rhs) {}

DivInst::DivInst(Reg lhs, Reg rhs) : lhs(lhs), rhs(rhs) {}

MFHiInst::MFHiInst(Reg dst) : dst(dst) {}

MFLoInst::MFLoInst(Reg dst) : dst(dst) {}

CallInst::CallInst(ir::Func *func) : func(func) {}

ControlInst::ControlInst(BB *to) : to(to) {}

BranchInst::BranchInst(BranchInst::Op op, Reg lhs, Reg rhs, BB *to) :
    ControlInst(to), op(op), lhs(lhs), rhs(rhs) {}

JumpInst::JumpInst(BB *to) : ControlInst(to) {}

AccessInst::AccessInst(Reg base, int off) : base(base), off(off) {}

LoadInst::LoadInst(Reg dst, Reg base, int off) : AccessInst(base, off), dst(dst) {}

StoreInst::StoreInst(Reg src, Reg base, int off) : AccessInst(base, off), src(src) {}

LoadStrInst::LoadStrInst(Reg dst, uint id) : dst(dst), id(id) {}

}
