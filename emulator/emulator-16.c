#include <cosmo.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static bool step = false;
static bool debug = true;
static uint8_t windowSize = 6;

typedef struct {
  bool    halted;
  
  bool    pcModified;
  uint8_t oldPC, pc;
  
  bool    readReg1, readReg2, wroteReg;
  uint8_t lastReadReg1, lastReadReg2, lastWriteReg;
  int16_t registers[16];
  
  bool    readMem, wroteMem;
  uint8_t lastReadAddr, lastWriteAddr;
  int16_t memory[256];
} cpu;

void handleStdin(cpu *cpuState, uint8_t nextReadAddr) {
  if (nextReadAddr != 0xFF) return;
  printf("input: \n");
  while (scanf("%4" SCNx16, &cpuState->memory[0xFF]) != 1) {
   printf("input: \n");
   scanf("%*s");
  }
  
  putchar('\n');
}

void handleStdout(cpu *cpuState, uint8_t lastWriteAddr) {
  if (lastWriteAddr != 0xFF) return;
  printf("output: %04" PRIX16 "(%" PRId16 ")\n\n", cpuState->memory[0xFF], cpuState->memory[0xFF]);
}


void writePC(cpu *cpuState, uint8_t newPC, bool cycleIncrement) {
  if (cpuState->halted) return;
  // Skip increment if the instruction changed PC already
  if (cpuState->pcModified && cycleIncrement) return;
  
  cpuState->oldPC = cpuState->pc;
  cpuState->pc = newPC;
  cpuState->pcModified |= !cycleIncrement;
}

int16_t readRegister(cpu *cpuState, uint8_t reg, bool secondRead) {
  if (reg >= 16) {
    fprintf(stderr, "Error: Invalid cpu register accessed");
    exit(1);
  }
  
  if (secondRead) {
    cpuState->readReg2 = true;
    cpuState->lastReadReg2 = reg;
  } else {
    cpuState->readReg1 = true;
    cpuState->lastReadReg1 = reg;
  } 
  
  return cpuState->registers[reg];
}

void writeRegister(cpu *cpuState, uint8_t reg, int16_t value) {
  if (reg >= 16) {
    fprintf(stderr, "Error: Invalid cpu register accessed");
    exit(1);
  }
  
  if (reg != 0) {
    cpuState->wroteReg = true;
    cpuState->lastWriteReg = reg;
  }
  cpuState->registers[reg] = value;
}

int16_t readMemory(cpu *cpuState, uint8_t addr) {
  handleStdin(cpuState, addr);
  cpuState->readMem = true;
  cpuState->lastReadAddr = addr;
  return cpuState->memory[addr];
}

void writeMemory(cpu *cpuState, uint8_t addr, int16_t value) {
  cpuState->wroteMem = addr != 0xFF;
  cpuState->lastWriteAddr = addr;
  cpuState->memory[addr] = value;
  handleStdout(cpuState, addr);
}


const char *redStr = "\033[31m";  // PC, register memory or modified
const char *yellowStr = "\033[33m";  // PC incremented and current memory address
const char *blueStr1 = "\033[34m"; // First read source register or memory addr
const char *blueStr2 = "\033[94m"; // Second read source register
const char *whiteStr = "\033[97m"; // Default

const char *getPCColour(cpu *cpuState) {
  
  if (cpuState->halted) {
    return whiteStr;
  }
  
  if (cpuState->pcModified) {
    return redStr;
  }
  
  if (cpuState->oldPC != cpuState->pc) {
    return yellowStr;
  }
  
  return whiteStr;
}

const char *getRegColour(cpu *cpuState, uint8_t reg) {
  if ((cpuState->wroteReg && cpuState->lastWriteReg == reg)) {
    return redStr;
  }
  
  if ((cpuState->readReg1 && cpuState->lastReadReg1 == reg)) {
    return blueStr1;
  }
  
  if ((cpuState->readReg2 && cpuState->lastReadReg2 == reg)) {
    return blueStr2;
  }
  
  return whiteStr;
}

const char *getMemColour(cpu *cpuState, uint8_t addr) {
  if (addr == cpuState->pc) {
    return yellowStr;
  }
  
  if (cpuState->readMem && addr == cpuState->lastReadAddr) {
    return blueStr1;
  }
  
  if (cpuState->wroteMem && addr == cpuState->lastWriteAddr) {
    return redStr;
  }
  
  return whiteStr;
}


