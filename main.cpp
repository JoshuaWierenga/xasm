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

static std::array<OperandType, 3> OperandFormatRRR = {Register, Register, Register};
static std::array<OperandType, 3> OperandFormatR_R = {Register, Zero, Register};
static std::array<OperandType, 3> OperandFormatRA  = {Register, Address,  End};

// TODO: Remove registerCount and just use non End operands?
struct Instruction {
  std::size_t registerCount;

  uint_fast8_t opcode;
  std::array<OperandType, 3> operandTypes;
};

static std::map<std::string, struct Instruction> Instructions{{
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

static std::size_t MemoryLocation = 0x10;
static const std::size_t MemorySize = UINT8_MAX + 1;
static std::array<bool, MemorySize> MemoryUsed = {false};
static std::array<std::string, MemorySize> Memory;

static std::map<std::string, std::size_t> Labels;


// Error checking and label finding
class XToyPreListener : public asmxtoyBaseListener {
public:
  void exitInstruction(asmxtoyParser::InstructionContext *) override;
  void exitDirective(asmxtoyParser::DirectiveContext *) override;
  void exitLabel(asmxtoyParser::LabelContext *) override;
};

class XToyOutputListener : public asmxtoyBaseListener {
public:
  void exitInstruction(asmxtoyParser::InstructionContext *) override;
  void exitDirective(asmxtoyParser::DirectiveContext *) override;
};


void XToyPreListener::exitInstruction(asmxtoyParser::InstructionContext *instructionCtx) {
  Token *mnemonicToken = instructionCtx->MNEMONIC()->getSymbol();
  std::string mnemonic = mnemonicToken->getText();

  auto instructionIter = Instructions.find(mnemonic);
  if (instructionIter == Instructions.end()) {
    std::cerr << "Found invalid instruction " << mnemonic << std::endl;
    std::cerr << mnemonicToken->toString() << std::endl;
    throw std::exception();
  }

  struct Instruction instruction = instructionIter->second;
  std:size_t argumentCount = instructionCtx->argument().size();
  if (instruction.registerCount != argumentCount) {
    std::cerr << "Instruction " << mnemonic << " has incorrect argument count " << argumentCount
      << ", expected " << instruction.registerCount << std::endl;
    std::cerr << mnemonicToken->toString() << std::endl;
    throw std::exception();
  }

#ifndef NDEBUG
  std::cout << "Found instruction " << mnemonic << " with " << argumentCount << " registers" << std::endl;
#endif

  std::size_t argumentIdx = 0;
  for (enum OperandType operandType : instruction.operandTypes) {
    asmxtoyParser::ArgumentContext *const &argumentCtx = instructionCtx->argument(argumentIdx);
    tree::TerminalNode *argumentNode = nullptr;

    switch (operandType) {
      case End:
      case Zero:
        break;
      case Register: {
        // TODO: Report error
        if (!argumentCtx) {
          throw std::exception();
        }

        argumentNode = argumentCtx->REGISTER();
        if (!argumentNode) {
          std::cerr << "Instruction " << mnemonic << " has incorrect argument at position "
            << argumentIdx << ", expected register" << std::endl;
          std::cerr << mnemonicToken->toString() << std::endl;
          throw std::exception();
        }

#ifndef NDEBUG
        std::cout << "  Register: ";
#endif
        break;
      }
      case Address:
        // TODO: Report error
        if (!argumentCtx) {
          throw std::exception();
        }

        argumentNode = argumentCtx->ADDRESS();
        if (argumentNode) {
#ifndef NDEBUG
          std::cout << "   Address: ";
#endif
        } else{
          argumentNode = argumentCtx->LABEL_USE();
#ifndef NDEBUG
          std::cout << "     Label: ";
#endif
        }

        if (!argumentNode) {
          std::cerr << "Instruction " << mnemonic << " has incorrect argument at position "
            << argumentIdx << ", expected memory address or label" << std::endl;
          std::cerr << mnemonicToken->toString() << std::endl;
          throw std::exception();
        }
        break;
    }

    if (argumentNode) {
#ifndef NDEBUG
      std::cout << argumentNode->getText() << std::endl;
#endif
      ++argumentIdx;
    }
  }

  ++MemoryLocation;
  if (MemoryLocation >= MemorySize) {
    std::cerr << "Memory address has exceeded max size" << std::endl;
    std::cerr << mnemonicToken->toString() << std::endl;
    throw std::exception();
  }
}

void XToyPreListener::exitDirective(asmxtoyParser::DirectiveContext *directiveCtx) {
  tree::TerminalNode *addressNode = directiveCtx->ADDRESS();

  std::string address = addressNode->toString();
#ifndef NDEBUG
  std::cout << "Found directive ORG with address " << address << std::endl;
#endif

  std::size_t newMemoryLocation = std::stoi(address, nullptr, 16);
  if (MemoryUsed[newMemoryLocation]) {
    Token *addressToken = addressNode->getSymbol();
    std::cerr << "ORG directive with an existing adddress is not allowed" << std::endl;
    std::cerr << addressToken->toString() << std::endl;
    throw std::exception();
  }

  MemoryLocation = newMemoryLocation;
}

void XToyPreListener::exitLabel(asmxtoyParser::LabelContext *labelCtx) {
  tree::TerminalNode *labelNode = labelCtx->LABEL_DEF();

  std::string label = labelNode->toString();
  label.pop_back();

#ifndef NDEBUG
  std::cout << std::setfill('0') << std::setw(2) << std::uppercase << std::hex
    << "Found label " << label << " with address " << MemoryLocation << std::endl;
#endif

  if (Labels.count(label) == 1) {
    Token *labelToken = labelNode->getSymbol();
    std::cerr << "Cannot redefine label" << std::endl;
    std::cerr << labelToken->toString() << std::endl;
    throw std::exception();
  }

  Labels[label] = MemoryLocation;
}


void XToyOutputListener::exitInstruction(asmxtoyParser::InstructionContext *instructionCtx) {
  Token *mnemonicToken = instructionCtx->MNEMONIC()->getSymbol();
  std::string mnemonic = mnemonicToken->getText();
  struct Instruction instruction = Instructions.find(mnemonic)->second;

  std::stringstream word;
  word << std::uppercase << std::hex << +instruction.opcode;

  std::size_t argumentIdx = 0;
  for (enum OperandType operandType : instruction.operandTypes) {
    asmxtoyParser::ArgumentContext *const &argumentCtx = instructionCtx->argument(argumentIdx);

    switch (operandType) {
      case End:
        break;
      case Zero:
        word << "0";
        break;
      case Register: {
        std::string registerStr = argumentCtx->REGISTER()->getText();
        word << std::uppercase << std::hex << registerStr.back();
        ++argumentIdx;
        break;
      }
      case Address:
        tree::TerminalNode *addressNode = argumentCtx->ADDRESS();
        if (addressNode) {
          word << addressNode->getText();
        } else {
          std::string labelStr = argumentCtx->LABEL_USE()->getText();
          labelStr.erase(0, 1);

          if (Labels.count(labelStr) == 0) {
            std::cerr << "Cannot reference undefined label" << std::endl;
            std::cerr << mnemonicToken->toString() << std::endl;
            throw std::exception();
          }

          word << Labels[labelStr];
        }

        ++argumentIdx;
        break;
    }
  }

  MemoryUsed[MemoryLocation] = true;
  Memory[MemoryLocation] = word.str();
  ++MemoryLocation;
  if (MemoryUsed[MemoryLocation]) {
    std::cerr << "Memory address hit an existing adddress which is not allowed" << std::endl;
    std::cerr << mnemonicToken->toString() << std::endl;
    throw std::exception();
  }
}

void XToyOutputListener::exitDirective(asmxtoyParser::DirectiveContext *directiveCtx) {
  std::string address = directiveCtx->ADDRESS()->toString();
  MemoryLocation = std::stoi(address, nullptr, 16);;
}


int main(void) {
  std::ifstream code("test.xasm");
  ANTLRInputStream input(code);
  asmxtoyLexer lexer(&input);
  CommonTokenStream tokens(&lexer);
  asmxtoyParser parser(&tokens);

  tree::ParseTree* tree = parser.file();

  XToyPreListener preListener;
  XToyOutputListener outputListener;
  try {
    tree::ParseTreeWalker::DEFAULT.walk(&preListener, tree);
    MemoryLocation = 0x10;
    tree::ParseTreeWalker::DEFAULT.walk(&outputListener, tree);
  } catch (const std::exception &) {
    return EXIT_FAILURE;
  }

  std::cout << std::endl << "Output:" << std::endl;
  for (std::size_t i = 0; i < MemorySize; ++i) {
    if (MemoryUsed[i]) {
      std::cout << std::setfill('0') << std::setw(2) << std::uppercase << std::hex
        << i << ": " << Memory[i] << std::endl;
    }
  }

  return EXIT_SUCCESS;
}
