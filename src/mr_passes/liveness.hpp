#pragma once

#include "../mips.hpp"

using namespace mips;

std::pair<vector<Reg>, vector<Reg>> get_def_use(Inst *i, Func *f);

vector<Reg *> get_owned_regs(Inst *i);

std::pair<Reg *, vector<Reg *>> get_owned_def_use(Inst *i);

vector<Reg> get_def(Inst *i);

bool is_ignored(const Operand &x);
std::pair<vector<Reg>, vector<Reg>> get_def_use_uncolored(Inst *i, Func *f);
void build_liveness(Func *f);
