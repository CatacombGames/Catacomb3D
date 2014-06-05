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

; JABHACK.ASM

.386C
IDEAL
MODEL	MEDIUM

EXTRN	LDIV@:far

;============================================================================

DATASEG

EXTRN	_intaddr:word

;============================================================================

CODESEG

;	Hacked up Juan Jimenez's code a bit to just return 386/not 386
PROC	_CheckIs386
PUBLIC	_CheckIs386

	pushf			; Save flag registers, we use them here
	xor	ax,ax		; Clear AX and...
	push ax			; ...push it onto the stack
	popf			; Pop 0 into flag registers (all bits to 0),
	pushf			; attempting to set bits 12-15 of flags to 0's
	pop	ax			; Recover the save flags
	and	ax,08000h	; If bits 12-15 of flags are set to
	cmp	ax,08000h	; zero then it's 8088/86 or 80188/186
	jz	not386

	mov	ax,07000h	; Try to set flag bits 12-14 to 1's
	push ax			; Push the test value onto the stack
	popf			; Pop it into the flag register
	pushf			; Push it back onto the stack
	pop	ax			; Pop it into AX for check
	and	ax,07000h	; if bits 12-14 are cleared then
	jz	not386		; the chip is an 80286

	mov	ax,1		; We now assume it's a 80386 or better
	popf
	retf

not386:
	xor	ax,ax
	popf
	retf

	ENDP


PROC	_jabhack2
PUBLIC	_jabhack2

	jmp	@@skip

@@where:
	int	060h
	retf

@@skip:
	push	es

	mov	ax,seg LDIV@
	mov	es,ax
	mov	ax,[WORD PTR @@where]
	mov	[WORD FAR es:LDIV@],ax
	mov	ax,[WORD PTR @@where+2]
	mov	[WORD FAR es:LDIV@+2],ax

	mov	ax,offset @@jabdiv
	mov	[_intaddr],ax
	mov	ax,seg @@jabdiv
	mov	[_intaddr+2],ax

	pop	es
	retf

@@jabdiv:
	add	sp,4	;Nuke IRET address, but leave flags
	push bp
	mov	bp,sp	;Save BP, and set it equal to stack
	cli

	mov	eax,[bp+8]
	cdq
	idiv [DWORD PTR bp+12]
	mov	edx,eax
	shr	edx,16

	pop	bp		;Restore BP
	popf 		;Restore flags (from INT)
	retf	8	;Return to original caller

	ENDP

	END
