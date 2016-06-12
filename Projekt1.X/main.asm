#include p16f877a.inc

; CONFIG
; __config 0xFFFB
 __CONFIG _FOSC_HS & _WDTE_OFF & _PWRTE_ON & _BOREN_OFF & _LVP_OFF & _CPD_OFF & _WRT_OFF & _CP_OFF

; frekvence krystalu je nastavena na 18.432 MHz (4 impulsy = 1 strojovy cyklus)
; 18432000/4 = 4608000     ->   1 sekunda = 4608000 strojovych cyklu

    cblock 0x20
        v0
        v1
        v2
    endc

    org 0

; volba Bank1 (TRISD je v Bank1)
    bsf STATUS,RP0 ; RP0 = 1
    bcf STATUS,RP1 ; RP1 = 0

; nastaveni PORTD jako vystupni
    bcf TRISD,4 ; TRISB.4=0 PORTB.4 bude vystupni
    bcf TRISD,7 ; TRISB.7=0 PORTB.7 bude vystupni

; volba Bank0
    bcf STATUS,RP0 ; RPO = 0

main_loop:
    ; toceni antenou v jednom smeru
    bsf PORTD,7 ; RD7 = 1
    bcf PORTD,4 ; RD4 = 0

    ; casova smycka 3 sekundy
    call Delay1s
    call Delay1s
    call Delay1s


    ; zastaveni anteny
    bsf PORTD,4 ; RD4 = 1
    ;casova smycka 1 sekunda
    call Delay1s


    ; toceni antenou v druhem smeru
    bcf PORTD,7 ; RD7 = 0
    ;casova smycka 3 sekundy
    call Delay1s
    call Delay1s
    call Delay1s


    ; zastaveni anteny
    bsf PORTD,7 ; RD7 = 1
    ;casova smycka 1 sekunda
    call Delay1s
   

    goto main_loop

Delay1s: ; 4607993 strojovych cyklu
	movlw	D'108'
	movwf	v0
	movlw	D'12'
	movwf	v1
	movlw	D'11'
	movwf	v2
Delay1s_0:
	decfsz	v0, f
	goto	$+2
	decfsz	v1, f
	goto	$+2
	decfsz	v2, f
	goto	Delay1s_0

	; + 3 strojove cykly
	nop
	nop
	nop

	; + 4 strojove cykly (call: 2, return: 2)
	return

    END