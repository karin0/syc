#pragma once

#include "ir.hpp"

using namespace ir;

void build_dom(Func *f);
void build_pred(Func *f);
void build_loop(Func *f);

void cg(Prog *f);
bool dce(Func *f);
bool dbe(Func *f);

vector<const Use *> get_owned_uses(Inst *i);

// Erase its uses in phis before dropping
void drop_bb(BB *u, Func *f);
