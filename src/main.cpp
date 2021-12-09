#include "parser.hpp"
#include "common.hpp"
#include "prompt.hpp"
#include "ir_builder.hpp"
#include "mips_builder.hpp"
#include "passes.hpp"

#include <iostream>
#include <fstream>
#include <cstdlib>

#ifdef SYC_DEBUG
    #include <cerrno>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

constexpr const char *input_file = "testfile.txt";
// constexpr const char *output_file = "output.txt";
// constexpr const char *output_file = "error.txt";
constexpr const char *output_file = "mips.txt";

string read_file(const char *file) {
    std::ifstream inf(file, std::ios::binary | std::ios::ate);
    if (!inf) {
        std::perror("Cannot open input file");
        std::quick_exit(1);
    }
    auto size = inf.tellg();
    info("input file has size %d", size);
    inf.seekg(0);
    std::string s(size, 0);
    inf.read(&s[0], size);
    return s;
}

constexpr int buf_size = 100;
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
        return {read_stdin(), nullptr};
}

#ifdef SYC_DEBUG
template <class T>
void debug_put(T &x, const char *file) {
    std::ofstream f(file);
    f << x;
}
#else
    #define debug_put(...) void(0)
#endif

const char *check(vector<Token> &tokens);

void work(int argc, char **argv) {
    std::ofstream outf;
    std::ostream *out = &outf;

#ifdef SYC_STDIN
    auto p = parse_args(argc, argv);
    auto &src = p.first;
    if (p.second)
        outf.open(p.second);
    else
        out = &std::cout;
#else
    auto src = read_file(input_file);
    outf.open(output_file);
#endif

#ifdef SYC_STDOUT
    set_os(std::cout);
#else
    std::ofstream prompt_outf(output_file);
    set_os(prompt_outf);
#endif

#ifdef SYC_DEBUG
    info("debug build");
    errno = 0;
    int r = mkdir("syc_tmp", 0755);
    if (r < 0 && errno != EEXIST) {
        std::perror("Cannot create temp dir");
        std::quick_exit(1);
    }
    r = chdir("syc_tmp");
    if (r < 0) {
        std::perror("Cannot chdir to temp dir");
        std::quick_exit(1);
    }
#endif

    info("read %zu bytes", src.size());

    vector<Token> tokens = lex(&src[0]);
    ast::Prog ast = parse(tokens);

    HANDLE_ERR(
        put_errs();
        return 0;
    )

    ir::Prog ir = build_ir(std::move(ast));
    debug_put(ir, "ir.txt");

    run_passes(ir);
    debug_put(ir, "ir2.txt");

    mips::Prog mr = build_mr(ir);
    debug_put(mr, "mr.asm");

    run_mips_passes(mr);
    debug_put(mr, "mr2.asm");

    *out << mr;

    info("bye");
    *out << ".text\nnop\n";
}

int main(int argc, char **argv) {
    work(argc, argv);
    ir::no_value_check = true;
    std::quick_exit(0);
}
