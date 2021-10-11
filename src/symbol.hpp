#pragma once

#include "common.hpp"
#include "ast.hpp"

#include <unordered_map>

using ast::Symbol;

struct SymbolTable {
    std::unordered_map<string, Symbol *> all, local;
    vector<std::unordered_map<string, Symbol *>> scopes;

    void insert(Symbol *s);

    Symbol *find(const string &n) const;

    void push();

    void pop();

    template <class T>
    T *find_a(const string &n) const {
        if (T *res = dynamic_cast<T *>(find(n)))
            return res;
        fatal("symbol %s is not a %s", n.data(), typeid(T).name());
    }
};
