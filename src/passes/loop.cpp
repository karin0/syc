#include "ir_common.hpp"

Loop::Loop(BB *header) : header(header) {}

Loop::~Loop() {
    for (auto *l: chs)
        delete l;
}

static void build(BB *u) {
    for (auto *v: u->dom_chs)
        build(v);
    vector<BB *> latches;
    for (auto *v: u->pred)
        if (v->dom.count(u))
            latches.push_back(v);
    if (!latches.empty()) {
        auto *loop = new Loop{u};
        do {
            auto *v = latches.back();
            latches.pop_back();
            if (auto *ch = v->loop) {
                while (ch->parent)
                    ch = ch->parent;
                if (ch != loop) {
                    ch->parent = loop;
                    for (auto *p: ch->header->pred)
                        if (p->loop != ch)
                            latches.push_back(p);
                }
            } else {
                v->loop = loop;
                if (v != u)
                    for (auto *w: v->pred)
                        latches.push_back(w);
            }
        } while (!latches.empty());
    }
}

static void build_children(BB *u, Func *f) {
    if (u->vis)
        return;
    u->vis = true;
    for (auto *v: u->get_succ())
        build_children(v, f);
    auto *loop = u->loop;
    if (loop && loop->header == u) {
        if (loop->parent)
            loop->parent->chs.push_back(loop);
        else
            f->loop_roots.push_back(loop);
    }
}

static void set_depth(Loop *u, int depth) {
    infof("loop with header bb", u->header->id, "has depth", depth);
    u->depth = depth;
    for (auto *v: u->chs)
        set_depth(v, depth + 1);
}

void build_loop(Func *f) {
    build_dom(f);
    build_pred(f);
    FOR_BB (bb, *f) {
        bb->loop = nullptr;
        bb->vis = false;
    }
    for (auto *l: f->loop_roots)
        delete l;
    f->loop_roots.clear();
    build(f->bbs.front);
    build_children(f->bbs.front, f);
    for (auto *loop: f->loop_roots)
        set_depth(loop, 1);
}
