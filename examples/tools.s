
            .extern _Dp

tool        .macro num
            ldx ##\num
            jsl long: 0xe10000
            sta long: _toolErr
            .endm


            .section code,text
            .global ReadTime
ReadTime        
;
; not actually a toolcall but eh
;
; returns seconds since epoch (Jan 1, 1904)
           sep #0x30
           jsl long: 0xe1008c ; TOREADTIME
           rep #0x30
           bcs +
           lda long: 0xe103e3 ;CLKRDATA+2
           tax
           lda long: 0xe103e1 ;CLKRDATA
           rts
+:
           lda ##0
           ldx ##0
           rts

            .section code,text
            .global CloseWindow
CloseWindow:
            pei dp: _Dp+2
            pei dp: _Dp
            tool 0x0b0e
            rts

            .section code,text
            .global SetSysWindow
SetSysWindow:
            pei dp: _Dp+2
            pei dp: _Dp
            ldx ##0x4b0e
            jsl 0xe10000
            rts

            .section code,text
            .global BeginUpdate
BeginUpdate:
            pei dp: _Dp+2
            pei dp: _Dp
            ldx ##0x1e0e
            jsl 0xe10000
            rts

            .section code,text
            .global EndUpdate
EndUpdate:
            pei dp: _Dp+2
            pei dp: _Dp
            ldx ##0x1f0e
            jsl 0xe10000
            rts


            .section code,text
            .global DrawControls
DrawControls:
            pei dp: _Dp+2
            pei dp: _Dp
            ldx ##0x1010
            jsl 0xe10000
            rts


            .section code,text
            .global NewWindow
NewWindow:
            pea #0
            pea #0
            pei dp: _Dp+2
            pei dp: _Dp
            ldx ##0x090e
            jsl 0xe10000
            pla
            plx
            rts


            .section code,text
            .global MoveTo
MoveTo:
            pha
            pei dp: _Dp
            ldx ##0x3a04
            jsl 0xe10000
            rts

            .section code,text
            .global SetBackColor
SetBackColor:
            pha
            ldx ##0xa204
            jsl 0xe10000
            rts

            .section code,text
            .global SetFontFlags
SetFontFlags:
            pha
            ldx ##0x9804
            jsl 0xe10000
            rts


            .section code,text
            .global SetSolidBackPat
SetSolidBackPat:
            pha
            ldx ##0x3804
            jsl 0xe10000
            rts


            .section code,text
            .global SetBackPat
SetBackPat:
            pei dp: _Dp+2
            pei dp: _Dp
            ldx ##0x3404
            jsl 0xe10000
            rts

            .section code,text
            .global PaintRect
PaintRect:
            pei dp: _Dp+2
            pei dp: _Dp
            ldx ##0x5404
            jsl 0xe10000
            rts


            .section code,text
            .global EraseRect
EraseRect:
            pei dp: _Dp+2
            pei dp: _Dp
            ldx ##0x5504
            jsl 0xe10000
            rts



            .section code,text
            .global GetMasterSCB
GetMasterSCB:
            pea #0
            ldx ##0x1704
            jsl 0xe10000
            pla
            rts



            .section code,text
            .global InvalRect
InvalRect:
            pei dp: _Dp+2
            pei dp: _Dp
            ldx ##0x3a0e
            jsl 0xe10000
            rts



            .section code,text
            .global GetPort
GetPort:
            pea #0
            pea #0
            ldx ##0x1c04
            jsl 0xe10000
            pla
            plx
            rts

            .section code,text
            .global SetPort
SetPort:
            pei dp: _Dp+2
            pei dp: _Dp
            ldx ##0x1b04
            jsl 0xe10000
            rts



            .section code,text
            .global DrawCString
DrawCString:
            pei dp: _Dp+2
            pei dp: _Dp
            ldx ##0xa604
            jsl 0xe10000
            rts

            .section code,text
            .global CStringWidth
CStringWidth:
            pea #0
            pei dp: _Dp+2
            pei dp: _Dp
            ldx ##0xaa04
            jsl 0xe10000
            pla
            rts

            .section code,text
            .global InstallFont
InstallFont:
; _Dp is scale
; a:x is font id
            phx
            pha
            pei dp: _Dp
            ldx ##0x0e1b
            jsl 0xe10000
            rts

            .section code,text
            .global FMStatus
FMStatus:
            pea #0
            tool 0x061b
            pla
            rts




            .section code,text
            .global ReadTimeHex
ReadTimeHex:
; _Dp, _Dp+2 is pointer to TimeRec struct
            lda ##0
            pha
            pha
            pha
            pha
            ldx ##0x0d03
            jsl 0xe10000

            ldy ##2
            pla
            sta [dp: _Dp]
            pla
            sta [dp: _Dp],y
            iny
            iny
            pla
            sta [dp: _Dp],y
            iny
            iny
            pla
            sta [dp: _Dp],y

            rts



            .section zdata,bss
            .global _toolErr
_toolErr:   .space 2
