#include "ir.hpp"
#include <ostream>

#define INST_PRE "%"
#define BB_PRE "$L"

namespace ir {

std::ostream &operator << (std::ostream &os, const Use &u) {
    if (u.value)
        u.value->print_val(os);
    else
        os << "0";
    return os;
}

std::ostream &operator << (std::ostream &os, Inst &i) {
    i.print(os);
    return os;
}

std::ostream &operator << (std::ostream &os, const BB &bb) {
    os << BB_PRE << bb.id;
    return os;
}

static void print_decl(const Decl &var, std::ostream &os, const char *endl = "\n", bool is_alloca = false) {
    if (var.is_const)
        os << "const ";
    if (!var.dims.empty())
        os << "arr ";
    if (is_alloca)
        os << "int _stack_" << var.name;
    else
        os << "int " << var.name;
    for (auto l: var.dims) {
        if (l >= 0)
            os << '[' << l << ']';
        else
            os << "[]";
    }
    if (!var.init.empty()) {
        if (var.dims.empty()) {
            if (auto *x = as_a<ast::Number>(var.init[0]))
                os << " = " << x->val;
            os << endl;
        } else {
            os << endl;
            uint i = 0;
            if (var.dims.size() == 2) {
                uint j = 0, m = var.dims[1];
                for (auto *e: var.init) {
                    if (auto *x = as_a<ast::Number>(e)) {
                        if (is_alloca)
                            os << "_stack_";
                        os << var.name << '[' << i << "][" << j << "] = " << x->val << endl;
                    }
                    ++j;
                    if (j == m)
                        ++i, j = 0;
                }
            } else {
                for (auto *e: var.init) {
                    if (auto *x = as_a<ast::Number>(e)) {
                        if (is_alloca)
                            os << "_stack_";
                        os << var.name << '[' << i << "] = " << x->val << endl;
                    }
                    ++i;
                }
            }
        }
    }
    os << endl;
}

static std::ostream &operator << (std::ostream &os, const Decl &var) {
    print_decl(var, os);
    return os;
}

void Const::print_val(std::ostream &os) {
    os << val;
}

void Global::print_val(std::ostream &os) {
    os << var->name;
}

void Argument::print_val(std::ostream &os) {
    os << var->name;
}

void Undef::print_val(std::ostream &os) {
    os << '0';
}

void Inst::print_val(std::ostream &os) {
    os << INST_PRE << id;
}

static const char *bin_op_repr(tkd::TokenKind op) {
    using namespace tkd;
    switch (op) {
        case Add: return "+";
        case Sub: return "-";
        case Mul: return "*";
        case Div: return "/";
        case Mod: return "%";
        case Eq:  return "==";
        case Ne:  return "!=";
        case Lt:  return "<";
        case Gt:  return ">";
        case Le:  return "<=";
        case Ge:  return ">=";
        default:
            unreachable();
    }
}

void BinaryInst::print(std::ostream &os) {
    os << INST_PRE << id << " = " << lhs << ' ' << bin_op_repr(op) << ' ' << rhs;
}

#define INDENT      "    "
#define ENDL "\n" INDENT

void CallInst::print(std::ostream &os) {
    for (auto &u: args)
        os << "push " << u << ENDL;
    os << "call " << func->name << ENDL
          INST_PRE << id << " = RET";
}

void BranchInst::print(std::ostream &os) {
    os << "cmp " << cond << ", 0" ENDL
          "bne " BB_PRE << bb_then->id << ENDL
          "goto " BB_PRE << bb_else->id;
}

void JumpInst::print(std::ostream &os) {
    os << "goto " BB_PRE << bb_to->id;
}

void ReturnInst::print(std::ostream &os) {
    if (val.value)
        os << INST_PRE << id << " = " << val << ENDL
              "ret " << INST_PRE << id;
    else
        os << "ret";
}

void LoadInst::print(std::ostream &os) {
    os << INST_PRE << id << " = " << base;
    os << '[' << off << ']';
}

void StoreInst::print(std::ostream &os) {
    os << base;
    os << '[' << off << ']';
    os << " = " << val;
}

void GEPInst::print(std::ostream &os) {
    os << INST_PRE << id << " = " << base << " + " << off << " * " << size;
}

void AllocaInst::print(std::ostream &os) {
    print_decl(*var, os, ENDL, true);
    os << INST_PRE << id << " = &_stack_" << var->name;
}

void PhiInst::print(std::ostream &  os) {
    os << INST_PRE << id << " = phi ";
    bool first = true;
    for (auto &p : vals) {
        if (!first)
            os << ", ";
        else
            first = false;
        os << "[ " << p.first << ", " BB_PRE << p.second->id << " ]";
    }
}

static const char *rel_op_repr(RelOp op) {
    using namespace rel;
    switch (op) {
        case Eq: return "eq";
        case Ne: return "ne";
        case Lt: return "lt";
        case Le: return "le";
        case Gt: return "gt";
        case Ge: return "ge";
        default: unreachable();
    }
}

void BinaryBranchInst::print(std::ostream &os) {
    os << "cmp " << lhs << ", " << rhs << ENDL
          "b" << rel_op_repr(op) << " " BB_PRE << bb_then->id << ENDL
          "goto " BB_PRE << bb_else->id;
}

std::ostream &operator << (std::ostream &os, const Prog &prog) {
    for (auto *glob : prog.globals)
        os << *glob;

    for (auto &func : prog.funcs) {
        os << '\n' << (func.returns_int ? "int " : "void ") << func.name << "()\n";
        for (auto *p : func.params)
            os << "para " << *p;

        uint inst_cnt = 0;
        FOR_BB_INST (i, bb, func)
            i->id = inst_cnt++;  // phi nodes can use insts after themselves

        FOR_BB (bb, func) {
            os << BB_PRE << bb->id << ":\n";
            FOR_INST (i, *bb) {
                os << INDENT << *i << '\n';
            }
        }
    }
    return os;
}
}
