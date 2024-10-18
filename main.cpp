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

static std::unordered_map<std::string, struct Instruction> Instructions{{
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


enum DirectiveName {
  ORG,
  WORD
};

struct Directive {
  enum DirectiveName name;
  uint_fast8_t argumentLength;
};

static std::unordered_map<std::string, struct Directive> Directives({
  {"ORG", {DirectiveName::ORG, 2}},
  {"WORD", {DirectiveName::WORD, 4}}
});


static std::size_t MemoryLocation = 0x10;
static const std::size_t MemorySize = UINT8_MAX + 1;
static std::array<bool, MemorySize> MemoryUsed = {false};
static std::array<std::string, MemorySize> Memory;

static std::unordered_map<std::string, std::size_t> Labels;


static void SetMemoryLocation(std::string word, Token *token) {
  MemoryUsed[MemoryLocation] = true;
  Memory[MemoryLocation] = word;
  ++MemoryLocation;
  if (MemoryUsed[MemoryLocation]) {
    std::cerr << "Memory address hit an existing adddress which is not allowed" << std::endl;
    std::cerr << token->toString() << std::endl;
    throw std::exception();
  }
}


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
  std::size_t argumentCount = instructionCtx->argument().size();
  if (instruction.registerCount != argumentCount) {
    std::cerr << "Instruction " << mnemonic << " has incorrect argument count " << argumentCount
      << ", expected " << instruction.registerCount << std::endl;
    std::cerr << mnemonicToken->toString() << std::endl;
    throw std::exception();
  }

#ifndef NDEBUG
  std::cout << "Found instruction " << mnemonic << " with " << argumentCount << " arguments" << std::endl;
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

        argumentNode = argumentCtx->HALFWORD();
        if (argumentNode) {
          std::string address = argumentNode->getSymbol()->getText();
          if (address.length() != 2) {
            std::cerr << "Instruction " << mnemonic << " has incorrect argument at position "
              << argumentIdx << ", expected 2 digit memory address" << std::endl;
            std::cerr << mnemonicToken->toString() << std::endl;
            throw std::exception();
          }
        }

        if (argumentNode) {
#ifndef NDEBUG
          std::cout << "   Address: ";
#endif
        } else{
          argumentNode = argumentCtx->LABEL();
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
      std::cout << argumentNode->getSymbol()->getText() << std::endl;
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
  Token *directiveToken = directiveCtx->DIRECTIVE()->getSymbol();
  Token *argumentToken = directiveCtx->WORD()->getSymbol();
  std::string directive = directiveToken->getText();
  std::string argument = argumentToken->getText();

  directive.erase(0, 1);

#ifndef NDEBUG
  std::cout << "Found directive " << directive << " with argument "
    << argument << std::endl;
#endif

  if (Directives.count(directive) != 1) {
    std::cerr << "directive " << directive << " does not exist" << std::endl;
    std::cerr << directiveToken->toString() << std::endl;
    throw std::exception();
  }

  struct Directive directiveInfo = Directives[directive];
  if (argument.length() != directiveInfo.argumentLength) {
    std::cerr << "directive " << directive << " expects an argument of length "
      << +directiveInfo.argumentLength << " but one of length " << argument.length()
      << " given" << std::endl;
    std::cerr << argumentToken->toString() << std::endl;
    throw std::exception();
  }

  // TODO: Ensure argument is a hex int

  switch (directiveInfo.name) {
    case DirectiveName::ORG: {
      std::size_t newMemoryLocation = std::stoi(argument, nullptr, 16);
      if (MemoryUsed[newMemoryLocation]) {
        std::cerr << "ORG directive with an existing adddress is not allowed" << std::endl;
        std::cerr << argumentToken->toString() << std::endl;
        throw std::exception();
      }

      MemoryLocation = newMemoryLocation;
      }
      break;
    case DirectiveName::WORD:
      ++MemoryLocation;
      break;
  }

}

void XToyPreListener::exitLabel(asmxtoyParser::LabelContext *labelCtx) {
  Token *labelToken = labelCtx->LABEL()->getSymbol();
  std::string label = labelToken->getText();

#ifndef NDEBUG
  std::cout << std::setfill('0') << std::setw(2) << std::uppercase << std::hex
    << "Found label " << label << " with address " << MemoryLocation << std::endl;
#endif

  if (Labels.count(label) == 1) {
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
        std::string registerStr = argumentCtx->REGISTER()->getSymbol()->getText();
        word << std::uppercase << std::hex << registerStr.back();
        ++argumentIdx;
        break;
      }
      case Address:
        tree::TerminalNode *addressNode = argumentCtx->HALFWORD();
        if (addressNode) {
          word << addressNode->getSymbol()->getText();
        } else {
          std::string labelStr = argumentCtx->LABEL()->getSymbol()->getText();

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

  SetMemoryLocation(word.str(), mnemonicToken);
}

void XToyOutputListener::exitDirective(asmxtoyParser::DirectiveContext *directiveCtx) {
  std::string directive = directiveCtx->DIRECTIVE()->getSymbol()->getText();
  Token *argumentToken = directiveCtx->WORD()->getSymbol();
  std::string argument = argumentToken->getText();

  directive.erase(0, 1);
  struct Directive directiveInfo = Directives[directive];

  switch (directiveInfo.name) {
    case DirectiveName::ORG:
      MemoryLocation = std::stoi(argument, nullptr, 16);
      break;
    case DirectiveName::WORD:
      SetMemoryLocation(argument, argumentToken);
      break;
  }
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
