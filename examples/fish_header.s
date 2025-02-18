
;
; --code-model small  --data-model large
;


stack_begin .macro

	phb
	phk
	plb
	tsc
	sec
	sbc ##dp_size
	tcs
	inc a
	phd
	tcd
	stz dp: _Vfp
	stz dp: _Vfp+2

	.endm

stack_end .macro

	pld
	tsc
	clc
	adc ##dp_size
	tcs
	plb
	.endm

_brk	.macro operand
	.byte 0x00,\operand
	.endm


	.extern nda_open, nda_close, nda_init, nda_action
	.section code,text

period	.equ 0xffff ; never
mask    .equ 0xffff ; all events


	.public _Dp, _Vfp

_Dp	.equ 0
_Vfp    .equ 16
dp_size .equ 20



header:

	.long _open
	.long _close
	.long _action
	.long _init
	.word period
	.word mask
	.asciz "--fish\\H**"


_close:
; inputs: (none)
; outputs: (none)

	stack_begin
	jsr nda_close
	stack_end
	rtl

_init:
; inputs:
; a <> 0 if called by DeskStartUp
; a = 0 if called by DeskShutDown
; outputs: (none)
	tay ; save
	stack_begin
	tya
	jsr nda_init
	stack_end
	rtl

_open:
; inputs: (none)
; outputs: window pointer stored on the stack a pascal return value
;
; 4 return space
; 4 rtlb
; [20 bytes] _Dp / _Vfp
; ^---dp
; 2 saved dp
; ^-- stack 
;

	stack_begin
	jsr nda_open
	stx dp: 26 ; high
	sta dp: 24 ; low 
	stack_end
	rtl


_action:
; inputs:
; a - action code
; x/y - ptr to various things, depending on action code
; outputs:
; a - flag indicating if handled

	pha ; save....
	stack_begin
	sty dp: 2 ; high
	stx dp: 0 ; low
	lda dp: dp_size + 1 ; restore (+1 for b stored on stack)
	jsr nda_action
	tay ; save
	stack_end
	plx ;
	tya
	rtl
