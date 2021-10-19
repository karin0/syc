#include "ir_common.hpp"

static void dce0(Func *f) {
    // TODO: opt; const prog
    bool changed;
    do {
        changed = false;
        FOR_BB (bb, *f) {
            bool end = false;
            for (auto *i = bb->insts.front; i;) {
                auto *next = i->next;
                if (end || (i->uses.empty() && !i->has_side_effects())) {
                    bb->erase(i);
                    delete i;
                    changed = true;
                } else if (i->is_control())
                    end = true;
                i = next;
            }
        }
    } while (changed);
    info("%s: dce done", f->name.data());
}

static void traverse(Inst *i) {
    i->vis = true;
    for (auto *u: get_owned_uses(i))
        if_a (Inst, x, u->value) if (!x->vis)
            traverse(x);
}

// TODO: keeping dce0 reduces 1 inst for some case
void dce(Func *f) {
    infof("dce", f->name);

    FOR_BB_INST (i, bb, *f)
        i->vis = false;

    FOR_BB_INST (i, bb, *f)
        if (i->has_side_effects())
            traverse(i);

    FOR_BB (bb, *f)
        FOR_LIST_MUT (i, bb->insts) if (!i->vis) {
            bb->erase_with(i, nullptr);
            delete i;
        }
}

void dcbe(Func *f) {
    void dbe(Func *);
    dbe(f);
    dce(f);
    dbe(f);  // works anyhow
}
