#include "parser.hpp"
#include "common.hpp"
#include "prompt.hpp"
#include "symbol.hpp"

using namespace tkd;
using namespace ast;

const int MAX_OP_LEVEL = 6, LEVEL_ADD = 2, LEVEL_OR = 6;

static int level(TokenKind op) {
    switch (op) {
        case Mul: case Div: case Mod:
            return 1;
        case Add: case Sub:
            return 2;
        case Le: case Ge: case Lt: case Gt:
            return 3;
        case Eq: case Ne:
            return 4;
        case And:
            return 5;
        case Or:
            return 6;
        default:
            return 0;
    }
}

static const char *level_sym[MAX_OP_LEVEL + 1] =
        { "UnaryExp", "MulExp", "AddExp", "RelExp", "EqExp", "LAndExp", "LOrExp" };

struct Parser {
    const Token *tok, *tok_end;
    std::ostream &out;

    SymbolTable ctx;
    vector<ast::Printf *> printfs;

    explicit Parser(const std::vector<Token> &toks) :
        tok(toks.data()),
        tok_end(tok + toks.size()),
        out(get_os()) {}

    /*
    const Token *peek(int i) const {
        const Token *p = tok + i;
        if (p >= tok_end)
            return nullptr;
        return p;
    }
    */

    const Token *get_unchecked() {
        auto name = tok->kind_name();
        // debug("getting token %s (%s)", name, string{tok->source, tok->len}.data());
        out << name << ' ';
        out.write(tok->source, tok->len);
        out << '\n';
        return tok++;
    }

    const Token *peek() const {
        if (tok >= tok_end)
            fatal("unexpected end of token stream");

        return tok;
    }

    bool try_peek() const {
        return tok < tok_end;
    }

    const Token *get_a(TokenKind k) {
        if (tok >= tok_end)
            fatal("expected %s (%d) but reached end", kind_name(k), k);
        if (!tok->is_a(k))
            fatal("expected %s (%d) but got %s (%d)", kind_name(k), k, kind_name(tok->kind), tok->kind);

        return get_unchecked();
    }

    bool tok_is_a(TokenKind k) const {
        return tok < tok_end && tok->kind == k;
    }

    bool try_get_a(TokenKind k) {
        if (tok_is_a(k)) {
            get_unchecked();
            return true;
        }
        return false;
    }

    bool peek_is_a(int i, TokenKind k) const {
        const Token *p = tok + i;
        return p < tok_end && p->kind == k;
    }

    template <typename T>
    void insert_symbols(std::vector<T> &decls) {
        // This requires that provided container is active and don't
        // change within the lifetime of the symbol table.
        for (T &x : decls)
            ctx.insert(x);
    }

#define REPORT(sym) (out << "<" sym ">\n")
#define REPORT_S(sym) (out << "<" << (sym) <<  ">\n")

    // Omit Decl and BType

    Prog comp_unit() {
        // CompUnit → {Decl} {FuncDef} MainFuncDef
        Prog res;
        while (true) {
            if (tok_is_a(tkd::Const))
                const_decl(res.globals);
            else if (peek_is_a(2, tkd::LPar))
                break;
            else
                var_decl(res.globals);
        }

        for (auto *g: res.globals)
            for (auto &p: g->init)
                evals(&p);

        // Don't do this earlier.
        // insert_symbols(res.globals);

        while (true) {
            if (peek_is_a(1, tkd::Main)) {
                res.funcs.push_back(main_func_def());
                break;
            } else
                res.funcs.push_back(func_def());
        }
        res.printfs = std::move(printfs);

        REPORT("CompUnit");
        return res;
    }

    void const_decl(vector<Decl *> &res) {
        get_a(tkd::Const);
        get_a(tkd::Int);
        res.push_back(const_def());
        while (try_get_a(tkd::Comma))
            res.push_back(const_def());
        get_a(tkd::Semi);
        REPORT("ConstDecl");
    }

    Decl *const_def() {
        auto *res = new Decl{true, string(*get_a(Ident)), true};
        while (try_get_a(LBrk)) {
            res->dims.push_back(const_exp());
            get_a(tkd::RBrk);
        }

        get_a(tkd::Assign);
        const_init(res->init);

        ctx.insert(res);
        REPORT("ConstDef");
        return res;
    }

    void const_init(vector<Expr *> &res) {
        // FIRST(ConstExp) = FIRST(UnaryExp) = FIRST(PrimaryExp) = { LPar, Ident, IntConst }
        if (try_get_a(tkd::LBrc)) {
            // FIRST(ConstInitVal) = FIRST(ConstExp) + { LBrc }
            if (!try_get_a(RBrc)) {
                do {
                    const_init(res);
                } while (try_get_a(Comma));
                get_a(RBrc);
            }
        } else
            res.push_back(new Number{const_exp()});
        REPORT("ConstInitVal");
    }

