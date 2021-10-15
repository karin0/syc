#include "symbol.hpp"

bool SymbolTable::insert(Symbol *s) {
    auto it = all.find(s->name);
    // debug("inserted symbol %s (%p)", s->name.data(), &s);
    if (it != all.end()) {
        auto lit = local.find(s->name);
        if (lit != local.end()) {
            // fatal("redefined symbol %s", s->name.data());
            return false;
        }
        local[s->name] = it->second;
        it->second = s;
    } else {
        local[s->name] = nullptr;
        all[s->name] = s;
    }
    return true;
}

Symbol *SymbolTable::find(const string &n) const {
    auto it = all.find(n);
    if (it != all.end()) {
        // debug("found symbol (%p) named %s", it->second, it->second->name.data());
        return it->second;
    }
    return nullptr;
    // fatal("undefined symbol %s", n.data());
}

void SymbolTable::push() {
    // debug("push scope %zu", scopes.size() + 1);
    scopes.push_back(std::move(local));
    local.clear();
}

void SymbolTable::pop() {
    // debug("pop scope %zu", scopes.size());
    for (const auto &t : local) {
        if (t.second == nullptr)
            all.erase(t.first);
        else
            all[t.first] = t.second;
    }
    local = std::move(scopes.back());
    scopes.pop_back();
}
