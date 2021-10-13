#include "mips.hpp"
#include <ostream>

namespace mips {

const Func *func_now;
const uint *str_addr;

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

#define END_LABEL "__END"
#define GLOB_PRE "__GLO_"
#define STR_PRE "__STR_"
#define FUNC_PRE "__FUN_"
#define INDENT "    "

static void put_li(std::ostream &os, Reg dst, int src) {
    if (src & 0xffff)
        os << "li " << dst << ", " << src;
    else
        os << "lui " << dst << ", " << (src >> 16);
}

std::ostream &operator << (std::ostream &os, const Prog &prog) {
    os << ".data\n";
    for (auto &g: prog.ir->globals) {
        os << INDENT GLOB_PRE << g->name << ": ";
        if (g->has_init) {
            os << ".word";
            if (!(g->init.empty() || g->init.size() == g->size()))
                fatal("illegal initializer list for %s", g->name.data());
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
    auto *addrs = new uint[n];
    for (auto &p: prog.strs)
        strs[p.second] = &p.first;
    uint data = prog.str_base_addr;
    for (uint i = 0; i < n; ++i) {
        addrs[i] = data;
        data += strs[i]->size() + 1 - std::count(strs[i]->begin(), strs[i]->end(), '\\');
    }
    for (uint i = 0; i < n; ++i)
        os << INDENT STR_PRE << i << ": .asciiz \"" << *strs[i] << "\"\n";
    delete []strs;
    str_addr = addrs;

    os << "\n.text\n";

    for (auto &f : prog.funcs) if (f.is_main) {
        func_now = &f;
        os << FUNC_PRE "main:\n";
        if (prog.gp_used) {
            os << INDENT;
            put_li(os, Reg::make_machine(Regs::gp), DATA_BASE);
            os << '\n';
        }
        FOR_BB (bb, f) {
            os << *bb << ":\n";
            FOR_INST (i, *bb) {
                os << INDENT;
                if (is_a<ReturnInst>(i)) {
                    if (i->next || bb->next || prog.funcs.size() > 1)
                        os << "j " END_LABEL;
                } else
                    i->print(os);
                os << '\n';
            }
        }
        break;
    }

    for (auto &f : prog.funcs) if (!f.is_main) {
        func_now = &f;
        os << FUNC_PRE << f.ir->name << ":\n";
        FOR_BB (bb, f) {
            os << *bb << ":\n";
            FOR_INST (i, *bb) {
                os << INDENT;
                i->print(os);
                os << '\n';
            }
        }
    }
    os << END_LABEL << ":\n";
    func_now = nullptr;
    str_addr = nullptr;
    delete []addrs;

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
            unreachable();
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
            unreachable();
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
        default:
            unreachable();
    }
    os << dst << ", " << lhs << ", " << rhs;
}

void MoveInst::print(std::ostream &os) const {
    // TODO: pseudo insts are used, hence use of $at is forbidden
    asserts(dst.is_reg());
    if (src.is_const()) {
        // TODO: li is not implemented optimally, better do it ourselves
        put_li(os, dst, src.val);
        return;
    }
    os << "move " << dst << ", " << src;
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
    os << "jal " FUNC_PRE << func->name;
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
        default:
            unreachable();
    }
    os << lhs << ", " << rhs << ", " << *to;
}

void BranchZeroInst::print(std::ostream &os) const {
    asserts(lhs.is_reg());
    switch (op) {
        case Op::Eq:
            os << "beq " << lhs << ", $0, " << *to;
            return;
        case Op::Ne:
            os << "bne " << lhs << ", $0, " << *to;
            return;
        case Op::Lt:
            os << "bltz ";
            break;
        case Op::Gt:
            os << "bgtz ";
            break;
        case Op::Le:
            os << "blez ";
            break;
        case Op::Ge:
            os << "bgez ";
            break;
        default:
            unreachable();
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
    // TODO: la is not optimal too
    asserts(dst.is_reg());
    if (str_addr)
        put_li(os, dst, int(str_addr[id]));
    else
        os << "la " << dst << ", " STR_PRE << id;
}

}
