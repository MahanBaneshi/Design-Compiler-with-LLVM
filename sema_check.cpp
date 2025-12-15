// sema_check.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include "Lexer.h"
#include "Token.h"
#include "parser.h"
#include "Sema.h"

static void printUsage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " <source-file>\n";
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printUsage(argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    std::ifstream in(filename);
    if (!in) {
        std::cerr << "Error: cannot open file: " << filename << "\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string source = buffer.str();

    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenizeAll();
    if (tokens.empty() || tokens.back().type != TokenType::END_OF_FILE) {
        tokens.emplace_back(TokenType::END_OF_FILE, "", 0, 0);
    }

    Parser parser(tokens);
    ASTNodePtr ast;
    try {
        ast = parser.parseProgram();
    } catch (const ParseException&) {
    }

    if (parser.hasErrors()) {
        const auto& errs = parser.getErrors();
        std::cerr << "=== PARSE ERRORS (" << errs.size() << ") ===\n";
        for (std::size_t i = 0; i < errs.size(); ++i) {
            const auto& e = errs[i];
            std::cerr << "[" << (i + 1) << "] "
                      << "Line " << e.line << ", Col " << e.column
                      << " : " << e.message << "\n";
        }
        return 1;
    }

    Sema sema(ast);
    bool ok = sema.analyze();

    if (!ok || sema.hasErrors()) {
        std::cerr << "=== SEMANTIC ERRORS ===\n";
        return 2;
    }

    std::cout << "=== SEMA OK ===\n";
    return 0;
}
