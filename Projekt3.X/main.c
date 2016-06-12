#include <pic.h>
#include <xc.h>

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

// CONFIGURATION BITS
#pragma config FOSC = HS        // Oscillator Selection bits (HS oscillator)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = ON       // Power-up Timer Enable bit (PWRT enabled)
#pragma config BOREN = OFF       // Brown-out Reset Enable bit (BOR enabled)
#pragma config LVP = OFF         // Low-Voltage (Single-Supply) In-Circuit Serial Programming Enable bit (RB3/PGM pin has PGM function; low-voltage programming enabled)
#pragma config CPD = OFF        // Data EEPROM Memory Code Protection bit (Data EEPROM code protection off)
#pragma config WRT = OFF        // Flash Program Memory Write Enable bits (Write protection off; all program memory may be written to by EECON control)
#pragma config CP = OFF         // Flash Program Memory Code Protection bit (Code protection off)

#define FS1 RB4
#define FS2 RB5

// pocatecni inicializace promennych
char kontrolovat_smer = 1;
char merit = 0;

unsigned char rb;

int AN0_max_napeti = 0;
int AN0_napeti = 0;
int pocet_rotaci = 0;
int poloha_aktualni = -1;
int poloha_maximalni = 0;

char motor_x = 1;
char motor_y = 0;

int cas = 0;

void motor_start()
{
    RD4 = motor_x;
    RD7 = motor_y;
}

void motor_stop()
{
    RD4 = 1;
    RD7 = 1;
}

bit is_motor_running()
{
    if (RD4 && RD7)
        return 0;
    else
        return 1;
}

void motor_direct_switch()
{
    motor_x = !motor_x;
    motor_y = !motor_y;
}

void set_in_out()
{
    TRISB = 0x33; // RB0=0, RB1=1, RB4=1, RB5=1 as INPUT
                  // RB2=0, RB3=0, RB6=0, RB7=0 as OUTPUT

    TRISD = 0; // RD0 - RD7  as OUTPUT


    TRISE0 = 0; /* RE0 jako VYSTUPNI*/

    TRISA = 0x01; /* AN0 jako vstupni */

    INTEDG = 0; /* Interrupt on falling edge of RB0/INT pin */
}

void set_interrupts()
{

    RBIE = 1; // Enables the RB port change interrupt
    INTE = 1; // Enables the RB0/INT external interrupt
    INTCON = 0b11011000;
    //INTCON.GIE = 1;        // enable global interrupts
    //INTCON.INTE = 1;       // enable External interrupts
    //INTCON.RABIE = 1;      // enable interrupt on change (IOC) for ports A & B
    PIR2 = 0;              // clear interrupts
}

int measure()
{
    ADCON0bits.GO_DONE = 1;  // Spusi A/D prevod
    while(ADCON0bits.GO_DONE); // Ceka na konec A/D prevodu
    int value = (ADRESH << 2) | ( (ADRESL & 0xc0) >> 6); // 8 bitu z ADRESH a 2 bity z ADRESL
    return value;
}

bit je_kladny_smer()
{


    //while ( (PORTB & 0b00110000) >> 2 == rb ); // cekat, dokud nenastane zmena na FS1 FS2
    
    rb |= (PORTB & 0b00110000); // precist FS1 a FS2 po zmene

    switch (rb) {

        case 0b00101100: // FS2_nove=1 FS1_nove=0 FS2_stare=1 FS1_stare=1  -> kladny
        case 0b00110100: // FS2_nove=1 FS1_nove=1 FS2_stare=0 FS1_stare=1  -> kladny
        case 0b00001000: // FS2_nove=0 FS1_nove=0 FS2_stare=1 FS1_stare=0  -> kladny
        case 0b00010000:
            return 1;

        case 0b00011100: // FS2_nove=0 FS1_nove=1 FS2_stare=1 FS1_stare=1  -> zaporny
        case 0b00000100: // FS2_nove=0 FS1_nove=0 FS2_stare=0 FS1_stare=1  -> zaporny
        case 0b00111000: // FS2_nove=1 FS1_nove=1 FS2_stare=1 FS1_stare=0  -> zaporny
        case 0b00100000:
            return 0;
    }
}

void interrupt preruseni()
{
    asm("nop");
    asm("nop");
    asm("nop");
    if (INTF) // The RB0/INT external interrupt occurred. It must be cleared in software. // MAGNET
    {
        poloha_aktualni = 0;
        ++pocet_rotaci;
        INTF = 0;  // clear the interrupt 0 flag

    }

    if (RBIF) // At least one of the RB7 ? RB4 pins changed state // FOTOSNIMACE
    {
        if (kontrolovat_smer) {
            kontrolovat_smer = 0;
            if (!je_kladny_smer()) {
                motor_stop();
                motor_direct_switch();
                motor_start();
                //kontrolovat_smer = 0;
            }
        }

        else if (merit == 0) {
            ++poloha_aktualni;
        }

        else if (merit == 1) {
            ++poloha_aktualni;
            AN0_napeti = measure();
            if (AN0_napeti > AN0_max_napeti) {
                AN0_max_napeti = AN0_napeti;
                poloha_maximalni = poloha_aktualni;
            }
        }
        char tmp = PORTB;
        RBIF = 0; // clear the interrupt 0 flag
    }
}



