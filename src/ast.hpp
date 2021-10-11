#pragma once

#include "lexer.hpp"


namespace ir {
struct Value;
struct Func;
struct PrintfFunc;
struct Builder;
}

namespace ast {

struct Symbol {
    string name;

    explicit Symbol(string);

    virtual ~Symbol() = default;
};

struct Expr {
    virtual int eval();

    virtual ir::Value *build(ir::Builder *) = 0;
};

struct Decl : Symbol {
    bool is_const;

    bool has_init;
    std::vector<int> dims;
    std::vector<Expr *> init;

    uint addr;  // for globals

    ir::Value *value;

    Decl(bool is_const, const string &name, bool has_init = false);

    uint size() const;  // without 4
};

struct Func;


struct LVal : Expr {
    Decl *var;
    std::vector<Expr *> dims;

    int eval() override;

    ir::Value *build(ir::Builder *) override;
};

struct Number : Expr {
    int val;

    explicit Number(int);

    int eval() override;

    ir::Value *build(ir::Builder *) override;
};

struct Binary : Expr {
    TokenKind op;
    Expr *lhs, *rhs;

    Binary(TokenKind op, Expr *lh, Expr *rh);

    int eval() override;

    ir::Value *build(ir::Builder *) override;
};

struct Call : Expr {
    Func *func;
    std::vector<Expr *> args;

    ir::Value *build(ir::Builder *) override;
};


struct Stmt {
    // virtual ~Stmt() = default;
    virtual void build(ir::Builder *) = 0;
};

struct Assign : Stmt {
    LVal *lhs;
    Expr *rhs;

    explicit Assign(LVal *l, Expr *r);

    void build(ir::Builder *) override;
};

struct DeclStmt : Stmt {
    std::vector<Decl *> vars;

    void build(ir::Builder *) override;
};

struct ExprStmt : Stmt {
    Expr *val;

    explicit ExprStmt(Expr *e);

    void build(ir::Builder *) override;
};

struct Dummy : Stmt {
    void build(ir::Builder *) override;
};

struct Block : Stmt {
    std::vector<Stmt *> stmts;

    void build(ir::Builder *) override;
};

struct If : Stmt {
    Expr *cond;
    Stmt *body_then;
    Stmt *body_else; // nullable

    void build(ir::Builder *) override;
};

struct While : Stmt {
    Expr *cond;
    Stmt *body;

    void build(ir::Builder *) override;
};

struct Break : Stmt {
    void build(ir::Builder *) override;
};

struct Continue : Stmt {
    void build(ir::Builder *) override;
};

struct Return : Stmt {
    Expr *val; // nullable

    void build(ir::Builder *) override;
};

struct GetInt : Stmt {
    LVal *lhs;

    explicit GetInt(LVal *l);

    void build(ir::Builder *) override;
};

struct Printf : Stmt {
    const char *fmt;
    std::size_t len;
    std::vector<Expr *> args;

    ir::PrintfFunc *func;

    void build(ir::Builder *) override;
};

struct Func : Symbol {
    bool returns_int;

    std::vector<Decl *> params;
    Block body;

    ir::Func *ir;

    Func(const string &name, bool returns_int);

    Func(const string &name, bool returns_int, Block body);
};

struct Prog {
    std::vector<Decl *> globals;
    std::vector<Func *> funcs;
    std::vector<Printf *> printfs;
};

int evals(Expr **e);

}
