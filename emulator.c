#include <ctype.h>    // isspace
#include <errno.h>    // error, ERANGE
#include <inttypes.h> // PRIX8, PRIX16, PRIX32, SCNx32, UINT8_MAX, uint8_t, UINT16_MAX, uint16_t, uint32_t
#include <stdbool.h>  // bool, false, true
#include <stdio.h>    // FILE, feof, ferror, fopen, getchar, printf, puts, stderr
#include <stdlib.h>   // exit, free, malloc, realloc, strtoul
#include <string.h>   // strcmp, strlen, strncmp

#define REG_COUNT   (uint8_t)16
#define MEM_SIZE_16 (uint32_t)(UINT8_MAX + 1)
#define MEM_SIZE_32 (uint32_t)(UINT16_MAX + 1)

static bool step = false;
static bool debug = true;

static uint8_t  windowSize = 6;
static uint32_t memMaxValue16 = UINT16_MAX;
// static uint32_t memMaxValue32 = UINT32_MAX;
static uint16_t stdInOutAddr16 = 0xFF;
static uint16_t stdInOutAddr32 = 0x7F; // 0xFF/2

typedef struct {
  bool     halted;
  bool     in32Bit;
  
  bool     pcModified;
  uint16_t oldPC, pc;
  
  bool     readReg1, readReg2, wroteReg;
  uint8_t  lastReadReg1, lastReadReg2, lastWriteReg;
  uint32_t registers[REG_COUNT];
  
  bool     readMem, wroteMem;
  uint16_t lastReadAddr, lastWriteAddr;
  union {
    uint16_t memory16[MEM_SIZE_16];
    uint32_t memory32[MEM_SIZE_32];
  };
} cpu;

// TODO: Test with memory32
void handleStdin(cpu *cpuState, uint16_t nextReadAddr) {
  char *fmt;
  void *addr;
  if (cpuState->in32Bit) {
    if (nextReadAddr != stdInOutAddr32) {
      return;
    }
    fmt = "%8" SCNx32;
    addr = cpuState->memory32 + stdInOutAddr32;
  } else {
    if (nextReadAddr != stdInOutAddr16) {
      return;
    }
    fmt = "%4" SCNx32;
    addr = cpuState->memory16 + stdInOutAddr16;
  }

  printf("input: \n");
  while (scanf(fmt, addr) != 1) {
    printf("input: \n");
    scanf("%*s");
  }
  
  putchar('\n');
}

// TODO: Test with memory32
void handleStdout(cpu *cpuState, uint16_t lastWriteAddr) {
  // TODO: Fix mem mess, requires flipping 16 bit bytes based on host endianness(big = nothing, small = flip)
  // This is different from common endian functions which use native byte sizes(normally 8 bits)
  char *fmt;
  uint32_t mem;
  if (cpuState->in32Bit) {
    if (lastWriteAddr != stdInOutAddr32) {
      return;
    }
    fmt = "%08" PRIX32;
    mem = cpuState->memory32[stdInOutAddr32];
  } else {
    if (lastWriteAddr != stdInOutAddr16) {
      return;
    }
    fmt = "%04" PRIX32;
    mem = cpuState->memory16[stdInOutAddr16];
  }

  printf("output: ");
  printf(fmt, mem);
  printf("(%" PRId16 ")\n\n", mem);
}


void writePC(cpu *cpuState, uint16_t newPC, bool cycleIncrement) {
  if (cpuState->halted) return;
  // Skip increment if the instruction changed PC already
  if (cpuState->pcModified && cycleIncrement) return;
  
  cpuState->oldPC = cpuState->pc;
  cpuState->pc = newPC;
  cpuState->pcModified |= !cycleIncrement;
}

