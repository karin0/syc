#include "liveness.hpp"

void dce(Func *func) {
    build_liveness(func);
    for (auto *bb = func->bbs.back; bb; bb = bb->prev) {
        auto live = bb->live_out;
        for (Inst *i = bb->insts.back, *prev; i; i = prev) {
            prev = i->prev;
            auto def_use = get_def_use(i, func);
            auto &def = def_use.first;
            auto &use = def_use.second;
            if (def.size() == 1 && def.front().is_virtual() && !live.count(def.front()) && i->is_pure()) {
                infof("dce erasing", *i);
                bb->insts.erase(i);
                delete i;
                continue;
            }
            for (auto &d: def)
                live.erase(d);
            for (auto &u: use)
                live.insert(u);
        }
    }
}
