;	handle PRT interrupts

	.global _timer_handler_1
	.global _should_animate

	.assume ADL=1
	.text

_timer_handler_1_tick_version:
	di
	push af
	IN0	a,(0x83) ;	resets PRT

	; reset tick
	ld a, 1
	ld (_should_animate), a

	pop af
	ei
	reti.lil



_timer_handler_1:
	di
	push af
	IN0	a,(0x83) ;	resets it.
	push bc
	push de
	push hl
	push ix
	push iy
	push hl

	; in C code
	call _animate_sprites

	pop hl
	pop iy
	pop ix
	pop hl
	pop de
	pop bc
	pop af
	ei
	reti.lil



