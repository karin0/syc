#include "../ir.hpp"

using namespace ir;

void dce(Func *f) {
    // TODO: opt; const prog; remove after j/br/ret
    bool changed;
    do {
        changed = false;
        FOR_BB (bb, *f) {
            bool end = false;
            for (auto *i = bb->insts.front; i;) {
                auto *next = i->next;
                if (end || (i->uses.empty() && i->is_pure())) {
                    bb->erase(i);
                    delete i;
                    changed = true;
                } else if (i->is_branch())
                    end = true;
                i = next;
            }
        }
    } while (changed);
    info("%s: dce done", f->name.data());
}
