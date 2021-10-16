#pragma once

#include "common.hpp"
#include "ast.hpp"
#include <unordered_map>

using ast::Symbol;

struct SymbolTable {
    std::unordered_map<string, Symbol *> all, local;
    vector<std::unordered_map<string, Symbol *>> scopes;

    bool insert(Symbol *s);

    Symbol *find(const string &n) const;

    void push();

    void pop();
};
