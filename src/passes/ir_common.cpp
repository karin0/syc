#include "ir_common.hpp"

void build_pred(Func *f) {
    FOR_BB (u, *f)
        u->pred.clear();
    FOR_BB (u, *f)
        for (BB *v : u->get_succ())
            v->pred.push_back(u);
}

static void traverse(BB *u, BB *block) {
    if (u->vis || u == block)
        return;
    u->vis = true;
    for (auto v : u->get_succ())
        traverse(v, block);
}

static void set_depth(BB *u, int depth) {
    u->dom_depth = depth;
    for (auto *p: u->domees)
        set_depth(p, depth + 1);
}

void build_dom(Func *f) {
    FOR_BB (u, *f) {
        u->dom.clear();
        u->domees.clear();
        u->idom = nullptr;
    }

    FOR_BB (w, *f) {
        FOR_BB (u, *f)
            u->vis = false;

        traverse(f->bbs.front, w);
        FOR_BB (u, *f) if (!u->vis) {
                u->dom.insert(w);
                w->domees.insert(u);
                info("%s: bb_%d doms bb_%d", f->name.data(), w->id, u->id);
            }
    }

    FOR_BB (u, *f) {
        for (BB *w : u->dom) if (u != w) {
                bool ok = true;
                for (BB *v : u->dom) if (v != u && w != v && v->dom.count(w)) {
                        ok = false;
                        break;
                    }
                if (ok) {
                    u->idom = w;
                    info("%s: bb_%d idoms bb_%d", f->name.data(), w->id, u->id);
                    break;
                }
            }
    }

    set_depth(f->bbs.front, 0);
}
