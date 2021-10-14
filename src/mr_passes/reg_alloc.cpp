#include "use_def.hpp"
#include <algorithm>
#include <bitset>

using namespace mips;
using std::set;
using std::unordered_map;

// TODO: drop ignored, colored, Machine/Pinned
static bool is_ignored(const Operand &x) {
    return !(x.is_virtual() || (x.is_pinned() && Regs::inv_allocatable[x.val] < 32));
}

static void remove_colored(vector<Reg> &v) {
    v.erase(std::remove_if(v.begin(), v.end(), is_ignored), v.end());
    for (auto &x: v)
        asserts(x.is_uncolored());
}

static std::pair<vector<Reg>, vector<Reg>> get_def_use_uncolored(Inst *i, Func *f) {
    auto r = get_def_use(i, f);
    remove_colored(r.first);
    remove_colored(r.second);
    return r;
}

// TODO: this may be unreliable, as bbs now are not "real" bbs, but can contain j/brs due to phi resolving
// that's possibly why dce on machine regs cannot be performed
// maybe we should add phi copies at the beginning of the source bb (tested to affect perf) or in a new inserted bb
static void liveness_analysis(Func *f) {
    FOR_BB (bb, *f) {
        bb->use.clear();
        bb->def.clear();
        bb->live_out.clear();

        FOR_INST (i, *bb) {
            auto use_def = get_def_use_uncolored(i, f);
            for (auto &x: use_def.second) if (!bb->def.count(x))
                bb->use.insert(x);
            for (auto &x: use_def.first) if (!bb->use.count(x))
                bb->def.insert(x);
        }

        bb->live_in = bb->use;
    }
    bool changed;
    do {
        changed = false;
        FOR_BB (bb, *f) {
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
    uint degree = 0, color = 0x7f;
    Node *alias = nullptr;
    std::set<Node *> adj_list;
    std::set<MoveInst *> move_list;
    bool colored = false;  // colored_nodes
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

    Node *get_node(Operand r) {
        // if (nodes.find(r) == nodes.end())
        //     debugf("inserting", r);
        auto &u = nodes[r];
        u.reg = r;
        return &u;
    }

    static void add_edge(Node *u, Node *v) {
        asserts(u->reg.is_uncolored());
        asserts(v->reg.is_uncolored());
        // asserts(u->reg.is_virtual() || v->reg.is_virtual());
        // TODO: adj_set
        if (u == v || u->adj_list.count(v))
            return;
        if (!u->reg.is_pinned()) {
            infof("adding v", v->reg, "to adj of u", u->reg);
            u->adj_list.insert(v);
            ++u->degree;
        }
        if (!v->reg.is_pinned()) {
            infof("adding u", u->reg, "to adj of v", v->reg);
            v->adj_list.insert(u);
            ++v->degree;
        }
    }

    void build() {
        // FOR_BB (bb, *func) { // TODO: ??
        for (auto *bb = func->bbs.back; bb; bb = bb->prev) {
            auto live = bb->live_out;
            for (Inst *i = bb->insts.back, *prev; i; i = prev) {
                prev = i->prev;
                auto def_use = get_def_use_uncolored(i, func);
                auto &def = def_use.first;
                auto &use = def_use.second;
                if (def.size() == 1 && def.front().is_virtual() && !live.count(def.front()) && i->is_pure()) {
                    infof("erasing", *i);
                    bb->insts.erase(i);
                    delete i;
                    continue;
                }
                if_a (MoveInst, x, i) if (!(is_ignored(x->src) || is_ignored(x->dst))) {
                    x->active = false;  // initialize active_moves
                    live.erase(x->src);
                    get_node(x->src)->move_list.insert(x);
                    get_node(x->dst)->move_list.insert(x);
                    wl_moves.insert(x);
                }
                for (auto &d: def)
                    live.insert(d);  // the point is to insert them all to the graph
                for (auto &d: def)
                    for (auto &l: live) {
                        if (d != l)
                            infof(func->ir->name, ": building edge &", d, '&', l);
                        add_edge(get_node(l), get_node(d));
                    }
                for (auto &d: def)
                    live.erase(d);
                for (auto &u: use)
                    live.insert(u);
            }
        }
    }

    set<Node *> adjacent(Node *u) {
        set<Node *> r;
        for (auto *x: u->adj_list)
            if (
                std::find(select_stack.begin(), select_stack.end(), x) == select_stack.end() &&
                !coalesced_nodes.count(x)
            )
                r.insert(x);
        return r;
    }

    set<MoveInst *> node_moves(Node *u) const {
        set<MoveInst *> r;
        for (auto *x: u->move_list)
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
            auto it = nodes.find(Reg::make_virtual(i));
            if (it == nodes.end())
                continue;
            auto &u = it->second;
            if (u.degree >= K)
                spill_wl.insert(&u), infof("adding", u.reg, "to spill_wl");
            else if (move_related(&u))
                freeze_wl.insert(&u), infof("adding", u.reg, "to freeze_wl");
            else
                simplify_wl.insert(&u), infof("adding", u.reg, "to simplify_wl");
        }
    }

    void simplify() {
        auto it = simplify_wl.begin();
        auto u = *it;
        infof("simplifying", u->reg, "with deg", u->degree);
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
                freeze_wl.insert(u), infof("dec_deg inserting", u->reg, "to freeze_wl");
            else
                simplify_wl.insert(u), infof("dec_deg inserting", u->reg, "to simplify_wl");
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
        auto *u = get_alias(&nodes[m->dst]);
        auto *v = get_alias(&nodes[m->src]);
        if (m->src.is_pinned())
            std::swap(u, v);
        wl_moves.erase(it);
        if (u == v) {
            // coalesced_moves is unused
            add_wl(u);
            return;
        }
        if (v->reg.is_pinned() || u->adj_list.count(v) || v->adj_list.count(u)) {
            // constrained_moves is unused
            add_wl(u);
            add_wl(v);
            return;
        }
        bool u_precolored = u->reg.is_pinned();
        auto ad = adjacent(v);
        if ((u_precolored && std::all_of(ad.begin(), ad.end(),
             [&](Node *t) {
                return ok(t, u);
             })) || (!u_precolored && (insert_all(ad, adjacent(u)), conservative(ad)))) {
            combine(u, v);
            add_wl(u);
        } else
            m->active = true;
    }

    void add_wl(Node *u) {
        if (!u->reg.is_pinned() && u->degree < K && !move_related(u)) {
            infof("add_wl", u->reg);
            freeze_wl.insert(u);
            simplify_wl.insert(u);
        }
    }

    static bool ok(Node *t, Node *r) {
        return t->degree < K || t->reg.is_pinned() || t->adj_list.count(r) || r->adj_list.count(t);
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
        infof(func->ir->name, "combining", u->reg, "with", v->reg);
        if (!freeze_wl.erase(v))
            spill_wl.erase(v);
        coalesced_nodes.insert(v);
        v->alias = u;
        // typo?
        insert_all(u->move_list, v->move_list);
        auto s = adjacent(v);
        for (Node *t: s) {
            infof("so connect", t->reg, "to", u->reg);
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
        infof("freezing", u->reg);
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
            asserts(nodes.find(vx) != nodes.end());
            auto *v = &nodes[vx];
            if (!move_related(v) && v->degree < K) {
                infof("move", v->reg, "from freeze_wl to simplify_wl, with u", u->reg);
                freeze_wl.erase(v);
                simplify_wl.insert(v);
            }
        }
    }

    void select_spill() {
        auto it = spill_wl.begin();
        auto *u = *it;
        infof("selecting spill", u->reg);
        asserts(u->reg.is_virtual());
        spill_wl.erase(it);
        simplify_wl.insert(u);
        freeze_moves(u);
    }

    uint get_color(Node *u) {
        asserts(u->reg.is_uncolored());
        if (u->reg.is_pinned())
            return Regs::inv_allocatable[u->reg.val];
        if (u->colored)
            return u->color;
        return K;
    }

    void assign_colors() {
        while (!select_stack.empty()) {
            auto *u = select_stack.back();
            select_stack.pop_back();
            // bool ok_colors[K];
            std::bitset<K> ok_colors;
            // std::fill(ok_colors, ok_colors + K, true);
            ok_colors.set();
            for (auto *v: u->adj_list) {
                uint c = get_color(get_alias(v));
                if (c < K) {
                    // if (ok_colors.test(c))
                    //     warnf(func->ir->name, "color", c, Regs::to_name(Regs::allocatable[c]), "is taken for", u->reg);
                    ok_colors.reset(c);
                }
            }
                // ok_colors[c] = false;
            if (ok_colors.count()) {
                u->colored = true;
                u->color = K;
                for (uint i = 0; i < K; ++i) if (ok_colors.test(i)) {
                    u->color = i;
                    break;
                }
                infof(func->ir->name, "coloring", u->reg, "with", Regs::to_name(Regs::allocatable[u->color]), u->color);
                // TODO: in what order?
                asserts(u->color < K);
            } else {
                infof(func->ir->name, "spilling", u->reg);
                spilled_nodes.insert(u);
            }
        }

        for (auto *u: coalesced_nodes) {
            auto *a = get_alias(u);
            u->color = get_color(a);  // todo: why double coloring?
            // asserts(u->color < K);
            if (u->color >= K) {
                asserts(spilled_nodes.count(a));
                continue; // todo: qwqwq
            }
            infof(func->ir->name, "coloring coalesced", u->reg, "with", Regs::to_name(Regs::allocatable[u->color]), u->color);
            u->colored = true;
        }

        FOR_BB_INST (i, bb, *func) {
            for (auto *x: get_owned_regs(i)) {
                auto it = nodes.find(*x);
                if (it != nodes.end()) {
                    auto &u = it->second;
                    if (x->is_pinned())
                        x->kind = Reg::Pinned;
                    else if (u.colored)
                        *x = Reg::make_pinned(Regs::allocatable[u.color]); // TODO: QAQ why machined?
                }
            }
        }
    }

    void rewrite_program() {
        for (auto &u: spilled_nodes)
            spill(u->reg);
    }

    void spill(Reg r) const {
        infof("doing spilling for", r);
        int off = int((func->max_call_arg_num + func->alloca_num + func->spill_num) << 2);
        FOR_BB (bb, *func) {
            Inst *first_use = nullptr, *last_def = nullptr;
            Operand spiller;
            auto cp = [&]() {
                if (first_use) {
                    asserts(spiller.is_virtual());
                    infof("use", spiller, "for spilled", r, "at", off);
                    bb->insts.insert(first_use, new LoadInst{
                        spiller, Reg::make_pinned(Regs::sp), off
                    });
                    first_use = nullptr;
                }
                if (last_def) {
                    asserts(spiller.is_virtual());
                    infof("def", spiller, "for spilled", r, "at", off);
                    bb->insts.insert_after(last_def, new StoreInst{
                        spiller, Reg::make_pinned(Regs::sp), off
                    });
                    last_def = nullptr;
                }
                spiller.kind = Operand::Void;
            };
            int cnt = 0;
            FOR_INST (i, *bb) {
                auto def_use = get_owned_def_use(i);
                auto *def = def_use.first;
                if (def && *def == r) {
                    if (spiller.is_void())
                        spiller = func->make_vreg();
                    *def = spiller;
                    last_def = i;
                }
                for (auto *use: def_use.second) if (*use == r) {
                    if (spiller.is_void())
                        spiller = func->make_vreg();
                    *use = spiller;
                    if (!first_use && !last_def)
                        first_use = i;
                }
                if (cnt++ > 30) {
                    cp();
                    // TODO: cnt = 0 ?
                }
            }
            cp();
        }
        ++func->spill_num;
    }

    void run() {
        while (true) {
            infof(func->ir->name + ": reg alloc loop");
            liveness_analysis(func);

            for (uint i = 0; i < K; ++i) if (Regs::inv_allocatable[i] < 32)
                get_node(Reg::make_pinned(i))->degree = 0x7fffffff;

            build();
            make_wl();
            do {
                info("reg alloc inner loop");
                if (!simplify_wl.empty())
                    simplify();
                else if (!wl_moves.empty())
                    coalesce();
                else if (!freeze_wl.empty())
                    freeze();
                else if (!spill_wl.empty())
                    select_spill();
            } while (!(simplify_wl.empty() && wl_moves.empty() && freeze_wl.empty() && spill_wl.empty()));
            assign_colors();
            if (spilled_nodes.empty())
                break;
            rewrite_program();
            clear();
        }
        clear();
    }
};

}

void dce(Func *func) {
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

// TODO: alloc sp (or just use in mem insts)
void reg_alloc(Func *f) {
    reg_allocater::Allocater{f}.run();
}
