#pragma once

#include "../mips.hpp"

std::pair<vector<mips::Reg>, vector<mips::Reg>> get_use_def(mips::Inst *i, mips::Func *f);

vector<mips::Reg *> get_owned_regs(mips::Inst *i);