    void var_decl(vector<Decl *> &res) {
        get_a(Int);
        do {
            res.push_back(var_def());
        } while (try_get_a(Comma));

        get_a(Semi);
        REPORT("VarDecl");
    }

    Decl *var_def() {
        auto *res = new Decl{false, string(*get_a(Ident))};
        while (try_get_a(LBrk)) {
            int dim = const_exp();
            res->dims.push_back(dim);
            debug("got a dim %d for %s", dim, res->name.data());
            get_a(RBrk);
        }

        // FOLLOW(VarDef) = { Semi, Comma }
        if (try_get_a(tkd::Assign)) {
            res->has_init = true;
            init(res->init);
        }

        ctx.insert(res);
        REPORT("VarDef");
        return res;
    }

    void init(vector<Expr *> &res) {
        // FIRST(Exp) = FIRST(AddExp) = FIRST(UnaryExp)
        // Copied from const_init
        if (try_get_a(LBrc)) {
            if (!try_get_a(RBrc)) {
                do {
                    init(res);
                } while (try_get_a(Comma));
                get_a(RBrc);
            }
        } else
            res.push_back(exp());

        REPORT("InitVal");
    }

    ast::Func *func_def() {
        bool returns_int = func_type();
        string name(*get_a(Ident));
        get_a(LPar);

        // FIRST(FuncFParams) = { Int }
        auto *res = new ast::Func{name, returns_int};
        if (!try_get_a(RPar)) {
            func_formal_params(res->params);
            get_a(RPar);
        }
        ctx.insert(res); // support recursive

        debug("Entering func %s", name.data());
        ctx.push();
        for (Decl *x: res->params)
            ctx.insert(x);
        res->body = block(false);
        ctx.pop();
        debug("Exiting func %s", name.data());

        REPORT("FuncDef");
        return res;
    }

    ast::Func *main_func_def() {
        get_a(Int);
        get_a(Main);
        get_a(LPar);
        get_a(RPar);
        auto body = block();
        REPORT("MainFuncDef");
        return new ast::Func{"main", true, body};
    }

    bool func_type() {
        bool res = true;
        if (!try_get_a(Int)) {
            res = false;
            get_a(Void);
        }
        REPORT("FuncType");
        return res;
    }

    void func_formal_params(vector<Decl *> &res) {
        // FOLLOW(FuncFParams) = { RPar }
        do {
            res.push_back(func_formal_param());
        } while (try_get_a(Comma));

        REPORT("FuncFParams");
    }

    Decl *func_formal_param() {
        get_a(tkd::Int);
        auto *res = new Decl{false, string(*get_a(Ident))};

        // FOLLOW(FuncFParam) = { Comma, RPar }
        if (try_get_a(LBrk)) {
            res->dims.push_back(-1);
            get_a(RBrk);
            while (try_get_a(LBrk)) {
                res->dims.push_back(const_exp());
                get_a(RBrk);
            }
        }

        REPORT("FuncFParam");
        return res;
    }

    Block block(bool push = true) {
        get_a(LBrc);

        // FIRST(Decl) = { Const, Int }
        // FIRST(Stmt} = { Ident, Comma, {}(Exp), LBrc, If, While, Break, Continue, Return, printf }
        Block res;
        if (push)
            ctx.push();
        while (!try_get_a(RBrc))
            res.stmts.push_back(block_item());
        if (push)
            ctx.pop();

        REPORT("Block");
        return res;
    }

    Stmt *block_item() {
        const auto *tk = peek();
        if (tk->is_a(Const)) {
            auto *res = new DeclStmt;
            const_decl(res->vars);
            return res;
        }
        if (tk->is_a(Int)) {
            auto *res = new DeclStmt;
            var_decl(res->vars);
            return res;
        }
        return statement();
    }

