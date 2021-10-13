#include "../ir.hpp"

using namespace ir;

static void traverse(BB *u) {
    if (u->vis)
        return;
    u->vis = true;
    for (auto v : u->get_succ())
        traverse(v);
}

// This ensures BB::get_succ works properly and all bbs are reachable
void dbe(Func *f) {
    FOR_BB (bb, *f) {
        bool end = false;
        FOR_LIST_MUT (i, bb->insts) {
            if (end) {
                bb->erase(i);
                delete i;
            } else if (i->is_control())
                end = true;
        }
    }

    FOR_BB (u, *f)
        u->vis = false;

    traverse(f->bbs.front);
    FOR_LIST_MUT (u, f->bbs) {
        if (!u->vis) {
            infof("unreachable bb", u->id);
            FOR_LIST_MUT (i, u->insts)
                delete i;
            f->bbs.erase(u);
            delete u;
        }
    }

    infof(f->name, ": dbe done");
}
