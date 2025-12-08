#include <iostream>
#include <fstream>
#include <string>

#include "antlr4-runtime.h"
#include "SysYLexer.h"
#include "SysYParser.h"
#include "ir/Module.h"
#include "ir/IRBuilder.h"
#include "compiler/SysYIRGenerator.h"

using namespace antlr4;

int main(int argc, const char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: ./compiler <input-file> <output-file>"
              << std::endl;
    return 1;
  }

  std::string inputPath = argv[1];
  std::string outputPath = argv[2];

  std::ifstream inputFile(inputPath);
  if (!inputFile.is_open()) {
    std::cerr << "Error: Could not open input file " << inputPath << std::endl;
    return 1;
  }

  ANTLRInputStream input(inputFile);
  SysYLexer lexer(&input);
  CommonTokenStream tokens(&lexer);
  SysYParser parser(&tokens);

  // Add error listener to report syntax errors
  parser.removeErrorListeners();
  parser.addErrorListener(&ConsoleErrorListener::INSTANCE);

  SysYParser::CompUnitContext *tree = parser.compUnit();

  if (parser.getNumberOfSyntaxErrors() > 0) {
    std::cerr << "Syntax error(s) found. Compilation aborted." << std::endl;
    return 1;
  }

  ir::Module module;
  ir::IRBuilder builder;
  compiler::SysYIRGenerator generator(module, builder);

  try {
    generator.generate(tree);
  } catch (const std::exception &e) {
    std::cerr << "Error during IR generation: " << e.what() << std::endl;
    return 1;
  }

  std::ofstream outputFile(outputPath);
  if (!outputFile.is_open()) {
    std::cerr << "Error: Could not open output file " << outputPath << std::endl;
    return 1;
  }

  outputFile << module.print();
  outputFile.close();

  return 0;
}