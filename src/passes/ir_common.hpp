#pragma once

#include "../ir.hpp"

using namespace ir;

void build_dom(Func *f);
void build_cg(Prog *f);
void build_pred(Func *f);
