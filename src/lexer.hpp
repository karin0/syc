#pragma once

#include <string>
#include <vector>

using std::string;
using std::vector;
using std::size_t;

typedef unsigned uint;

namespace tkd {
    enum TokenKind {
        Ident, IntConst, FmtStr,
        Main,
        Const, Break, Continue, If, Else, While, Return,
        GetInt, Printf,
        Int, Void,
        Not, Add, Sub, Mul, Div, Mod, Lt, Gt, Assign, Semi, Comma, LPar, RPar, LBrk, RBrk, LBrc, RBrc,
        And, Or, Le, Ge, Eq, Ne
    };
}

using tkd::TokenKind;

struct Token {
    TokenKind kind;
    const char *source;
    std::size_t len;
    int ln;

    Token(TokenKind k, const char *src, std::size_t n, int ln) :
        kind(k), source(src), len(n), ln(ln) {}

    const char *kind_name() const;
    bool is_a(TokenKind k) const;

    explicit operator string() const;
};

const char *kind_name(TokenKind);

vector<Token> lex(char *ch);
