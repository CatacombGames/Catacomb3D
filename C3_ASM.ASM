; Catacomb 3-D Source Code
; Copyright (C) 1993-2014 Flat Rock Software
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License along
; with this program; if not, write to the Free Software Foundation, Inc.,
; 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

IDEAL

MODEL	MEDIUM,C

VIEWWIDTH	=	(33*8)
GC_INDEX	=	03CEh

DATASEG
EVEN

;=================== Tables filled in by DrawVWall ==========================

;
; wallheight has the height (scale number) of that collumn of scaled wall
; it is pre bounded to 1-MAXSCALE (the actuial height on screen is 2*height)
;
wallheight	dw	VIEWWIDTH dup (?)

;
; wallwidth has the pixel width (1-7) of that collumn
;
wallwidth	dw	VIEWWIDTH dup (?)

;
; wallseg has the segment of the wall picture
;
wallseg		dw	VIEWWIDTH dup (?)

;
; wallofs has the offset of the wall picture
;
wallofs		dw	VIEWWIDTH dup (?)

;============================================================================

;
; screenbyte is just position/8
;
LABEL		screenbyte	WORD
pos	=	0
REPT		VIEWWIDTH
			dw	pos/8
pos	=	pos+1
ENDM

;
; screenbit is (position&7)*16
;
LABEL		screenbit	WORD
pos	=	0
REPT		VIEWWIDTH
			dw	(pos AND 7)*16
pos	=	pos+1
ENDM

;
; Use offset: screenbit[]+pixwidth*2
; acess from bitmasks-2+offset for one biased pixwidth
; the low byte of bitmasks is for the first screen byte, the high byte
; is the bitmask for the second screen byte (if non 0)
;

bitmasks	dw	0080h,00c0h,00e0h,00f0h,00f8h,00fch,00feh,00ffh
			dw	0040h,0060h,0070h,0078h,007ch,007eh,007fh,807fh
			dw	0020h,0030h,0038h,003ch,003eh,003fh,803fh,0c03fh
			dw	0010h,0018h,001ch,001eh,001fh,801fh,0c01fh,0e01fh
			dw	0008h,000ch,000eh,000fh,800fh,0c00fh,0e00fh,0f00fh
			dw	0004h,0006h,0007h,8007h,0c007h,0e007h,0f007h,0f807h
			dw	0002h,0003h,8003h,0c003h,0e003h,0f003h,0f803h,0fc03h
			dw	0001h,8001h,0c001h,0e001h,0f001h,0f801h,0fc01h,0fe01h


;
; wallscalecall is a far pointer to the start of a compiled scaler
; The low word will never change, while the high word is set to
; compscaledirectory[scale]
;
wallscalecall	dd	(65*6)			; offset of t_compscale->code[0]


PUBLIC	wallheight,wallwidth,wallseg,wallofs,screenbyte,screenbit
PUBLIC	bitmasks,wallscalecall


EXTRN	scaledirectory:WORD			; array of MAXSCALE segment pointers to
									; compiled scalers
EXTRN	screenseg:WORD				; basically just 0xa000
EXTRN	bufferofs:WORD				; offset of the current work screen

CODESEG

;============================================================================
;
; ScaleWalls
;
; AX	AL is scratched in bit mask setting and scaling
; BX	table index
; CX	pixwidth*2
; DX	GC_INDEX
; SI	offset into wall data to scale from, allways 0,64,128,...4032
; DI    byte at top of screen that the collumn is contained in
; BP	x pixel * 2, index into VIEWWIDTH wide tables
; DS	segment of the wall data to texture map
; ES	screenseg
; SS	addressing DGROUP variables
;
;============================================================================

PROC	ScaleWalls
PUBLIC	ScaleWalls
USES	SI,DI,BP

	xor	bp,bp						; start at location 0 in the tables
	mov	dx,GC_INDEX+1
	mov	es,[screenseg]

;
; scale one collumn of data, possibly across two bytes
;
nextcollumn:

	mov	bx,[wallheight+bp]			; height of walls (1-MAXSCALE)
	shl	bx,1
	mov	ax,[ss:scaledirectory+bx]	; segment of the compiled scaler
	mov [WORD PTR ss:wallscalecall+2],ax

	mov	cx,[wallwidth+bp]
	or	cx,cx
	jnz	okwidth
	mov	cx,2
	jmp	next

okwidth:
	shl	cx,1
	mov	ds,[wallseg+bp]
	mov	si,[wallofs+bp]

	mov	di,[screenbyte+bp]			; byte at the top of the scaled collumn
	add	di,[ss:bufferofs]			; offset of current page flip
	mov	bx,[screenbit+bp]			; 0-7 << 4
	add	bx,cx
	mov	ax,[ss:bitmasks-2+bx]
	out	dx,al						; set bit mask register
	call [DWORD PTR ss:wallscalecall]		; scale the line of pixels
	or	ah,ah						; is there anything in the second byte?
	jnz	secondbyte
;
; next
;
next:
	add	bp,cx
	cmp	bp,VIEWWIDTH*2
	jb	nextcollumn
	jmp	done

;
; draw a second byte for vertical strips that cross two bytes
;
secondbyte:
	mov	al,ah
	inc	di								; next byte over
	out	dx,al							; set bit mask register
	call [DWORD PTR ss:wallscalecall]	; scale the line of pixels
;
; next
;
	add	bp,cx
	cmp	bp,VIEWWIDTH*2
	jb	nextcollumn

done:
	mov	ax,ss
	mov	ds,ax
	ret

ENDP


END