void printMemRange(cpu *cpuState, uint8_t addr) {
  uint8_t startAddr = addr - windowSize;
  uint8_t endAddr = addr + windowSize;
  
  //printf("Start: 0x%02X, Addr: 0x%X, End: 0x%X\n", startAddr, addr, endAddr);
  
  if (addr < windowSize) {
    startAddr = 0;
    endAddr = 2 * windowSize;
  }
  
  if (addr > 254 - windowSize) {
    startAddr = 254 - 2 * windowSize; 
    endAddr = 254;
  }
  
  //printf("Start: 0x%02X, Addr: 0x%X, End: 0x%X\n", startAddr, addr, endAddr);
  
  printf("    %sM[%02" PRIX8 "]%s", getMemColour(cpuState, startAddr), startAddr, whiteStr);
  for (uint8_t i = startAddr + 1; i <= endAddr && i < 256; ++i) {
    printf(", %sM[%02" PRIX8 "]%s", getMemColour(cpuState, i), i, whiteStr);
  }

  printf("\n     %s%04" PRIX16 "%s", getMemColour(cpuState, startAddr), cpuState->memory[startAddr], whiteStr);
  for (uint8_t i = startAddr + 1; i <= endAddr && i < 256; ++i) {
    printf(",  %s%04" PRIX16 "%s", getMemColour(cpuState, i), cpuState->memory[i], whiteStr);
  }
  
  putchar('\n');
}

void printCpuState(cpu *cpuState) {
 if (!debug) return;
 
 if (cpuState->halted) {
    printf("%sCpu has halted%s\n", redStr, whiteStr);
    return;
  }
  
  printf("Cpu state:\n  Registers:\n    %sPC%s", getPCColour(cpuState), whiteStr);
  for (int i = 0; i < 16; ++i) {
    printf(", %sR[%X]%s", getRegColour(cpuState, i), i, whiteStr);
  }

  printf("\n    %s%02" PRIX8 "%s", getPCColour(cpuState), cpuState->pc, whiteStr);
  for (int i = 0; i < 16; ++i) {
    printf(", %s%04" PRIX16 "%s", getRegColour(cpuState, i), cpuState->registers[i], whiteStr);
  }

  puts("\n\n  Memory near PC:");
  printMemRange(cpuState, cpuState->pc);

  if (cpuState->readMem) {
    puts("\n  Memory near last read:");
    printMemRange(cpuState, cpuState->lastReadAddr);
  }

  if (cpuState->wroteMem) {
    puts("\n  Memory near last write:");
    printMemRange(cpuState, cpuState->lastWriteAddr);
  }
  
  putchar('\n');
}


void initCpuState(cpu *cpuState) {
  cpuState->halted = cpuState->pcModified = false;
  cpuState->oldPC = cpuState->pc = 0x10;
  
  cpuState->readReg1 = cpuState->readReg2 = cpuState->wroteReg = false;
  cpuState->lastReadReg1 = cpuState->lastReadReg2 = cpuState->lastWriteReg = 0;
  for (int i = 0; i < 16; ++i) {
    cpuState->registers[i] = 0;
  }

  cpuState->readMem = cpuState->wroteMem = false;
  cpuState->lastReadAddr = cpuState->lastWriteAddr = 0;
  for (int i = 0; i < 256; ++i) {
    cpuState->memory[i] = 0;
  }
}


void processLine(char *line, cpu *cpuState) {
  static bool inComment = false;
  char *rest;
  
  // Trim spaces from beginning and end of line
  while (isspace((unsigned char)*line)) line++;
  ptrdiff_t len = strlen(line);
  while(len && isspace((unsigned char)line[len - 1])) --len;
  line[len] = 0;
  
  // Handle end of multi line comments
  if (inComment && line[0] == '*' && strcmp(line + len - 2, "*/") == 0) {
    inComment = false;
    putchar('\n');
    return;
  }
  
  // Handle start and body of multi line comments
  if ((inComment && line[0] == '*') || strncmp(line, "/*", 2) == 0) {
    inComment = true;
    putchar('\n');
    return;
  }
  
  if (inComment) {
    puts("\nInvalid line in multi line comment");
    exit(1);
  }
  
  // Handle whitespace only lines, program and function declarations, and single line comments
  if (!strlen(line) || strncmp(line, "program", 7) == 0 || 
      strncmp(line, "function", 8) == 0 || strncmp(line, "//", 2) == 0) {
    putchar('\n');
    return;
  }

  unsigned long address = strtoul(line, &rest, 16);
  if (errno == ERANGE || address > 0xFF || (line + 2) > rest) {
    puts("\nInvalid memory address");
    exit(1);
  }

  line = rest + 1;
  unsigned long data = strtoul(line, &rest, 16);
  if (errno == ERANGE || data >= 0xFFFF || (line + 4) > rest) {
    puts("\nInvalid memory value");
    exit(1);
  }

  printf(", %02lX -> %04X\n", address, data);
  cpuState->memory[address] = data;
}

