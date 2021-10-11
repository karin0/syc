#include "use_def.hpp"
#include <algorithm>

using namespace mips;
using std::set;
using std::unordered_map;

static void remove_colored(vector<Reg> &v) {
    auto pred = [](Operand &x) { return !x.is_uncolored(); };
    v.erase(std::remove_if(v.begin(), v.end(), pred), v.end());
    for (auto &x: v)
        asserts(x.is_uncolored());
}

static std::pair<vector<Reg>, vector<Reg>> get_use_def_uncolored(Inst *i, Func *f) {
    auto r = get_use_def(i, f);
    remove_colored(r.first);
    remove_colored(r.second);
    return r;
}

static void liveness_analysis(Func *f) {
    FOR_MBB (bb, *f) {
        bb->use.clear();
        bb->def.clear();
        bb->live_out.clear();

        FOR_MINST (i, *bb) {
            auto use_def = get_use_def(i, f);
            for (auto &x: use_def.first) if (!bb->def.count(x))
                bb->use.insert(x);
            for (auto &x: use_def.second) if (!bb->use.count(x))
                bb->def.insert(x);
        }

        bb->live_in = bb->use;
    }
    bool changed;
    do {
        changed = false;
        FOR_MBB (bb, *f) {
            set<Reg> out;
            for (auto *t: bb->succ)
                out.insert(t->live_in.begin(), t->live_in.end());
            if (out != bb->live_out) {
                changed = true;
                bb->live_out = std::move(out);
                bb->live_in = bb->use;
                for (auto &x: bb->live_out) if (!bb->def.count(x))
                    bb->live_in.insert(x);
            }
        }
    } while (changed);
}

template <>
struct std::hash<Operand> {
    size_t operator () (const Operand &k) const {
        return (k.val << 2) + k.kind;
    }
};

namespace reg_allocater {

constexpr uint K = Regs::allocatable.size();

struct Node {
    Operand reg;
    uint degree = 0, color;
    Node *alias;
    std::set<Node *> adj_list;
    std::set<MoveInst *> move_list;  // TODO: why use a set?
    bool colored;  // colored_nodes
};

template <class T>
void insert_all(set<T> dst, set<T> src) {
    for (auto &x: src)
        dst.insert(x);
}

struct Allocater {
    Func *func;
    unordered_map<Operand, Node> nodes;
    vector<Node *> select_stack;
    set<MoveInst *> wl_moves;
    set<Node *> spilled_nodes, coalesced_nodes, spill_wl, freeze_wl, simplify_wl;

    void clear() {
        nodes.clear();
        select_stack.clear();
        wl_moves.clear();
        spilled_nodes.clear();
        coalesced_nodes.clear();
        spill_wl.clear();
        freeze_wl.clear();
        simplify_wl.clear();
    }

    explicit Allocater(Func *f) {
        func = f;
    }

    static void add_edge(Node *u, Node *v) {
        if (u == v || u->adj_list.count(v))
            return;
        u->adj_list.insert(v);
        v->adj_list.insert(u);
        if (!u->reg.is_pinned())
            ++u->degree;
        if (!v->reg.is_pinned())
            ++v->degree;
    }

    void build() {
        FOR_MBB (bb, *func) {
            auto live = bb->live_out;
            for (auto *i = bb->insts.back; i; i = i->prev) {
                if_a (MoveInst, x, i) if (x->src.is_reg()) {
                    x->active = false;  // initialize active_moves
                    live.erase(x->src);
                    nodes[x->src].move_list.insert(x);
                    nodes[x->dst].move_list.insert(x);
                    wl_moves.insert(x);
                }
                auto use_def = get_use_def(i, func);
                auto &def = use_def.second;
                for (auto &d: def)
                    live.insert(d);
                for (auto &d: def)
                    for (auto &l: live)
                        add_edge(&nodes[l], &nodes[d]);
                for (auto &d: def)
                    live.erase(d);
                for (auto &u: use_def.first)
                    live.insert(u);
            }
        }
    }

    set<Node *> adjacent(Node *u) {
        set<Node *> r;
        for (auto &x: u->adj_list) if (
            std::find(select_stack.begin(), select_stack.end(), x) == select_stack.end() &&
            !coalesced_nodes.count(x)
        )
            r.insert(u);
        return r;
    }

    set<MoveInst *> node_moves(Node *u) const {
        set<MoveInst *> r;
        for (auto &x: u->move_list)
            if (x->active || wl_moves.count(x))
                r.insert(x);
        return r;
    }

    bool move_related(Node *u) const {
        auto &l = u->move_list;
        return std::any_of(l.begin(), l.end(), [&](MoveInst *x) {
            return x->active || wl_moves.count(x);
        });
    }

    void make_wl() {
        for (uint i = 0; i < func->vreg_cnt; ++i) {
            auto &u = nodes[Reg::make_virtual(i)]; // TODO: initial
            if (u.degree >= K)
                spill_wl.insert(&u);
            else if (move_related(&u))
                freeze_wl.insert(&u);
            else
                simplify_wl.insert(&u);
        }
    }

    void simplify() {
        auto it = simplify_wl.begin();
        auto u = *it;
        simplify_wl.erase(it);
        select_stack.push_back(u);
        for (auto *v: adjacent(u))
            dec_degree(v);
    }

