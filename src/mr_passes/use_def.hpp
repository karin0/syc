#pragma once

#include "../mips.hpp"

std::pair<vector<mips::Reg>, vector<mips::Reg>> get_def_use(mips::Inst *i, mips::Func *f);

vector<mips::Reg *> get_owned_regs(mips::Inst *i);

std::pair<mips::Reg *, vector<mips::Reg *>> get_owned_def_use(mips::Inst *i);

vector<mips::Reg> get_def(mips::Inst *i);
