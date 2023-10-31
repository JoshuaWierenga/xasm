#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"

typedef enum {
  op_halt = 0x0, // halt
  // ARITHMETIC and LOGICAL operations
  op_addition = 0x1, // add             R[d] <- R[s] +  R[t]
  op_subtract = 0x2, // subtract        R[d] <- R[s] -  R[t]
  op_bitwiseand = 0x3, // and             R[d] <- R[s] &  R[t]
  op_bitwisexor = 0x4, // xor             R[d] <- R[s] ^  R[t]
  op_shiftleft = 0x5, // shift left      R[d] <- R[s] << R[t]
  op_shiftright = 0x6, // shift right     R[d] <- R[s] >> R[t]
  // TRANSFER between registers and memory
  op_loadimm = 0x7, // load address    R[d] <- addr
  op_load = 0x8, // load            R[d] <- M[addr]
  op_store = 0x9, // store           M[addr] <- R[d]
  op_loadindr = 0xA, // load indirect   R[d] <- M[R[t]]
  op_storeindr = 0xB, // store indirect  M[R[t]] <- R[d]
  // CONTROL
  op_branchzero = 0xC, // branch zero     if (R[d] == 0) PC <- addr
  op_branchpos = 0xD, // branch positive if (R[d] >  0) PC <- addr
  op_jump = 0xE, // jump register   PC <- R[d]
  op_call = 0xF  // jump and link   R[d] <- PC; PC <- addr
} op_code;

typedef struct {
  b32 unknown;
  // Only used if !unknown
  op_code op;
} op_info;

const c8 *op_full_names[] = {
  "Halt",
  "Addition",
  "Subtraction",
  "Bitwise and",
  "Bitwise xor",
  "Shift left",
  "Shift right",
  "Load immediate",
  "Load",
  "Store",
  "Load indirect",
  "Store indirect",
  "Branch if zero",
  "Branch if positive",
  "Jump",
  "Call function"
};

#define UNKNOWN "Unknown"

op_info *getopinfo(u16 word) {
  static op_info info;
  if (word <= 0x0FFF) {
    info.op = op_halt;
    return &info;
  }

  info.op = word >> 12;
  return &info;
}

size getopdesclen(op_info *info) {
  if (info->unknown) {
    return lengthof(UNKNOWN);
  }

  return snprintf(NULL, 0, op_full_names[info->op & 0xF]);
}

c8 *getopdesc(op_info *info) {
  size opdesclen = getopdesclen(info) + 1;
  c8 *opdesc = malloc(opdesclen);

  if (info->unknown) {
    snprintf(opdesc, opdesclen, UNKNOWN);
  } else {
    snprintf(opdesc, opdesclen, op_full_names[info->op & 0xF]);
  }

  return opdesc;
}

int main(void) {
  for (i32 word; word <= UINT16_MAX; ++word) {
    op_info *info = getopinfo(word);

    c8 *infodesc = getopdesc(info);
    printf("%04"
    PRIX32
    ": %s\n", word, infodesc);
    free(infodesc);
  }

  return 0;
}
