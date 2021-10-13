#include "parser.hpp"
#include "common.hpp"
#include "prompt.hpp"
#include "ir_builder.hpp"
#include "mips_builder.hpp"
#include "passes.hpp"

#include <iostream>
#include <fstream>

constexpr const char *input_file = "testfile.txt";
constexpr const char *output_file = "output.txt";

string read_file(const char *file) {
    std::ifstream inf(file, std::ios::binary | std::ios::ate);
    if (!inf) {
        std::perror("Cannot open input file");
        std::exit(1);
    }
    auto size = inf.tellg();
    info("input file has size %d", size);
    inf.seekg(0);
    std::string s(size, 0);
    inf.read(&s[0], size);
    return s;
}

constexpr int buf_size = 10;
char read_buf[buf_size + 1];

string read_stdin() {
    auto *buf = std::cin.rdbuf();
    auto n = buf->sgetn(read_buf, buf_size);
    std::string s(read_buf, n);
    while (n >= buf_size) {
        n = buf->sgetn(read_buf, buf_size);
        s.append(read_buf, n);
    }
    return s;
}

std::pair<string, const char *> parse_args(int argc, char **argv) {
    if (argc > 1) {
        if (argc >= 4)  // syc src.c -o out.asm
            return {read_file(argv[1]), argv[3]};
        return {read_file(argv[1]), nullptr};
    } else
        return {read_file(input_file), nullptr};
}

int main(int argc, char **argv) {
    std::ofstream outf;
    std::ostream *out;

#ifdef SYC_STDIN
    auto p = parse_args(argc, argv);
    auto &src = p.first;
    if (p.second) {
        outf.open(p.second);
        out = &outf;
    } else
        out = &std::cout;
#else
    auto src = read_file(input_file);
#endif

#ifdef SYC_STDOUT
    set_os(std::cout);
#else
    std::ofstream prompt_outf(output_file);
    set_os(prompt_outf);
#endif

#ifdef SYC_DEBUG
    info("debug build");
#endif

    info("read %zu bytes", src.size());

    vector<Token> tokens = lex(&src[0]);
    ast::Prog ast = parse(tokens);

    ir::Prog ir = build_ir(std::move(ast));
    {
        std::ofstream irf("ir.txt");
        irf << ir;
    }

    run_passes(ir);
    {
        std::ofstream irf("ir2.txt");
        irf << ir;
    }

    mips::Prog mr = build_mr(ir);
    {
        std::ofstream mrf("mr.asm");
        mrf << mr;
    }

    run_mips_passes(mr);
    {
        // std::ofstream mrf("mr2.asm");
    }
    *out << mr;

    info("bye");
    return 0;
}