    void dec_degree(Node *u) {
        if (u->degree-- == K) {
            enable_moves(u);
            for (auto *v: adjacent(u))
                enable_moves(v);
            spill_wl.erase(u);
            if (move_related(u))
                freeze_wl.insert(u);
            else
                simplify_wl.insert(u);
        }
    }

    // inner subroutine
    void enable_moves(Node *u) {
        for (auto *m: node_moves(u)) if (m->active) {
            m->active = false;
            wl_moves.erase(m);
        }
    }

    void coalesce() {
        auto it = wl_moves.begin();
        auto *m = *it;
        auto *u = &nodes[m->dst];
        auto *v = &nodes[m->src];
        if (m->src.is_pinned())
            std::swap(u, v);
        wl_moves.erase(it);
        if (u == v) {
            // coalesced_moves is unused
            add_wl(u);
            return;
        }
        if (v->reg.is_pinned() || u->adj_list.count(v)) {
            // constrained_moves is unused
            add_wl(u);
            add_wl(v);
            return;
        }
        bool u_precolored = u->reg.is_pinned();
        auto ad = adjacent(v);
        if ((u_precolored && std::all_of(ad.begin(), ad.end(),
             [&](const Node *t) {
                return ok(t, u);
             })) || (!u_precolored && (insert_all(ad, adjacent(u)), conservative(ad)))) {
            combine(u, v);
            add_wl(u);
        } else
            m->active = true;
    }

    void add_wl(Node *u) {
        if (!u->reg.is_pinned() && u->degree < K && !move_related(u)) {
            freeze_wl.insert(u);
            simplify_wl.insert(u);
        }
    }

    static bool ok(const Node *t, Node *r) {
        return t->degree < K || t->reg.is_pinned() || t->adj_list.count(r);
    }

    static bool conservative(set<Node *> s) {
        uint k = 0;
        for (auto *u: s)
            if (u->degree >= K && ++k >= K)
                return false;
        return true;
    }

    Node *get_alias(Node *u) {
        return coalesced_nodes.count(u) ? get_alias(u->alias) : u;
    }

    void combine(Node *u, Node *v) {
        if (!freeze_wl.erase(v))
            spill_wl.erase(v);
        coalesced_nodes.insert(v);
        v->alias = u;
        // typo?
        insert_all(u->move_list, v->move_list);
        for (Node *t: adjacent(v)) {
            add_edge(t, u);
            dec_degree(t);
        }
        if (u->degree >= K && freeze_wl.erase(u))
            spill_wl.insert(u);
    }

    void freeze() {
        auto it = freeze_wl.begin();
        auto *u = *it;
        freeze_wl.erase(it);
        simplify_wl.insert(u);
        freeze_moves(u);
    }

    void freeze_moves(Node *u) {
        for (auto *m: node_moves(u)) {
            if (m->active)
                m->active = false;
            else
                wl_moves.erase(m);
            // frozen_moves is unused
            auto vx = m->src;
            if (vx == u->reg)
                vx = m->dst;
            else
                asserts(m->dst == u->reg);
            auto *v = &nodes[vx];
            if (!move_related(v) && v->degree < K) {
                freeze_wl.erase(v);
                simplify_wl.insert(v);
            }
        }
    }

    void select_spill() {
        auto it = spill_wl.begin(); // TODO
        auto *u = *it;
        spill_wl.erase(it);
        simplify_wl.insert(u);
        freeze_moves(u);
    }

    void assign_colors() {
        while (!select_stack.empty()) {
            auto *u = select_stack.back();
            select_stack.pop_back();
            bool ok_colors[K];
            uint siz = K;
            std::fill(ok_colors, ok_colors + K, true);
            for (auto *v: u->adj_list) {
                auto *va = get_alias(v);
                if (va->reg.is_pinned() || v->colored) {  // todo: real color?
                    siz -= ok_colors[va->color];  // FIXME: is_pinned and color is what?
                    ok_colors[va->color] = false;
                }
                if (va->reg.kind == Reg::Machine)
                    fatal("????");
            }
            if (siz) {
                u->colored = true;
                u->color = K;
                for (uint i = 0; i < K; ++i) if (ok_colors[i]) {
                    u->color = i;
                    break;
                }
                // TODO: in what order?
                asserts(u->color < K);
            } else
                spilled_nodes.insert(u);
        }
        for (auto *u: coalesced_nodes)
            u->color = get_alias(u)->color;
        FOR_MBB_MINST (i, bb, *func) {
            for (auto *x: get_owned_regs(i)) {
                auto it = nodes.find(*x);  // TODO: precolored?
                if (it != nodes.end()) {
                    auto &u = it->second;
                    if (u.colored) {
                        *x = Reg::make_machine(Regs::allocatable[u.color]);
                        info("filling %u", u.color);
                    }
                }
            }
        }
    }

    void rewrite_program() {
        // TODO
    }

    void run() {
        while (true) {
            liveness_analysis(func);
            // TODO: init degrees
            build();
            make_wl();
            do {
                if (!simplify_wl.empty())
                    simplify();
                else if (!wl_moves.empty())
                    coalesce();
                else if (!freeze_wl.empty())
                    freeze();
                else if (spill_wl.empty())
                    select_spill();
            } while (!(simplify_wl.empty() && wl_moves.empty() && freeze_wl.empty() && spill_wl.empty()));
            assign_colors();
            if (spilled_nodes.empty())
                return;
            rewrite_program();
        }
    }
};

}

void reg_alloc(Func *f) {
    reg_allocater::Allocater{f}.run();
}
