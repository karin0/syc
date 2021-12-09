#pragma once

#include "ir.hpp"
#include "mips.hpp"

void run_passes(ir::Prog &prog, bool opt = true);
void run_mips_passes(mips::Prog &prog, bool opt = true);
