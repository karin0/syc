#include "lexer.hpp"
#include "common.hpp"

#include <cctype>
#include <string>
#include <unordered_map>

using namespace tkd;

static const std::unordered_map<std::string, TokenKind> keywords = {
    { "main", Main },
    { "const", Const },
    { "int", Int },
    { "break", Break },
    { "continue", Continue },
    { "if", If },
    { "else", Else },
    { "while", While },
    { "getint", GetInt },
    { "printf", Printf },
    { "return", Return },
    { "void", Void }
};

const char *kind_name(TokenKind kind) {
    switch (kind) {
        case Ident:    return "IDENFR";
        case IntConst: return "INTCON";
        case FmtStr:   return "STRCON";
        case Main:     return "MAINTK";
        case Const:    return "CONSTTK";
        case Break:    return "BREAKTK";
        case Continue: return "CONTINUETK";
        case If:       return "IFTK";
        case Else:     return "ELSETK";
        case While:    return "WHILETK";
        case Return:   return "RETURNTK";
        case GetInt:   return "GETINTTK";
        case Printf:   return "PRINTFTK";
        case Int:      return "INTTK";
        case Void:     return "VOIDTK";
        case Not:      return "NOT";
        case Add:      return "PLUS";
        case Sub:      return "MINU";
        case Mul:      return "MULT";
        case Div:      return "DIV";
        case Mod:      return "MOD";
        case Lt:       return "LSS";
        case Gt:       return "GRE";
        case Assign:   return "ASSIGN";
        case Semi:     return "SEMICN";
        case Comma:    return "COMMA";
        case LPar:     return "LPARENT";
        case RPar:     return "RPARENT";
        case LBrk:     return "LBRACK";
        case RBrk:     return "RBRACK";
        case LBrc:     return "LBRACE";
        case RBrc:     return "RBRACE";
        case And:      return "AND";
        case Or:       return "OR";
        case Le:       return "LEQ";
        case Ge:       return "GEQ";
        case Eq:       return "EQL";
        case Ne:       return "NEQ";
    }
    return nullptr;
}

static bool is_ident_char(int c) {
    return std::isalnum(c) || c == '_';
}

static bool is_ident_start(int c) {
    return std::isalpha(c) || c == '_';
}

struct Cursor {
    const char *ch;
    int ln = 1;

    explicit Cursor(const char *s): ch(s) {}

    Cursor &operator ++ () {
        if (*ch == '\n')
            ++ln;
        ++ch;
        return *this;
    }

    Cursor &operator += (uint d) {
        while (d--)
            ++*this;
        return *this;
    }

    operator const char * () const {
        return ch;
    }
};

struct Lexer {
    Cursor ch;

    explicit Lexer(char *s) : ch(s) {}

    TokenKind next();
    std::vector<Token> lex();
};

TokenKind Lexer::next() {
    const char *start = ch;
    int c = *ch;

    if (is_ident_start(c)) {
        while (is_ident_char(*++ch));
        auto it = keywords.find(std::string(start, ch - start));
        if (it == keywords.end())
            return Ident;
        return it->second;
    }

    if (std::isdigit(c)) {
        while (std::isdigit(*++ch));
        return IntConst;
    }

    if (c == '"') {
        while (*++ch != '"');
        ++ch;
        return FmtStr;
    }

    switch (c) {

#define MO(p, t) case p: return ++ch, t;

        // Div is handled earlier
        MO('+', Add)
        MO('-', Sub)
        MO('*', Mul)
        MO('%', Mod)
        MO(';', Semi)
        MO(',', Comma)
        MO('(', LPar)
        MO(')', RPar)
        MO('[', LBrk)
        MO(']', RBrk)
        MO('{', LBrc)
        MO('}', RBrc)

#define MB(p, q, t, tt) case p: return ch[1] == (q) ? ch += 2, (tt) : (++ch, t);

        MB('!', '=', Not, Ne)
        MB('<', '=', Lt, Le)
        MB('>', '=', Gt, Ge)
        MB('=', '=', Assign, Eq)

#define BI(p, t) case p: return ch += 2, t;

        BI('&', And)
        BI('|', Or)

        default:
            fatal("unknown token %c", c);
    }
}

std::vector<Token> Lexer::lex() {
    std::vector<Token> res;
    while (true) {
        while (*ch && std::isspace(*ch))
            ++ch;

        int c = *ch;
        if (!c)
            break;

        const char *start = ch;
        TokenKind tok;
        if (c == '/') {
            if (ch[1] == '*') {
                ch += 2;
                while (!(*ch == '*' && ch[1] == '/'))
                    ++ch;
                ch += 2;
                continue;
            } else if (ch[1] == '/') {
                ++ch;
                while (*++ch != '\n');
                ++ch;
                continue;
            } else
                tok = Div, ++ch;
        } else
            tok = next();

        // info("Found token %s", kind_name(tok));
        res.emplace_back(tok, start, ch - start, ch.ln);
    }
    return res;
}

std::vector<Token> lex(char *s) {
    return Lexer(s).lex();
}

const char *Token::kind_name() const {
    return ::kind_name(kind);
}

bool Token::is_a(TokenKind k) const{
    return kind == k;
}

Token::operator string() const {
    return {source, len};
}
