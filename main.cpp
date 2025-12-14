// main.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include "Lexer.h"
#include "Token.h"
#include "parser.h"
#include "ast_printer.h"
#include "Sema.h"
#include "CodeGen.h"
#include "trace.h"
#include "ast_json.h"
#include "tokens_json.h"

// -----------------------------------------
// CmdOptions + parseArgs
// -----------------------------------------
struct CmdOptions {
    bool run = true;
    std::string irPath = "output.ll";
    bool emitObj = false;
    std::string objPath;
    std::string sourceFile;
};

static void printUsage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " [options] <source-file>\n\n"
              << "Options:\n"
              << "  --emit-ir=<file>      Save LLVM IR to <file> (default: output.ll)\n"
              << "  --no-run              Do not run the program via JIT\n"
              << "  --run                 Force run via JIT (default)\n"
              << "  --emit-obj[=<file>]   Emit object file (default: output.o)\n";
}

static bool parseArgs(int argc, char** argv, CmdOptions& opts) {
    if (argc < 2) {
        printUsage(argv[0]);
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg.rfind("--emit-ir=", 0) == 0) {
            opts.irPath = arg.substr(std::string("--emit-ir=").size());
        } else if (arg == "--no-run") {
            opts.run = false;
        } else if (arg == "--run") {
            opts.run = true;
        } else if (arg.rfind("--emit-obj=", 0) == 0) {
            opts.emitObj = true;
            opts.objPath = arg.substr(std::string("--emit-obj=").size());
        } else if (arg == "--emit-obj") {
            opts.emitObj = true;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return false;
        } else {
            if (!opts.sourceFile.empty()) {
                std::cerr << "Multiple source files are not supported.\n";
                return false;
            }
            opts.sourceFile = arg;
        }
    }

    if (opts.sourceFile.empty()) {
        std::cerr << "No source file specified.\n";
        printUsage(argv[0]);
        return false;
    }

    return true;
}

// -----------------------------------------
// MAIN
// -----------------------------------------
int main(int argc, char** argv) {

    uint64_t t_start = now_ms();

    CmdOptions opts;
    if (!parseArgs(argc, argv, opts)) {
        return 1;
    }

    const char* filename = opts.sourceFile.c_str();
    std::ifstream in(filename);
    if (!in) {
        std::cerr << "Error: cannot open file: " << filename << "\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string source = buffer.str();

    // === Lexer ===
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenizeAll();
    {
        std::ofstream jt("tokens.json");
        jt << tokensToJson(tokens);
    }

    if (tokens.empty() || tokens.back().type != TokenType::END_OF_FILE) {
        tokens.emplace_back(TokenType::END_OF_FILE, "", 0, 0);
    }

    uint64_t t_after_lex = now_ms();
    std::cerr << "[TRACE] lexer_ms=" << (t_after_lex - t_start) << "\n";

    std::cout << "\n=== TOKENS ===\n";
    for (const Token& tok : tokens) {
        std::cout
            << "Line " << tok.line
            << ", Col " << tok.column
            << " | " << tokenTypeToString(tok.type)
            << " | \"" << tok.lexeme << "\"\n";
    }

    // === Parser ===
    Parser parser(tokens);
    ASTNodePtr ast;

    try {
        ast = parser.parseProgram();
    } catch (const ParseException&) {}

    if (parser.hasErrors()) {
        const auto& errs = parser.getErrors();
        std::cerr << "\n=== PARSE ERRORS (" << errs.size() << ") ===\n";
        for (std::size_t i = 0; i < errs.size(); ++i) {
            const auto& e = errs[i];
            std::cerr << "  [" << (i + 1) << "] "
                      << "Line " << e.line << ", Col " << e.column
                      << " : " << e.message << "\n";
        }
        return 1;
    }

    uint64_t ast_nodes = countAST(ast);
    std::cerr << "[TRACE] ast_nodes=" << ast_nodes << "\n";

    uint64_t t_after_parse = now_ms();
    std::cerr << "[TRACE] parser_ms=" << (t_after_parse - t_after_lex) << "\n";

    std::cout << "\n=== PARSE OK ===\n";
    printAST(ast);
    {
        std::ofstream js("ast.json");
        js << astToJson(ast);
    }

    std::cout << "\n=== SEMANTIC ANALYSIS ===\n";

    Sema sema(ast);
    bool semaOK = sema.analyze();

    if (!semaOK) {
        std::cerr << "\n=== SEMANTIC ERRORS DETECTED ===\n";
        return 1;
    }

    uint64_t t_after_sema = now_ms();
    std::cerr << "[TRACE] sema_ms=" << (t_after_sema - t_after_parse) << "\n";

    std::cout << "=== SEMANTIC OK ===\n";

    CodeGen codegen("MainModule");
    bool ok = codegen.compile(ast);
    if (!ok) {
        std::cerr << "Code generation failed.\n";
        return 1;
    }

    uint64_t t_after_codegen = now_ms();
    std::cerr << "[TRACE] codegen_ms=" << (t_after_codegen - t_after_sema) << "\n";

    if (!codegen.saveIRToFile(opts.irPath)) {
        std::cerr << "Warning: failed to save IR to file.\n";
    }

    uint64_t t_after_emit = now_ms();
    std::cerr << "[TRACE] emit_ms=" << (t_after_emit - t_after_codegen) << "\n";

    if (opts.emitObj) {
        std::string objPath = opts.objPath.empty() ? "output.o" : opts.objPath;
        if (!codegen.emitObjectFile(objPath)) {
            std::cerr << "ERROR: failed to emit object file.\n";
            return 1;
        }
    }

    int exitCode = 0;
    if (opts.run) {
        exitCode = codegen.runMain();
    }

    return exitCode;
}
