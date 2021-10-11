#include "mips.hpp"

namespace mips {

const Func *func_now;

std::ostream &operator << (std::ostream &os, const BB &bb) {
    if (func_now)
        os << "_" << func_now->ir->name;
    os << "_bb_" << bb.id;
    return os;
}

std::ostream &operator << (std::ostream &os, const Operand &x) {
    switch (x.kind) {
        case Operand::Virtual:
            os << "V";
            break;
        case Operand::Pinned:
            os << 'P' << Regs::to_name(x.val);
            return os;
        case Operand::Machine:
            os << '$' << Regs::to_name(x.val);
            return os;
        case Operand::Const:
            break;
        case Operand::Void:
            fatal("found used void operand");
    }
    os << x.val;
    return os;
}

#define STR_PRE "__str_"
#define INDENT "    "

std::ostream &operator << (std::ostream &os, const Prog &prog) {
    os << ".data\n";
    for (auto &g: prog.ir->globals) {
        os << INDENT << g->name << ": ";
        if (g->has_init) {
            os << ".word";
            for (auto x: g->init) {
                if_a (ast::Number, n, x)
                    os << ' ' << n->val;
                else
                    fatal("unevaluated global initializer");
            }
            os << '\n';
        } else
            os << ".space " << (g->size() << 2) << '\n';
    }
    os << '\n';

    uint n = prog.strs.size();
    auto *strs = new const string *[n];
    for (auto &p: prog.strs)
        strs[p.second] = &p.first;

    for (uint i = 0; i < n; ++i)
        os << INDENT STR_PRE << i << ": .asciiz \"" << *strs[i] << "\"\n";
    delete []strs;

    os << "\n.text\n";
    for (auto &f : prog.funcs) {
        func_now = &f;
        os << f.ir->name << ":\n";
        FOR_MBB (bb, f) {
            os << *bb << ":\n";
            FOR_MINST (i, *bb) {
                os << INDENT;
                i->print(os);
                os << '\n';
            }
        }
    }
    return os;
}

const char *bin_name_r(BinaryInst::Op op) {
    switch (op) {
        case BinaryInst::Add:
            return "addu";
        case BinaryInst::Sub:
            return "subu";
        case BinaryInst::Lt:
            return "slt";
        case BinaryInst::Ltu:
            return "sltu";
        case BinaryInst::Xor:
            return "xor";
        default:
            fatal("unknown binary r-inst");
    }
}

const char *bin_name_i(BinaryInst::Op op) {
    switch (op) {
        case BinaryInst::Add:
            return "addiu";
        case BinaryInst::Lt:
            return "slti";
        case BinaryInst::Ltu:
            return "sltiu";
        case BinaryInst::Xor:
            return "xori";
        default:
            fatal("unknown binary i-inst");
    }
}

void BinaryInst::print(std::ostream &os) const {
    asserts(lhs.is_reg());
    asserts(dst.is_reg());
    if (rhs.kind == Operand::Const) {
        asserts(rhs.is_imm());
        os << bin_name_i(op);
    } else
        os << bin_name_r(op);
    os << ' ' << dst << ", " << lhs << ", " << rhs;
}

void ShiftInst::print(std::ostream &os) const {
    asserts(lhs.is_reg());
    asserts(dst.is_reg());
    asserts(rhs < 32);
    switch (op) {
        case Ll:
            os << "sll ";
            break;
        case Rl:
            os << "srl ";
            break;
    }
    os << dst << ", " << lhs << ", " << rhs;
}

void MoveInst::print(std::ostream &os) const {
    // TODO: pseudo insts are used, hence use of $at is forbidden
    asserts(dst.is_reg());
    if (src.is_const())
        os << "li ";
    else
        os << "move ";
    os << dst << ", " << src;
}

void MultInst::print(std::ostream &os) const {
    asserts(lhs.is_reg());
    asserts(rhs.is_reg());
    os << "mult " << lhs << ", " << rhs;
}

void DivInst::print(std::ostream &os) const {
    asserts(lhs.is_reg());
    asserts(rhs.is_reg());
    os << "div " << lhs << ", " << rhs;
}

void MFHiInst::print(std::ostream &os) const {
    asserts(dst.is_reg());
    os << "mfhi " << dst;
}

void MFLoInst::print(std::ostream &os) const {
    asserts(dst.is_reg());
    os << "mflo " << dst;
}

void CallInst::print(std::ostream &os) const {
    os << "jal " << func->name;
}

void BranchInst::print(std::ostream &os) const {
    asserts(lhs.is_reg());
    asserts(rhs.is_reg());
    switch (op) {
        case Eq:
            os << "beq ";
            break;
        case Ne:
            os << "bne ";
            break;
    }
    os << lhs << ", " << rhs << ", " << *to;
}

void BranchZeroInst::print(std::ostream &os) const {
    asserts(lhs.is_reg());
    switch (op) {
        case Lt:
            os << "bltz ";
            break;
        case Gt:
            os << "bgtz ";
            break;
        case Le:
            os << "blez ";
            break;
        case Ge:
            os << "bgez ";
            break;
    }
    os << lhs << ", " << *to;
}

void JumpInst::print(std::ostream &os) const {
    os << "j " << *to;
}

void ReturnInst::print(std::ostream &os) const {
    os << "jr $ra";
}

void LoadInst::print(std::ostream &os) const {
    asserts(dst.is_reg());
    asserts(base.is_reg());
    // asserts(is_imm(off));  // MARS does the trick with at
    os << "lw " << dst << ", " << off << '(' << base << ')';
}

void StoreInst::print(std::ostream &os) const {
    asserts(src.is_reg());
    asserts(base.is_reg());
    // asserts(is_imm(off));
    os << "sw " << src << ", " << off << '(' << base << ')';
}

void SysInst::print(std::ostream &os) const {
    os << "syscall";
}

SysInst::SysInst(uint no) : no(no) {}

void LoadStrInst::print(std::ostream &os) const {
    asserts(dst.is_reg());
    os << "la " << dst << ", " STR_PRE << id;
}

}