    Stmt *statement() {
        Stmt *res;
        switch (peek()->kind) {
            case tkd::If: {
                get_unchecked();
                auto *r = new ast::If;
                get_a(LPar);
                r->cond = cond();
                get_a(RPar);
                r->body_then = statement();
                if (try_get_a(Else))
                    r->body_else = statement();
                else
                    r->body_else = nullptr;
                res = r;
                goto stmt_no_semi;
            }

            case tkd::While: {
                get_unchecked();
                get_a(LPar);
                auto *r = new ast::While;
                r->cond = cond();
                get_a(RPar);
                r->body = statement();
                res = r;
                goto stmt_no_semi;
            }

            case tkd::Break:
                get_unchecked();
                res = new ast::Break;
                break;

            case tkd::Continue:
                get_unchecked();
                res = new ast::Continue;
                break;

            case tkd::Return: {
                get_unchecked();
                auto *r = new ast::Return{};
                if (!tok_is_a(Semi))
                    r->val = exp();
                res = r;
                break;
            }

            case tkd::Printf: {
                get_unchecked();
                get_a(LPar);
                auto *fmt = get_a(FmtStr);
                auto *r = new ast::Printf;
                r->fmt = fmt->source;
                r->len = fmt->len;
                while (try_get_a(Comma))
                    r->args.push_back(exp());
                get_a(RPar);
                printfs.push_back(r);
                res = r;
                break;
            }

            case LBrc:
                res = new Block{block()};
                goto stmt_no_semi;

            case Semi:
                get_unchecked();
                res = new Dummy;
                goto stmt_no_semi;

            case Ident: {
                const Token *c = tok;
                bool is_exp = true;
                while (c < tok_end && !c->is_a(Semi)) {
                    if (c->is_a(tkd::Assign)) {
                        is_exp = false;
                        break;
                    }
                    ++c;
                }
                if (c >= tok_end)
                    fatal("stmt not finalized");

                if (!is_exp) {
                    LVal *lhs = lvalue();
                    get_a(tkd::Assign);
                    if (try_get_a(tkd::GetInt)) {
                        res = new ast::GetInt{lhs};
                        get_a(LPar);
                        get_a(RPar);
                    } else
                        res = new ast::Assign{lhs, exp()};
                    break;
                }
            }
                // fall through

            default:
                res = new ExprStmt{exp()};
                break;
        }

        get_a(Semi);
stmt_no_semi:
        REPORT("Stmt");
        return res;
    }

    Expr *exp() {
        auto *res = bin_exp<LEVEL_ADD>();
        REPORT("Exp");
        return res;
    }

    Expr *cond() {
        auto *res = bin_exp<LEVEL_OR>();
        REPORT("Cond");
        return res;
    }

    LVal *lvalue() {
        auto *res = new LVal;
        res->var = ctx.find_a<Decl>(string(*get_a(Ident)));
        // FOLLOW(LVal) = binary operators and Assign
        while (try_get_a(LBrk)) {
            res->dims.push_back(exp());
            get_a(RBrk);
        }
        REPORT("LVal");
        return res;
    }

    Expr *primary_exp() {
        const auto *tk = peek();
        Expr *res;
        if (tk->is_a(LPar)) {
            get_unchecked();
            res = exp();
            get_a(RPar);
        } else if (tk->is_a(IntConst))
            res = number();
        else
            res = lvalue();
        REPORT("PrimaryExp");
        return res;
    }

    Number *number() {
        int val = std::stoi(string(*get_a(IntConst)));
        REPORT("Number");
        return new Number{val};
    }

    Expr *sub_unary() {
        get_unchecked();
        REPORT("UnaryOp");
        return unary_exp();
    }

    Expr *unary_exp() {
        const auto *tk = peek();
        auto op = tk->kind;
        Expr *res;
        switch (op) {
            case Add:
                res = sub_unary();
                break;

            case Sub:
                res = new Binary(tkd::Sub, new Number(0), sub_unary());
                break;

            case Not:
                res = new Binary(tkd::Eq, sub_unary(), new Number(0));
                break;

            case Ident:
                // FOLLOW(LVal) = binary operators and Assign
                if (peek_is_a(1, LPar)) {
                    get_unchecked();
                    get_a(LPar);
                    auto *r = new Call;
                    r->func = ctx.find_a<ast::Func>(string(*tk));
                    if (!try_get_a(RPar)) {
                        func_real_params(r->args);
                        get_a(RPar);
                    }
                    res = r;
                    break;
                }
                // fall through

            default:
                res = primary_exp();
        }
        REPORT("UnaryExp");
        return res;
    }

    void func_real_params(vector<Expr *> &args) {
        // FOLLOW(FuncRParams) = { RPar }
        do {
            args.push_back(exp());
        } while (try_get_a(Comma));

        REPORT("FuncRParams");
    }

    template <int L>
    Expr *bin_exp() {
        Expr *lhs = bin_exp<L - 1>();
        REPORT_S(level_sym[L]);
        TokenKind kind;
        while (try_peek() && level(kind = tok->kind) == L) {
            get_unchecked();
            Expr *rhs = bin_exp<L - 1>();
            lhs = new Binary(kind, lhs, rhs);
            REPORT_S(level_sym[L]);
        }
        return lhs;
    }

    int const_exp() {
        int v = bin_exp<LEVEL_ADD>()->eval();
        REPORT("ConstExp");
        return v;
    }
};

template <>
Expr *Parser::bin_exp<0>() {
    return unary_exp();
}

Prog parse(const vector<Token> &tokens) {
    return Parser(tokens).comp_unit();
}
