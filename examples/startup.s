


              .section registers, noinit
              .public _Dp, _Vfp
_Dp:          .space  16            ; direct page registers
_Vfp:         .space  4             ; virtual frame pointer (for VLAs)

              .extern main
              .extern _NearBaseAddress

              .section code,text
__startup
              lda ##.word2 _NearBaseAddress
              xba
              pha
              plb
              plb
              stz dp:.tiny(_Vfp)
              stz dp:.tiny(_Vfp+2)

              ; argc, argv
              lda ##0
              sta dp: _Dp
              sta dp: _Dp+2
              jsr main
              rtl


