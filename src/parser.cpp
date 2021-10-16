#include "parser.hpp"
#include "common.hpp"
#include "prompt.hpp"
#include "symbol.hpp"
#include "util.hpp"

using namespace tkd;
using namespace ast;

#ifdef SYC_SYNTAX_PROMPT
constexpr int MAX_OP_LEVEL = 6;
static const char *level_sym[MAX_OP_LEVEL + 1] =
        { "UnaryExp", "MulExp", "AddExp", "RelExp", "EqExp", "LAndExp", "LOrExp" };
#endif

constexpr int LEVEL_ADD = 2, LEVEL_OR = 6;

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

HANDLE_ERR(
bool check_args(const vector<Expr *> &args, const vector<Decl *> &params) {
    uint n = args.size();
    asserts(n == params.size());
    for (uint i = 0; i < n; ++i) {
        auto *arg = args[i];
        auto *par = params[i];
        if_a (LVal, x, arg) {
            if (!x->var)  // allow null Decl * from ctx
                return false;
            if (x->var->dims.size() != x->dims.size() + par->dims.size())
                return false;
            if (par->dims.size() == 2 && par->dims[1] != x->var->dims[1])
                return false;
            continue;
        }
        if (!par->dims.empty())
            return false;
        if_a (Call, x, arg)
            if (x->func && !x->func->returns_int)
                return false;
    }
    return true;
}
)

struct Parser {
    const Token *tok, *tok_end;
    std::ostream &out;

    SymbolTable ctx;
    vector<ast::Printf *> printfs;

#ifdef SYC_ERROR_PROMPT
    bool cur_func_returns_int;
    int while_stk = 0;
    int last_ln = 0;

    #define RLN int this_ln = tok->ln;
    #define RLNS last_ln = std::max(last_ln, this_ln);
#else
    #define RLN
    #define RLNS
#endif

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
        debug("getting token %s (%s) at ln %d", name, string{tok->source, tok->len}.data(), tok->ln);
#ifdef SYC_SYNTAX_PROMPT
        out << name << ' ';
        out.write(tok->source, tok->len);
        out << '\n';
#endif
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
        if (!tok->is_a(k)) {

#ifdef SYC_ERROR_PROMPT
            if (k == Semi)
                push_err('i', (tok - 1)->ln);
            else if (k == RPar)
                push_err('j', (tok - 1)->ln);
            else if (k == RBrk)
                push_err('k', (tok - 1)->ln);
            else
                throw std::exception{};
            return nullptr;
#else
            fatal("expected %s (%d) but got %s (%d)", kind_name(k), k, kind_name(tok->kind), tok->kind);
#endif

        }

        HANDLE_ERR(
            if (k == Int)
                last_ln = std::max(last_ln, tok->ln);
        )

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

HANDLE_ERR(
    const Token *peek_last() const {
        return tok - 1;
    }
)

#ifdef SYC_SYNTAX_PROMPT
    #define REPORT(sym) RLNS (out << "<" sym ">\n")
    #define REPORT_S(sym) RLNS (out << "<" << (sym) <<  ">\n")
#else
    #define REPORT(sym) RLNS void(0)
    #define REPORT_S(sym) RLNS void(0)
#endif

    void ctx_insert(Symbol *s, const Token *ident) {
        // TODO: ignore or override?
        if (!ctx.insert(s)) {

#ifdef SYC_ERROR_PROMPT
            push_err('b', ident->ln);
#else
            (void)(ident);
            fatal("redefined symbol %s", s->name.data());
#endif

        }
    }

    template <class T>
    T *ctx_find(const Token *ident) const {
        string s{*ident};
        T *res = ctx.find_a<T>(s);
        if (res == nullptr) {
#ifdef SYC_ERROR_PROMPT
            if (ctx.find(s) == nullptr)
                push_err('c', ident->ln);
            // TODO: what to return now?
#else
            string s{*ident};
            fatal("symbol %s is not a %s", s.data(), typeid(T).name());
#endif
        }
        return res;
    }