inline void adconverter_configure()
{
    // http://www.cuteminds.com/index.php/electronics/picmicro/picprorudiments/117-adcregs/

    ADCON1 = 0b000001110;  // Left justify, AN0 = analog, VREF+ = Vdd, VREF- = Vss, CHAN/Refs = 1/0
    ADCON0 = 0b10000001; // F/8, Channel 0 (RA0/AN0), Status=End, Active=A/D on
}

inline void timer1_configure()
{
    // fout =   fclk/ (4*prescaler*(65536-TMR1) * Count) =   18432000 / (4*1*(65536-19456) * Count)

    TMR1ON = 1; /* povoleni citace */
    TMR1CS = 0; /* interni hodiny jako zdroj signalu (18432000 Hz) */
    T1CKPS0 = 0; /* 1:1 deleni */
    T1CKPS1 = 0;
    TMR1 = 19456; /* pocatecni hodnota citace */
}


void delay_10ms()
{
    // fout =   fclk/ (4*prescaler*(65536-TMR1) * Count) =   18432000 / (4*1*(65536-19456) * 2) = 50Hz -> t= 0.02 s = 20ms
    for(int i=0; i<1; ++i) {
        TMR1 = 19456; // pocatecni hodnota citace
        while(!TMR1IF);
        TMR1IF = 0;
    }
}

void delay_50ms()
{
    // fout =   fclk/ (4*prescaler*(65536-TMR1) * Count) =   18432000 / (4*1*(65536-19456) * 2) = 50Hz -> t= 0.02 s = 20ms
    for(int i=0; i<5; ++i) {
        TMR1 = 19456; // pocatecni hodnota citace
        while(!TMR1IF);
        TMR1IF = 0;
    }
    asm("nop");
}

void measure_predelay_20ms()
{
     // fout =   fclk/ (4*prescaler*(65536-TMR1) * Count) =   18432000 / (4*1*(65536-19456) * 2) = 50Hz -> t= 0.02 s = 20ms
    for(int i=0; i<2; ++i) {
        TMR1 = 19456; // pocatecni hodnota citace
        while(!TMR1IF);
        TMR1IF = 0;
    }
}

void main(void)
{
    set_interrupts(); // nastaveni preruseni
    set_in_out(); // nastaveni vstup/vystup u portu

    timer1_configure(); // konfigurace Citace/Casovace cislo 1

    adconverter_configure(); // konfigurace AD prevodniku
    measure_predelay_20ms(); // kvuli nastaveni A/D prevodniku cekat 20ms pred prvnim prevodem nez se ustali
    measure_predelay_20ms(); // kvuli nastaveni A/D prevodniku cekat 20ms pred prvnim prevodem nez se ustali

    rb = (PORTB & 0b00110000) >> 2; // precist FS1 a FS2
    motor_start(); // spusteni motoru
    while(pocet_rotaci != 1); // pocka na dokonceni otacky
    motor_stop(); // zastavi motor

    pocet_rotaci = 0; // vynuluje citac otacek
    poloha_aktualni = poloha_maximalni = AN0_max_napeti = 0; // nastavit na 0 maximalni namerene hodnoty

    while (1) {
        cas = 0;

        if (RB1) continue; // pokud tlacitko neni stisknute na zacatek cykluu
        delay_50ms();
        if (RB1) continue; // pokud tlacitko bylo stisknute, ale po 50ms neni, jednalo se o zakmit a skok na zacatek cyklu
        cas += 5; // nejednalo se o zakmit, prictu tedy 50ms co jiz ubehlo

        while(!RB1) { // dokud je tlacitko stisknuto
            delay_10ms(); // 10ms pauza
            ++cas; // a inkrementuj pocitani casu
        }

        delay_50ms();

        if (cas > 100) {

            if (poloha_aktualni != 0) // pokud nejsem na zacatku
            {
                pocet_rotaci = 0; // vynuluju meric otacek
                motor_start(); // zapnu motor
                while (pocet_rotaci != 1); // a dokoncim otacku
            }

            pocet_rotaci = 0; // vynuluju pocitadlo rotaci
            merit = 1; // zapnu mereni napeti
            poloha_maximalni = AN0_max_napeti = 0; // vynulu namerene maxima (mohli zustat po predchozim mereni)
            motor_start(); // zapnu motor

            while (pocet_rotaci != 1); // ihned po prvni otacce
            merit = 0; // vypnu mereni napeti

            while (poloha_aktualni != poloha_maximalni && pocet_rotaci != 2); // pockam nez se motor dostane do maximalni polohy
            motor_stop(); // zastavim motor

        }

        else if (cas < 30 && cas > 0) {
            if (is_motor_running()) {
                motor_stop();
            } else {
                motor_start();
            }
        }

        else continue;
    }
}



