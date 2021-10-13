#include "ir.hpp"
#include <ostream>

#define INST_PRE "$"

namespace ir {

std::ostream &operator << (std::ostream &os, const Use &u) {
    if (u.value)
        u.value->print_val(os);
    else
        os << "null";
    return os;
}

std::ostream &operator << (std::ostream &os, Inst &i) {
    if (!i.uses.empty())
        os << INST_PRE << i.id << " := ";
    i.print(os);
    return os;
}


static std::ostream &operator << (std::ostream &os, const Decl &var) {
    os << var.name << '[';
    if (var.is_const)
        os << "const, ";
    for (auto x : var.dims)
        os << x << ", ";
    os << ']';
    if (var.has_init) {
        os << " { ";
        for (auto *e: var.init) {
            if (auto *x = as_a<ast::Number>(e))
                os << x->val << 'c';
            else
                os << "?";
            os << ", ";
        }
        os << " }";
    }
    return os;
}

void Const::print_val(std::ostream &os) {
    os << val;
}

void Global::print_val(std::ostream &os) {
    os << "glob_" << var->name;
}

void Argument::print_val(std::ostream &os) {
    os << "arg_" << var->name;
}

void Undef::print_val(std::ostream &os) {
    os << "undef??";
}

void Inst::print_val(std::ostream &os) {
    os << INST_PRE << id;
}

void BinaryInst::print(std::ostream &os) {
    os << "bin_" << kind_name(op) << ' ' << lhs << ", " << rhs;
}

void CallInst::print(std::ostream &os) {
    os << "call " << func->name << " :";
    for (auto &u: args)
        os << ' ' << u;
}

void BranchInst::print(std::ostream &os) {
    os << "br " << cond << " ? bb_" << bb_then->id << " : bb_" << bb_else->id;
}

void JumpInst::print(std::ostream &os) {
    os << "j bb_" << bb_to->id;
}

void ReturnInst::print(std::ostream &os) {
    os << "ret " << val;
}

void LoadInst::print(std::ostream &os) {
    os << "load " << base;
    if (off.value != &Const::ZERO)
        os << " @ " << off;
    os << "  # " << lhs->name;
}

void StoreInst::print(std::ostream &os) {
    os << "store " << val << ", " << base;
    if (off.value != &Const::ZERO)
        os << " @ " << off;
    os << "  # " << lhs->name;
}

void GEPInst::print(std::ostream &os) {
    os << "gep " << base << " @ " << off << " * " << size << "  # " << lhs->name;
}

void AllocaInst::print(std::ostream &os) {
    os << "alloca " << *var;
}

void PhiInst::print(std::ostream &  os) {
    os << "phi [ ";
    for (auto &p : vals)
        os << "bb_" << p.second->id << ": " << p.first << ", ";
    os << ']';
}

void BinaryBranchInst::print(std::ostream &os) {
    using namespace rel;
    os << "binr " << lhs << ' ';
    switch (op) {
        case Eq: os << "=="; break;
        case Ne: os << "!="; break;
        case Lt: os << '<';  break;
        case Le: os << "<="; break;
        case Gt: os << '>';  break;
        case Ge: os << ">="; break;
    }
    os << ' ' << rhs << " ? bb_" << bb_then->id << " : bb_" << bb_else->id;;
}

#define INDENT      "    "
#define INDENT_2    "        "
#define INDENT_3    "            "

std::ostream &operator << (std::ostream &os, const Prog &prog) {
    os << ".globals:\n";
    for (auto *glob : prog.globals)
        os << INDENT << *glob << '\n';

    for (auto &func : prog.funcs) {
        os << "\n" << func.name << ": ";
        for (auto *p : func.params)
            os << *p << " ; ";
        os << '\n';
        uint inst_cnt = 0;
        FOR_BB_INST (i, bb, func)
            i->id = inst_cnt++;  // phi nodes can use insts after themselves

        FOR_BB (bb, func) {
            os << "bb_" << bb->id << ":\n";
            FOR_INST (i, *bb) {
                os << INDENT << *i << '\n';
            }
        }
    }
    return os;
}

}
