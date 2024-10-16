grammar asmxtoy;

file: (line? EOL)* line? EOF;

line
    : WS
    | WS? operation WS?
    ;

operation
    : instruction
    | directive
    | label
    | COMMENT
    ;

instruction: MNEMONIC argument*;
argument: WS? (COMMA | WS) WS? (REGISTER | IMMEDIATE | LABEL_USE);

directive: DOT DIRECTIVE_NAME WS IMMEDIATE;

label: LABEL_DEF;

MNEMONIC
    : 'hlt' // 0XXX
    | 'add' // 1XXX
    | 'sub' // 2XXX
    | 'and' // 3XXX
    | 'xor' // 4XXX
    | 'asl' // 5XXX
    | 'asr' // 6XXX
    | 'lda' // 7XXX
    | 'lod' // 8XXX
    | 'str' // 9XXX
    | 'ldi' // AXXX
    | 'sti' // BXXX
    | 'brz' // CXXX
    | 'brp' // DXXX
    | 'jmp' // EXXX
    | 'jsr' // FXXX
    ;

DIRECTIVE_NAME
    : 'ORG'
    | 'WORD'
    ;

DIGIT: [0-9A-F];

REGISTER: 'r' DIGIT;
IMMEDIATE: DIGIT+;

LABEL_DEF: LABEL_NAME ':';
LABEL_USE: '$' LABEL_NAME;
LABEL_NAME: ~[ \t\n\r:,$.]+;

DOT: '.';
COMMA: ',';

COMMENT: ';' ~[\n\r]*;

WS: [ \t]+;
EOL: [\n\r];
