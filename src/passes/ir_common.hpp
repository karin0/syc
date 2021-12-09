#pragma once

#include "ir.hpp"

using namespace ir;

void build_dom(Func *f);
void build_cg(Prog *f);
void build_pred(Func *f);
void build_loop(Func *f);
void dce(Func *f);

vector<const Use *> get_owned_uses(Inst *i);
