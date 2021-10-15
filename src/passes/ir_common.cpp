#include "ir_common.hpp"

void build_pred(Func *f) {
    FOR_BB (u, *f)
        u->pred.clear();
    FOR_BB (u, *f)
        for (BB *v : u->get_succ())
            v->pred.push_back(u);
}
