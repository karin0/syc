#include "ir_common.hpp"
#include <unordered_set>

bool res;
Func *func;

bool try_elim(Loop *l) {
    static std::unordered_set<BB *> loop_bbs;
    loop_bbs.clear();
    loop_bbs.insert(l->bbs.begin(), l->bbs.end());
    BB *pre_header = nullptr;
    for (auto *bb: l->header->pred)
        if (!loop_bbs.count(bb)) {
            if (pre_header)
                return false;
            pre_header = bb;
        }
    if (!pre_header)
        return false;

    BB *exit = nullptr;
    vector<BB *> exitings;
    for (auto *bb: l->bbs)
        for (auto *v: bb->get_succ())
            if (!loop_bbs.count(v)) {
                if (!exit)
                    exit = v;
                else if (exit != v)
                    return false;
                exitings.push_back(bb);
            }

    if (!exit)
        return false;

    FOR_INST (i, *exit) {
        if_a (PhiInst, x, i) {
            Value *v = nullptr;
            for (const auto &p: x->vals)
                if (loop_bbs.count(p.second)) {
                    if (!v)
                        v = p.first.value;
                    else if (v != p.first.value)
                        return false;
                }
            asserts(v);
        } else
            break;
    }

    for (auto *bb: l->bbs)
        FOR_INST (i, *bb) {
            if_a (ReturnInst, x, i)
                return false;
            if (!i->is_control() && i->has_side_effects())
                return false;
            FOR_LIST (u, i->uses)
                if (!loop_bbs.count(u->user->bb))
                    return false;
        }

    infof("dropping loop with header", l->header->id, "in", func->name);

    FOR_INST (i, *exit) {
        if_a (PhiInst, x, i) {
            Value *v;
            vec_erase_if(x->vals, [&](const std::pair<Use, BB *> &p) {
                if (!loop_bbs.count(p.second))
                    return false;
                v = p.first.value;
                return true;
            });
            x->push(v, pre_header);
        } else
            break;
    }

    for (auto **v: pre_header->get_succ_mut())
        if (*v == l->header)
            *v = exit;

    for (auto *bb: l->bbs)
        drop_bb(bb, func);

    res = true;
    return true;
}

void dfs(Loop *l) {
    if (l->chs.empty())
        try_elim(l);
    else for (Loop *l: l->chs)
        dfs(l);
}

bool do_dle(Func *f) {
    build_loop(f);
    res = false;
    for (Loop *l: f->loop_roots)
        dfs(l);
    return res;
}

void dle(Func *f) {
    func = f;
    while (do_dle(f));
}
