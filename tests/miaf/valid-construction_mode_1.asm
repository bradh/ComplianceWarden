%define BE(a) ( ((((a)>>24)&0xFF) << 0) + ((((a)>>16)&0xFF) << 8) + ((((a)>>8)&0xFF) << 16)  + ((((a)>>0)&0xFF) << 24) )

ftyp_start:
dd BE(ftyp_end - ftyp_start)
db "ftyp"

db "isom"
dd BE(0x200)
db "mif1", "miaf"

ftyp_end:

meta_start:
dd BE(meta_end - meta_start)
db "meta"
dd BE(0)

hdlr_start:
dd BE(hdlr_end - hdlr_start)
db "hdlr"
dd BE(0)
dd BE(0)
db "pict"
dd BE(0)
dd BE(0)
dd BE(0)
hdlr_end:

iinf_start:
dd BE(iinf_end - iinf_start)
db "iinf"
dd BE(0)
iinf_end:

iloc_start:
dd BE(iloc_end - iloc_start)
db "iloc"
dd BE(0x01000000)
dd BE(1) ; 1 item
dd BE(1) ; construction_method(1)
iloc_end:

iref_start:
dd BE(iref_end - iref_start)
db "iref"
iref_end:

meta_end:

; vim: syntax=nasm