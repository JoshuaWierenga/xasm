#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>

#include "antlr4-runtime.h"
#include "asmxtoyLexer.h"
#include "asmxtoyParser.h"
#include "asmxtoyBaseListener.h"

using namespace antlr4;


enum OperandType {
  End,
  Zero,
  Register,
  Address
};

std::array<OperandType, 3> OperandFormatRRR = {Register, Register, Register};
std::array<OperandType, 3> OperandFormatR_R = {Register, Zero, Register};
std::array<OperandType, 3> OperandFormatRA  = {Register, Address,  End};

// TODO: Remove registerCount and just use non End operands?
struct Instruction {
  std::size_t registerCount;

  uint_fast8_t opcode;
  std::array<OperandType, 3> operandTypes;
};

std::map<std::string, struct Instruction> Instructions{{
  {"hlt", {0, 0x0, {Zero, Zero, Zero}}},
  {"add", {3, 0x1, OperandFormatRRR}},
  {"sub", {3, 0x2, OperandFormatRRR}},
  {"and", {3, 0x3, OperandFormatRRR}},
  {"xor", {3, 0x4, OperandFormatRRR}},
  {"asl", {3, 0x5, OperandFormatRRR}},
  {"asr", {3, 0x6, OperandFormatRRR}},
  {"lda", {2, 0x7, OperandFormatRA}},
  {"lod", {2, 0x8, OperandFormatRA}},
  {"str", {2, 0x9, OperandFormatRA}},
  {"ldi", {2, 0xA, OperandFormatR_R}},
  {"sti", {2, 0xB, OperandFormatR_R}},
  {"brz", {2, 0xC, OperandFormatRA}},
  {"brp", {2, 0xD, OperandFormatRA}},
  {"jmp", {1, 0xE, {Register, Zero, Zero}}},
  {"jsr", {2, 0xF, OperandFormatRA}}
}};


static uint8_t memoryLocation = 0x10;


class XToyListener : public asmxtoyBaseListener {
public:
  void exitInstruction(asmxtoyParser::InstructionContext *) override;
  void exitDirective(asmxtoyParser::DirectiveContext *) override;
};

void XToyListener::exitInstruction(asmxtoyParser::InstructionContext *instructionCtx) {
  tree::TerminalNode *mnemonicNode = instructionCtx->MNEMONIC();
  Token *mnemonicToken = mnemonicNode->getSymbol();
  std::string mnemonic = mnemonicToken->getText();

  auto instructionIter = Instructions.find(mnemonic);
  if (instructionIter == Instructions.end()) {
    std::cerr << "Found invalid instruction " << mnemonic << std::endl;
    std::cerr << mnemonicToken->toString() << std::endl;
    throw std::exception();
  }

  struct Instruction instruction = instructionIter->second;
  std::vector<asmxtoyParser::ArgumentContext *> arguments = instructionCtx->argument();
  std:size_t argumentCount = arguments.size();
  if (instruction.registerCount != argumentCount) {
    std::cerr << "Instruction " << mnemonic << " has incorrect argument count " << argumentCount
      << ", expected " << instruction.registerCount << std::endl;
    std::cerr << mnemonicToken->toString() << std::endl;
    throw std::exception();
  }

  // std::cout << "Found instruction " << mnemonic << " with " << argumentCount << " registers" << std::endl;

  std::cout << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << +memoryLocation << ": ";
  ++memoryLocation;

  std::cout << std::uppercase << std::hex << +instruction.opcode;

  size_t argumentIdx = 0;
  for (enum OperandType operandType : instruction.operandTypes) {
    asmxtoyParser::ArgumentContext *const &argumentCtx = instructionCtx->argument(argumentIdx);
    tree::TerminalNode *argumentNode;
    Token *argumentToken = nullptr;

    switch (operandType) {
      case End:
        break;
      case Zero:
        std::cout << "0";
        break;
      case Register: {
        // TODO: Report error
        if (!argumentCtx) {
          throw std::exception();
        }

        argumentNode = argumentCtx->REGISTER();
        if (!argumentNode) {
          std::cerr << "Instruction " << mnemonic << " has incorrect argument at position"
            << argumentIdx << ", expected register" << std::endl;
          std::cerr << mnemonicToken->toString() << std::endl;
          throw std::exception();
        }

        // std::cout << "  Register: ";

        argumentToken = argumentNode->getSymbol();
        std::string registerStr = argumentToken->getText();

        std::cout << std::uppercase << std::hex << registerStr.back();
        break;
      }
      case Address:
        // TODO: Report error
        if (!argumentCtx) {
          throw std::exception();
        }

        argumentNode = argumentCtx->ADDRESS();
        if (!argumentNode) {
          std::cerr << "Instruction " << mnemonic << " has incorrect argument at position"
            << argumentIdx << ", expected memory address" << std::endl;
          std::cerr << mnemonicToken->toString() << std::endl;
          throw std::exception();
        }

        // std::cout << "   Address: ";

        argumentToken = argumentNode->getSymbol();

        std::cout << argumentNode->getText();
        break;
    }

    if (argumentToken) {
      // std::cout << argumentToken->getText() << std::endl;
      ++argumentIdx;
    }
  }

  std::cout << std::endl;
}

void XToyListener::exitDirective(asmxtoyParser::DirectiveContext *directiveCtx) {
  tree::TerminalNode *addressNode = directiveCtx->ADDRESS();
  if (!addressNode) {
    std::cerr << "ORG directive missing memory address" << std::endl;
    throw std::exception();
  }

  std::string address = addressNode->toString();
  // std::cout << "Found direction ORG with address " << address << std::endl;

  // TODO: Allow out of order memory locations?
  // Can do by putting instructions into an array first and then write out in order
  // Need to also track written values to split unwritten from hlt written explicitly
  // Can restore debug message then
  uint_fast8_t newMemoryLocation = std::stoi(address, nullptr, 16);
  if (newMemoryLocation <= memoryLocation) {
    Token *addressToken = addressNode->getSymbol();
    std::cerr << "ORG direction will smaller memory address not allowed, current address is "
      << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << +memoryLocation <<
      ", requested address is " << address << std::endl;
    std::cerr << addressToken->toString() << std::endl;
    throw std::exception();
  }

  memoryLocation = newMemoryLocation;
}


int main(void) {
  std::ifstream code("test.xasm");
  ANTLRInputStream input(code);
  asmxtoyLexer lexer(&input);
  CommonTokenStream tokens(&lexer);
  asmxtoyParser parser(&tokens);

  tree::ParseTree* tree = parser.file();

  std::cout << "Output:" << std::endl;
  XToyListener listener;
  try {
    tree::ParseTreeWalker::DEFAULT.walk(&listener, tree);
  } catch (const std::exception &) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
