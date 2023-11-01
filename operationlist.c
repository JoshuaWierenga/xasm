#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"

typedef enum {
  op_halt = 0x0, // halt
  // ARITHMETIC and LOGICAL operations
  op_addition = 0x1,   // add             R[d] <- R[s] +  R[t]
  op_subtract = 0x2,   // subtract        R[d] <- R[s] -  R[t]
  op_bitwiseand = 0x3, // and             R[d] <- R[s] &  R[t]
  op_bitwisexor = 0x4, // xor             R[d] <- R[s] ^  R[t]
  op_shiftleft = 0x5,  // shift left      R[d] <- R[s] << R[t]
  op_shiftright = 0x6, // shift right     R[d] <- R[s] >> R[t]
  // TRANSFER between registers and memory
  op_loadimm = 0x7,   // load address     R[d] <- addr
  op_load = 0x8,      // load             R[d] <- M[addr]
  op_store = 0x9,     // store            M[addr] <- R[d]
  op_loadindr = 0xA,  // load indirect    R[d] <- M[R[t]]
  op_storeindr = 0xB, // store indirect   M[R[t]] <- R[d]
  // CONTROL
  op_branchzero = 0xC, // branch zero     if (R[d] == 0) PC <- addr
  op_branchpos = 0xD,  // branch positive if (R[d] >  0) PC <- addr
  op_jump = 0xE,       // jump register   PC <- R[d]
  op_call = 0xF,       // jump and link   R[d] <- PC; PC <- addr
} op_code;

typedef enum {
  format____, // Halt(0)
  format_R__, // Jump(E)
  format_R_R, // Load indirect and Store indirect(A, B)
  format_RRR, // Addition through Shift right(1-6)
  format_RAA, // Load immediate through Store, Branch Zero, Branch Positive and Call(7-9, C, D, F)
} op_code_format;

typedef struct {
  b32 unknown;
  // Only used if !unknown
  op_code op;
  // This is just the raw nibbles, with format_RAA the two src nibbles are actually one complete byte
  byte dstOperand;
  byte src1Operand;
  byte src2Operand;
} op_info;

const op_code_format op_code_format_mapping[] = {
  format____, // op_halt
  format_RRR, // op_addition
  format_RRR, // op_subtract
  format_RRR, // op_bitwiseand
  format_RRR, // op_bitwisexor
  format_RRR, // op_shiftleft
  format_RRR, // op_shiftright
  format_RAA, // op_loadimm
  format_RAA, // op_load
  format_RAA, // op_store
  format_R_R, // op_loadindr
  format_R_R, // op_storeindr
  format_RAA, // op_branchzero
  format_RAA, // op_branchpos
  format_R__, // op_jump
  format_RAA, // op_call
};

const c8 *op_format_strings[] = {
  "Halt",
  "Addition, R[%" PRIX8 "] <- R[%" PRIX8 "] + R[%" PRIX8 "]",
  "Subtraction, R[%" PRIX8 "] <- R[%" PRIX8 "] - R[%" PRIX8 "]",
  "Bitwise and, R[%" PRIX8 "] <- R[%" PRIX8 "] & R[%" PRIX8 "]",
  "Bitwise xor, R[%" PRIX8 "] <- R[%" PRIX8 "] ^ R[%" PRIX8 "]",
  "Shift left, R[%" PRIX8 "] <- R[%" PRIX8 "] << R[%" PRIX8 "]",
  "Shift right, R[%" PRIX8 "] <- R[%" PRIX8 "] >> R[%" PRIX8 "]",
  "Load immediate, R[%" PRIX8 "] <- 00%" PRIX8 "%" PRIX8,
  "Load, R[%" PRIX8 "] <- M[%" PRIX8 "%" PRIX8 "]",
  "Store, M[%" PRIX8 "%" PRIX8 "] <- R[%" PRIX8 "]",
  "Load indirect, R[%" PRIX8 "] <- M[R[%" PRIX8 "]]",
  "Store indirect, M[R[%" PRIX8 "]] <- R[%" PRIX8 "]",
  "Branch if zero, if (R[%" PRIX8 "] == 0) goto %" PRIX8 "%" PRIX8,
  "Branch if positive, if (R[%" PRIX8 "] > 0) goto %" PRIX8 "%" PRIX8,
  "Jump, goto R[%" PRIX8 "]",
  "Call function, R[%" PRIX8 "] <- PC; goto %" PRIX8 "%" PRIX8,
};

#define UNKNOWN "Unknown"

op_info *getopinfo(u16 word) {
  static op_info info;
  if (word <= 0x0FFF) {
    info.op = op_halt;
    return &info;
  }

  info.op = (op_code)(word >> 12);
  info.dstOperand = (word >> 8) & 0xF;
  info.src1Operand = (word >> 4) & 0xF;
  info.src2Operand = word & 0xF;
  return &info;
}

size getopdesclen(op_info *info) {
  if (info->unknown) {
    return lengthof(UNKNOWN);
  } else if (op_code_format_mapping[info->op] == format_RRR ||
             (op_code_format_mapping[info->op] == format_RAA && info->op != op_store)) {
    return snprintf(NULL, 0, op_format_strings[info->op & 0xF], info->dstOperand, info->src1Operand, info->src2Operand);
  } else if (info->op == op_store) {
    return snprintf(NULL, 0, op_format_strings[info->op & 0xF], info->src1Operand, info->src2Operand, info->dstOperand);
  }
  return snprintf(NULL, 0, op_format_strings[info->op & 0xF]);
}

c8 *getopdesc(op_info *info) {
  size opdesclen = getopdesclen(info) + 1;
  c8 *opdesc = malloc(opdesclen);

  if (info->unknown) {
    snprintf(opdesc, opdesclen, UNKNOWN);
  } else if (info->op == op_halt || info->op == op_jump || op_code_format_mapping[info->op] == format_RRR ||
             (op_code_format_mapping[info->op] == format_RAA && info->op != op_store)) {
    snprintf(opdesc, opdesclen, op_format_strings[info->op & 0xF], info->dstOperand, info->src1Operand,
             info->src2Operand);
  } else if (info->op == op_store) {
    snprintf(opdesc, opdesclen, op_format_strings[info->op & 0xF], info->src1Operand, info->src2Operand,
             info->dstOperand);
  } else if (info->op == op_loadindr) {
    snprintf(opdesc, opdesclen, op_format_strings[info->op & 0xF], info->dstOperand, info->src2Operand);
  } else if (info->op == op_storeindr) {
    snprintf(opdesc, opdesclen, op_format_strings[info->op & 0xF], info->src2Operand, info->dstOperand);
  }
  return opdesc;
}

int main(void) {
  for (i32 word; word <= UINT16_MAX; ++word) {
    op_info *info = getopinfo(word);

    c8 *infodesc = getopdesc(info);
    printf("%04" PRIX32 ": %s\n", word, infodesc);
    free(infodesc);
  }

  return 0;
}