    // Omit Decl and BType

    Prog comp_unit() { RLN
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

        while (true) {
            if (peek_is_a(1, Main)) {
                res.funcs.push_back(main_func_def());
                break;
            } else
                res.funcs.push_back(func_def());
        }
        res.printfs = std::move(printfs);

        REPORT("CompUnit");
        return res;
    }

    void const_decl(vector<Decl *> &res) { RLN
        get_a(tkd::Const);
        get_a(tkd::Int);
        res.push_back(const_def());
        while (try_get_a(tkd::Comma))
            res.push_back(const_def());
        get_a(tkd::Semi);
        REPORT("ConstDecl");
    }

    Decl *const_def() { RLN
        auto *ident = get_a(Ident);
        auto *res = new Decl{true, string(*ident), true};
        while (try_get_a(LBrk)) {
            res->dims.push_back(const_exp());
            get_a(tkd::RBrk);
        }

        get_a(tkd::Assign);
        const_init(res->init);

        ctx_insert(res, ident);
        REPORT("ConstDef");
        return res;
    }

    void const_init(vector<Expr *> &res) { RLN
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

    void var_decl(vector<Decl *> &res) { RLN
        get_a(Int);
        do {
            res.push_back(var_def());
        } while (try_get_a(Comma));
        get_a(Semi);
        REPORT("VarDecl");
    }

    Decl *var_def() { RLN
        auto *ident = get_a(Ident);
        auto *res = new Decl{false, string(*ident)};
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

        ctx_insert(res, ident);
        REPORT("VarDef");
        return res;
    }

    void init(vector<Expr *> &res) { RLN
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

    ast::Func *func_def() { RLN
        bool returns_int = func_type();
        HANDLE_ERR(cur_func_returns_int = returns_int;)
        auto *ident = get_a(Ident);
        string name{*ident};
        get_a(LPar);

        // FIRST(FuncFParams) = { Int }
        auto *res = new ast::Func{name, returns_int};

#ifdef SYC_ERROR_PROMPT
        vector<std::pair<Decl *, const Token *>> params;
#endif

        if (!try_get_a(RPar)) {
#ifdef SYC_ERROR_PROMPT
            if (tok_is_a(LBrc))
                push_err('j', (tok - 1)->ln);
            else {
                func_formal_params(params);
                get_a(RPar);
            }
#else
            func_formal_params(res->params);
            get_a(RPar);
#endif
        }
        ctx_insert(res, ident);  // for recursive

        debug("Entering func %s", name.data());
        ctx.push();
#ifdef SYC_ERROR_PROMPT
        for (auto &p: params) {
            ctx_insert(p.first, p.second);
            res->params.push_back(p.first);
        }
#else
        for (Decl *x: res->params)
            ctx.insert(x);
#endif
        res->body = block(false);
        ctx.pop();
        debug("Exiting func %s", name.data());

        HANDLE_ERR(
            if (returns_int && (res->body.stmts.empty() || !is_a<ast::Return>(res->body.stmts.back())))
                push_err('g', peek_last()->ln);
        )

        REPORT("FuncDef");
        return res;
    }

    ast::Func *main_func_def() { RLN
        HANDLE_ERR(cur_func_returns_int = true;)
        get_a(Int);
        get_a(Main);
        get_a(LPar);
        get_a(RPar);
        auto body = block();

        HANDLE_ERR(
            if (body.stmts.empty() || !is_a<ast::Return>(body.stmts.back()))
                push_err('g', peek_last()->ln);
        )
        // TODO: does this apply to MainFuncDef?

        REPORT("MainFuncDef");
        return new ast::Func{"main", true, body};
    }

    bool func_type() { RLN
        bool res = true;
        if (!try_get_a(Int)) {
            res = false;
            get_a(Void);
        }
        REPORT("FuncType");
        return res;
    }

#ifdef SYC_ERROR_PROMPT
    void func_formal_params(vector<std::pair<Decl *, const Token *>> &res) { RLN
#else
    void func_formal_params(vector<Decl *> &res) { RLN
#endif
        // FOLLOW(FuncFParams) = { RPar }
        do
            res.push_back(func_formal_param());
        while (try_get_a(Comma));

        REPORT("FuncFParams");
    }

#ifdef SYC_ERROR_PROMPT
    std::pair<Decl *, const Token *> func_formal_param() { RLN
#else
    Decl *func_formal_param() { RLN
#endif
        get_a(tkd::Int);
        auto *ident = get_a(Ident);
        auto *res = new Decl{false, string(*ident)};

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
#ifdef SYC_ERROR_PROMPT
        return {res, ident};
#else
        return res;
#endif
    }

    Block block(bool push = true) { RLN
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

    Stmt *block_item() { RLN
        const auto *tk = peek();
        if (tk->is_a(Const)) {
            auto *res = new DeclStmt;
            const_decl(res->vars);
            RLNS
            return res;
        }
        if (tk->is_a(Int)) {
            auto *res = new DeclStmt;
            var_decl(res->vars);
            RLNS
            return res;
        }
        auto *r = statement();
        RLNS
        return r;
    }

    Stmt *statement() { RLN
        Stmt *res;
        auto *tk = peek();
        switch (tk->kind) {
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
                HANDLE_ERR(++while_stk;)
                r->body = statement();
                HANDLE_ERR(--while_stk;)
                res = r;
                goto stmt_no_semi;
            }

            case tkd::Break:
                HANDLE_ERR(
                    if (!while_stk)
                        push_err('m', tk->ln);
                )
                get_unchecked();
                res = new ast::Break;
                break;

            case tkd::Continue:
                HANDLE_ERR(
                    if (!while_stk)
                        push_err('m', tk->ln);
                )
                get_unchecked();
                res = new ast::Continue;
                break;

            case tkd::Return: {
                get_unchecked();
                auto *r = new ast::Return;
                r->val = nullptr;
                if (!tok_is_a(Semi)) {
#ifdef SYC_ERROR_PROMPT
                    bool ok = true;
                    auto ctx_tok = tok;
                    auto ctx_ln = last_ln;
                    push_err_mask();
                    try {
                        r->val = exp();
                        if (tok_is_a(tkd::Assign))
                            throw std::exception{};
                    } catch (std::exception &e) {
                        ok = false;
                        tok = ctx_tok;
                        last_ln = ctx_ln;
                        pop_err_mask_reject();
                    }
                    if (ok) {
                        pop_err_mask_resolve();
                        if (!cur_func_returns_int)
                            push_err('f', tk->ln);
                    }
#else
                    r->val = exp();
#endif
                }
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

                HANDLE_ERR(
                    for (uint i = 1; i < fmt->len - 1; ++i) {
                        int c = fmt->source[i];
                        if (!(
                            (
                                (c == 32 || c == 33 || (c >= 40 && c <= 126)) &&
                                (c != '\\' || (i + 1 < fmt->len - 1 && fmt->source[i + 1] == 'n'))
                            ) ||
                            (c == '%' && i + 1 < fmt->len - 1 && fmt->source[i + 1] == 'd')
                        )) {
                            push_err('a', fmt->ln);
                            break;
                        }
                    }

                    uint cnt = 0;
                    for (uint i = 1; i < fmt->len - 1; ++i)
                        if (fmt->source[i] == '%' && i + 1 < fmt->len - 1 && fmt->source[i + 1] == 'd')
                            ++cnt;

                    if (r->args.size() != cnt)
                        push_err('l', tk->ln);
                )

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
#ifndef SYC_ERROR_PROMPT
                if (c >= tok_end)
                    fatal("stmt not finalized");
#endif

                if (!is_exp) {
#ifdef SYC_ERROR_PROMPT
                    bool ok = true;
                    auto ctx_tok = tok;
                    auto ctx_ln = last_ln;
                    push_err_mask();
                    try {
                        auto *lhs = lvalue_non_const();
                        get_a(tkd::Assign);
                        if (try_get_a(tkd::GetInt)) {
                            res = new ast::GetInt{lhs};
                            get_a(LPar);
                            get_a(RPar);
                        } else
                            res = new ast::Assign{lhs, exp()};
                    } catch (std::exception &e) {
                        ok = false;
                        tok = ctx_tok;
                        last_ln = ctx_ln;
                        pop_err_mask_reject();
                        // push_err('i', last_ln);
                    }
                    if (ok) {
                        pop_err_mask_resolve();
                        break;
                    }
#else
                    auto *lhs = lvalue();
                    get_a(tkd::Assign);
                    if (try_get_a(tkd::GetInt)) {
                        res = new ast::GetInt{lhs};
                        get_a(LPar);
                        get_a(RPar);
                    } else
                        res = new ast::Assign{lhs, exp()};
                    break;
#endif
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

    Expr *exp() { RLN
        auto *res = bin_exp<LEVEL_ADD>();
        REPORT("Exp");
        return res;
    }

    Expr *cond() { RLN
        auto *res = bin_exp<LEVEL_OR>();
        REPORT("Cond");
        return res;
    }

    LVal *lvalue() { RLN
        auto *res = new LVal;
        res->var = ctx_find<Decl>(get_a(Ident));
        // FOLLOW(LVal) = binary operators and Assign
        while (try_get_a(LBrk)) {
            res->dims.push_back(exp());
            get_a(RBrk);
        }
        REPORT("LVal");
        return res;
    }

HANDLE_ERR(
    LVal *lvalue_non_const() { RLN
        auto *res = new LVal;
        auto *ident = get_a(Ident);
        res->var = ctx_find<Decl>(ident);
        if (res->var && res->var->is_const)
            push_err('h', ident->ln);
        while (try_get_a(LBrk)) {
            res->dims.push_back(exp());
            get_a(RBrk);
        }
        REPORT("LVal");
        return res;
    }
)

    Expr *primary_exp() { RLN
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

    Number *number() { RLN
        int val = std::stoi(string(*get_a(IntConst)));
        REPORT("Number");
        return new Number{val};
    }

    Expr *sub_unary() { RLN
        get_unchecked();
        REPORT("UnaryOp");
        return unary_exp();
    }

    Expr *unary_exp() { RLN
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
                    r->func = ctx_find<ast::Func>(tk);
                    if (!try_get_a(RPar)) {
#ifdef SYC_ERROR_PROMPT
                        push_err_mask();
                        bool ok = true;
                        auto *ctx_tok = tok;
                        auto ctx_ln = last_ln;
                        try {
                            func_real_params(r->args);
                            get_a(RPar);
                        } catch (std::exception &e) { // missing RPar
                            r->args.clear();
                            ok = false;
                            tok = ctx_tok;
                            last_ln = ctx_ln;
                            pop_err_mask_reject();
                            push_err('j', (tok - 1)->ln);
                        }
                        if (ok)
                            pop_err_mask_resolve();
#else
                        func_real_params(r->args);
                        get_a(RPar);
#endif
                    }
                    HANDLE_ERR(
                        if (r->func) {
                            if (r->func->params.size() != r->args.size())
                                push_err('d', tk->ln);
                            else if (!check_args(r->args, r->func->params))
                                push_err('e', tk->ln);
                            // TODO: what if they co-exist?
                        }
                    )
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

    void func_real_params(vector<Expr *> &args) { RLN
        // FOLLOW(FuncRParams) = { RPar }
        do {
            args.push_back(exp());
        } while (try_get_a(Comma));

        REPORT("FuncRParams");
    }

    template <int L>
    Expr *bin_exp() { RLN
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

    int const_exp() { RLN
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