void runCpu(cpu *cpuState) {
  while (!cpuState->halted) {
    uint16_t inst = cpuState->memory[cpuState->pc];
    
    switch(inst >> 12) {
      case 0x0:
        cpuState->halted = true;
        break;
      case 0x1:
        int16_t r1 = readRegister(cpuState, (inst >> 4) & 0xF, false);
        int16_t r2 = readRegister(cpuState, inst & 0xF, true);
        writeRegister(cpuState, (inst >> 8) & 0xF, r1 + r2);
        break;
      case 0x2:
        r1 = readRegister(cpuState, (inst >> 4) & 0xF, false);
        r2 = readRegister(cpuState, inst & 0xF, true);
        writeRegister(cpuState, (inst >> 8) & 0xF, r1 - r2);
        break;
      case 0x3:
        r1 = readRegister(cpuState, (inst >> 4) & 0xF, false);
        r2 = readRegister(cpuState, inst & 0xF, true);
        writeRegister(cpuState, (inst >> 8) & 0xF, r1 & r2);
        break;
      case 0x4:
        r1 = readRegister(cpuState, (inst >> 4) & 0xF, false);
        r2 = readRegister(cpuState, inst & 0xF, true);
        writeRegister(cpuState, (inst >> 8) & 0xF, r1 ^ r2);
        break;
      case 0x5:
        r1 = readRegister(cpuState, (inst >> 4) & 0xF, false);
        r2 = readRegister(cpuState, inst & 0xF, true);
        writeRegister(cpuState, (inst >> 8) & 0xF, r1 << r2);
        break;
      case 0x6:
        r1 = readRegister(cpuState, (inst >> 4) & 0xF, false);
        r2 = readRegister(cpuState, inst & 0xF, true);
        writeRegister(cpuState, (inst >> 8) & 0xF, r1 >> r2);
        break;
      case 0x7:
        writeRegister(cpuState, (inst >> 8) & 0xF, inst & 0xFF);
        break;
      case 0x8:
        int16_t mem = readMemory(cpuState, inst & 0xFF);
        writeRegister(cpuState, (inst >> 8) & 0xF, mem);
        break;
      case 0x9:
        r1 = readRegister(cpuState, (inst >> 8) & 0xF, false);
        writeMemory(cpuState, inst & 0xFF, r1);
        break;
      case 0xA:
        r1 = readRegister(cpuState, inst & 0xF, false);
        mem = readMemory(cpuState, r1);
        writeRegister(cpuState, (inst >> 8) & 0xF, mem);
        break;
      case 0xB:
        r1 = readRegister(cpuState, (inst >> 8) & 0xF, false);
        r2 = readRegister(cpuState, inst & 0xF, true);
        writeMemory(cpuState, r2, r1);
        break;
      case 0xC:
        r1 = readRegister(cpuState, (inst >> 8) & 0xF, false);
        if (r1 == 0) {
          writePC(cpuState, (inst & 0xFF), false);
        }
        break;
      case 0xD:
        r1 = readRegister(cpuState, (inst >> 8) & 0xF, false);
        if (r1 > 0) {
          writePC(cpuState, inst & 0xFF, false);
        }
        break;
      case 0xE:
        r1 = readRegister(cpuState, (inst >> 8) & 0xF, false);
        writePC(cpuState, r1, false);
        break;
      case 0xF:
        writeRegister(cpuState, (inst >> 8) & 0xF, cpuState->pc + 1);
        writePC(cpuState, inst & 0xFF, false);
        break;
      default:
        puts("Invalid opcode");
        exit(1);
    }

    writePC(cpuState, cpuState->pc + 1, true);
    writeRegister(cpuState, 0, 0);
    printCpuState(cpuState);

    cpuState->pcModified = false;
    cpuState->readReg1 = cpuState->readReg2 = cpuState->wroteReg = false;
    cpuState->readMem = cpuState->wroteMem = false;
    
    if (step) {
      getchar();
    }
  }
}

void processFile(char *filePath) {
  FILE *fp;
  char *line = NULL;
  ptrdiff_t lineLen;
  cpu state;

  if (!(fp = fopen(filePath, "r"))) {
    puts("Path is invalid");
    exit(1);
  }

  initCpuState(&state);

  while ((line = chomp(fgetln(fp, NULL)))) {
    printf("input: \"%s\"", line);
    processLine(line, &state);
  }


  if (ferror(fp) || !feof(fp)) {
    puts("File reading error");
    exit(1);
  }

  putchar('\n');
  printCpuState(&state);

  putchar('\n');
  runCpu(&state);
  //state.readMem = true;
  //state.lastReadAddr = 0x01;
  //printMemRange(&state, 0x01);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    puts("No path given");
    return 1;
  }

  processFile(argv[1]);

  return 0;
}
