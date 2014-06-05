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

include "ID_ASM.EQU"

;===========================================================================
;
;                    SCALING GRAPHICS
;
;===========================================================================



MACRO	MAKELAB NUM

lab&NUM:

ENDM

MACRO	MAKEREF NUM

dw OFFSET lab&NUM

ENDM


;=========================================================================

MAXSCALES equ 256

	DATASEG

EXTRN	screenseg:WORD
EXTRN	linewidth:WORD

LABEL endtable WORD
labcount = 0
REPT MAXSCALES
MAKEREF %labcount
labcount = labcount + 1
ENDM


	CODESEG

;==================================================
;
; void scaleline (int scale, unsigned picseg, unsigned maskseg,
;                 unsigned screen, unsigned width)
;
;==================================================

PROC	ScaleLine pixels:word, scaleptr:dword, picptr:dword, screen:word
USES	si,di
PUBLIC	ScaleLine

;
; modify doline procedure for proper width
;
	mov    	bx,[pixels]
	cmp	bx,MAXSCALES
	jbe	@@scaleok
	mov     bx,MAXSCALES
@@scaleok:
	shl	bx,1
	mov	bx,[endtable+bx]
	push	[cs:bx]			;save the code that will be modified over
	mov	[WORD cs:bx],0d18eh	;mov ss,cx
	push	[cs:bx+2]		;save the code that will be modified over
	mov	[WORD cs:bx+2],90c3h	;ret / nop
	push	bx

	mov	dx,[linewidth]

	mov	di,[WORD screen]
	mov	es,[screenseg]

	mov	si,[WORD scaleptr]
	mov	ds,[WORD scaleptr+2]

	mov	bx,[WORD picptr]
	mov	ax,[WORD picptr+2]	;will be moved into ss after call

	mov	bp,bx

	cli
	call	doline
	sti
;
; restore doline to regular state
;
	pop	bx		;address of modified code
	pop     [cs:bx+2]
	pop     [cs:bx]

	mov	ax,ss
	mov	ds,ax
	ret

;================
;
; doline
;
; Big unwound scaling routine
;
; ds:si = scale table
; ss:bx = pic data
; es:di = screen location
;
;================

doline:

	mov	cx,ss
	mov	ss,ax		;can't call a routine with ss used...

labcount = 0

REPT MAXSCALES

MAKELAB %labcount
labcount = labcount + 1

	lodsb			; get scaled pixel number
	xlat	[ss:bx]		; look it up in the picture
	xchg	[es:di],al	; load latches and write pixel to screen
	add	di,dx		; down to next line

ENDM

	mov	ss,cx
	ret

ENDP

END