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
argument: WS? (COMMA | WS) WS? (REGISTER | HALFWORD | LABEL);

directive: DOT DIRECTIVE_NAME WS WORD;

label: LABEL COLON;

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

COMMA: ',';
COLON: ':';
DOT: '.';

REGISTER: 'r' [0-9A-F];
HALFWORD: [0-9A-F] [0-9A-F];
WORD: HALFWORD HALFWORD;

LABEL: ~[ \t\n\r:,0-9] ~[ \t\n\r:,]*;
COMMENT: ';' ~[\n\r]*;

WS: [ \t]+;
EOL: [\n\r];