uint32_t readRegister(cpu *cpuState, uint8_t reg, bool secondRead) {
  if (reg >= REG_COUNT) {
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

void writeRegister(cpu *cpuState, uint8_t reg, uint32_t value) {
  if (reg >= REG_COUNT) {
    fprintf(stderr, "Error: Invalid cpu register accessed");
    exit(1);
  }
  
  if (reg != 0) {
    cpuState->wroteReg = true;
    cpuState->lastWriteReg = reg;
  }
  cpuState->registers[reg] = value;
}

uint32_t readMemory(cpu *cpuState, uint16_t addr) {
  handleStdin(cpuState, addr);
  cpuState->readMem = true;
  cpuState->lastReadAddr = addr;
  return cpuState->in32Bit ? cpuState->memory32[addr] : cpuState->memory16[addr];
}

void writeMemory(cpu *cpuState, uint16_t addr, uint32_t value) {
  cpuState->lastWriteAddr = addr;
  if (cpuState->in32Bit) {
    cpuState->wroteMem = addr != stdInOutAddr32;
    cpuState->memory32[addr] = value;
  } else {
    cpuState->wroteMem = addr != stdInOutAddr16;
    cpuState->memory16[addr] = value;
  }
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

const char *getMemColour(cpu *cpuState, uint16_t addr) {
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


void printMemRange(cpu *cpuState, uint16_t addr) {
  uint16_t startAddr = addr - windowSize;
  uint16_t endAddr = addr + windowSize;
  
  if (addr < windowSize) {
    startAddr = 0;
    endAddr = 2 * windowSize;
  }
  
  // Hide stdInOutAddr == 255, won't work for any other value 
  if (!cpuState->in32Bit && addr > 254 - windowSize) {
    startAddr = 254 - 2 * windowSize; 
    endAddr = 254;
  }
  
  char *fLabelStr1 = "     %sM[%04" PRIX16 "]%s";
  char *fLabelStr2 = ",   %sM[%04" PRIX16 "]%s";
  uint32_t memSize = MEM_SIZE_32;
  if (!cpuState->in32Bit) {
    fLabelStr1 = "    %sM[%02" PRIX8 "]%s";
    fLabelStr2 = ", %sM[%02" PRIX8 "]%s";
    memSize = MEM_SIZE_16;
  }
  
  printf(fLabelStr1, getMemColour(cpuState, startAddr), startAddr, whiteStr);
  for (uint16_t i = startAddr + 1; i <= endAddr && i < memSize; ++i) {
    printf(fLabelStr2, getMemColour(cpuState, i), i, whiteStr);
  }
  
  if (cpuState->in32Bit) {
    printf("\n    %s%08" PRIX32 "%s", getMemColour(cpuState, startAddr), cpuState->memory32[startAddr], whiteStr);
  } else {
    printf("\n     %s%04" PRIX16 "%s", getMemColour(cpuState, startAddr), cpuState->memory16[startAddr], whiteStr);
  }
  for (uint16_t i = startAddr + 1; i <= endAddr && i < memSize; ++i) {
    if (cpuState->in32Bit) {
      printf(",  %s%08" PRIX32 "%s", getMemColour(cpuState, i), cpuState->memory32[i], whiteStr);
    } else {
      printf(",  %s%04" PRIX16 "%s", getMemColour(cpuState, i), cpuState->memory16[i], whiteStr);
    }
  }
  
  putchar('\n');
}

void printCpuState(cpu *cpuState) {
  if (!debug) return;
 
  if (cpuState->halted) {
    printf("%sCpu has halted%s\n", redStr, whiteStr);
    return;
  }
  
  char *fLabelStr1 = "      %sPC%s";
  char *fLabelStr2 = ",    %sR[%02" PRIX8 "]%s";
  char *fValueStr1 = "\n    %s%04" PRIX16 "%s";
  char *fValueStr2 = ", %s%08" PRIX32 "%s";
  if (!cpuState->in32Bit) {
    fLabelStr1 = "    %sPC%s";
    fLabelStr2 = ", %sR[%" PRIX8 "]%s";
    fValueStr1 = "\n    %s%02" PRIX8 "%s";
    fValueStr2 = ", %s%04" PRIX16 "%s";
  }
  
  puts("Cpu state:\n  Registers:");
  printf(fLabelStr1, getPCColour(cpuState), whiteStr);
  for (uint8_t i = 0; i < REG_COUNT; ++i) {
    printf(fLabelStr2, getRegColour(cpuState, i), i, whiteStr);
  }

  printf(fValueStr1, getPCColour(cpuState), cpuState->pc, whiteStr);
  for (uint8_t i = 0; i < REG_COUNT; ++i) {
    printf(fValueStr2, getRegColour(cpuState, i), cpuState->registers[i], whiteStr);
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
  
  cpuState->readReg1 = cpuState->readReg2 = cpuState->wroteReg = false;
  cpuState->lastReadReg1 = cpuState->lastReadReg2 = cpuState->lastWriteReg = 0;
  for (uint8_t i = 0; i < REG_COUNT; ++i) {
    cpuState->registers[i] = 0;
  }
  
  cpuState->readMem = cpuState->wroteMem = false;
  cpuState->lastReadAddr = cpuState->lastWriteAddr = 0;
  for (uint32_t i = 0; i < MEM_SIZE_32; ++i) {
    cpuState->memory32[i] = 0;
  }
}

void initCpuState16(cpu *cpuState) {
  cpuState->in32Bit = false;
  cpuState->oldPC = cpuState->pc = 0x10;
  
  initCpuState(cpuState);
}

void endCycle(cpu *cpuState) {
  writePC(cpuState, cpuState->pc + 1, true);
  writeRegister(cpuState, 0, 0);
  printCpuState(cpuState)  ;
  
  cpuState->pcModified = false;
  cpuState->readReg1 = cpuState->readReg2 = cpuState->wroteReg = false;
  cpuState->readMem = cpuState->wroteMem = false;
  
  if (step) {
    getchar();
  }
}

void runCpu32(cpu *cpuState) {
  if (!cpuState->in32Bit) return;
  
  while (!cpuState->halted) {
    uint32_t inst = cpuState->memory32[cpuState->pc];
    
    switch(inst >> 12) {
      case 0x0:
        cpuState->in32Bit = inst == 0x0FFF;
        runCpu32(cpuState);
        cpuState->halted = !cpuState->in32Bit;
        break;
      default:
        puts("Invalid opcode");
        exit(1);
    }
    
    endCycle(cpuState);
  }
}

void runCpu16(cpu *cpuState) {
  if (cpuState->in32Bit) return;
  
  while (!cpuState->halted) {
    uint16_t inst = cpuState->memory16[cpuState->pc];
    
    switch(inst >> 12) {
      case 0x0:
        cpuState->in32Bit = inst == 0x0FFF;
        runCpu32(cpuState);
        cpuState->halted = !cpuState->in32Bit;
        break;
      case 0x1:
        uint32_t r1 = readRegister(cpuState, (inst >> 4) & 0xF, false);
        uint32_t r2 = readRegister(cpuState, inst & 0xF, true);
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
        uint32_t mem = readMemory(cpuState, inst & 0xFF);
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
    
    endCycle(cpuState);
  }
}


// From https://github.com/archiecobbs/libnbcompat/blob/4700b02/fgetln.c
char *fgetln(FILE *fp, size_t *len) {
  static char *buf = NULL;
  static size_t bufsiz = 0;
  static size_t buflen = 0;
  int c;

  if (buf == NULL) {
    bufsiz = BUFSIZ;
    if ((buf = malloc(bufsiz)) == NULL) {
      return NULL;
    }
  }

  buflen = 0;
  while ((c = fgetc(fp)) != EOF) {
    if (buflen >= bufsiz) {
      size_t nbufsiz = bufsiz + BUFSIZ;
      char *nbuf = realloc(buf, nbufsiz);

      if (nbuf == NULL) {
        int oerrno = errno;
        free(buf);
        errno = oerrno;
        buf = NULL;
        return NULL;
      }

      buf = nbuf;
      bufsiz = nbufsiz;
    }
    buf[buflen++] = c;
    if (c == '\n') {
      break;
    }
  }
  *len = buflen;
  return buflen == 0 ? NULL : buf;
}

void processLine16(char *line, cpu *cpuState) {
  static bool inComment = false;
  char *rest;
  
  // Trim spaces from beginning and end of line
  while (isspace((unsigned char)*line)) line++;
  size_t len = strlen(line);
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

  uint32_t address = strtoul(line, &rest, 16);
  if (errno == ERANGE || address >= MEM_SIZE_32 || (line + 2) > rest) {
    puts("\nInvalid memory address");
    exit(1);
  }

  line = rest + 1;
  uint32_t data = strtoul(line, &rest, 16);
  if (errno == ERANGE || (data > memMaxValue16 || (line + 4) > rest)) {
    puts("\nInvalid memory value");
    exit(1);
  }

  printf(", %02lX -> %04X\n", address, data);
  cpuState->memory16[address] = data;
}

void processFile16(cpu *cpuState, char *filePath) {
  FILE *fp;
  char *line = NULL;
  size_t lineLen;

  if (!(fp = fopen(filePath, "r"))) {
    puts("Path is invalid");
    exit(1);
  }

  initCpuState16(cpuState);

  while ((line = fgetln(fp, &lineLen))) {
    line[lineLen - 1] = '\0';
    printf("input: \"%s\"", line);
    processLine16(line, cpuState);
  }


  if (ferror(fp) || !feof(fp)) {
    puts("File reading error");
    exit(1);
  }

  putchar('\n');
}

int main(int argc, char **argv) {
  cpu cpuState;

  if (argc != 2) {
    puts("No path given");
    return 1;
  }

  processFile16(&cpuState, argv[1]);

  printCpuState(&cpuState);
  putchar('\n');

  runCpu16(&cpuState);

  return 0;
}
