#include "parser.hpp"
#include "common.hpp"
#include "prompt.hpp"
#include "ir_builder.hpp"
#include "passes.hpp"
#include "mips_builder.hpp"

#include <fstream>
#include <cstring>
#include <cerrno>

constexpr const char *input_file = "testfile.txt";
constexpr const char *output_file = "output.txt";

std::string read_file(const char *file) {
    std::ifstream inf(file, std::ios::binary | std::ios::ate);
    if (!inf)
        fatal("Cannot open input file: %s", std::strerror(errno));
    auto size = inf.tellg();
    info("input file has size %d", size);
    inf.seekg(0);
    std::string s(size, 0);
    inf.read(&s[0], size);
    return s;
}

constexpr int buf_size = 10;
char read_buf[buf_size + 1];

std::string read_stdin() {
    auto *buf = std::cin.rdbuf();
    auto n = buf->sgetn(read_buf, buf_size);
    std::string s(read_buf, n);
    while (n >= buf_size) {
        n = buf->sgetn(read_buf, buf_size);
        s.append(read_buf, n);
    }
    return s;
}

int main() {
#ifdef SYC_STDIN
    auto src = read_stdin();
#else
    auto src = read_file(input_file);
#endif

#ifdef SYC_STDOUT
    set_os(std::cout);
#else
    std::ofstream outf(output_file);
    set_os(outf);
#endif

#ifdef SYC_DEBUG
    info("debug build");
#endif

    info("read %d bytes", src.size());

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
        std::ofstream mrf("mr2.asm");
        mrf << mr;
    }


    return 0;
}
