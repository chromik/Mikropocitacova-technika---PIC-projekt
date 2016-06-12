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


// int = 16bit var, char = 8bit var


#define FS1 RB4
#define FS2 RB5


struct motor{
    char x;
    char y; // aktualni smer
    int uin; // napeti na AN0
    int uin_prev; // predchozi napeni na AN0
}motor;

void motor_set()
{
    motor.x = 0;
    motor.y = 1;
}

inline void motor_run()
{
    RD4 = motor.x;
    RD7 = motor.y; // rozbehnuti motoru
}

inline void rotate_change()
{
    motor.x = !motor.x;
    motor.y = !motor.y; // negace vystupu
}

inline void ports_configure();
inline void timer1_configure();
inline void adconverter_configure();


void measure_predelay_20ms();
void motor_stop_1s();
void time_delay_2s();
void motor_running_10s();


int measure(); // vrati hodnotu A/D prevodniku

inline void ports_configure()
{
    /* PORTS CONFIGURE */
    TRISD = 0; /* PORT D jako VYSTUPNI */
    TRISB4 = 1; /* PORT RB4 jako VSTUPNI*/
    TRISB5 = 1; /* PORT RB5 jako VSTUPNI*/
    TRISE0 = 0; /* RE0 jako VYSTUPNI*/
    TRISA = 0x01; /* AN0 jako vstupni */
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


inline void adconverter_configure()
{
    // http://www.cuteminds.com/index.php/electronics/picmicro/picprorudiments/117-adcregs/

    ADCON1 = 0b000001110;  // Left justify, AN0 = analog, VREF+ = Vdd, VREF- = Vss, CHAN/Refs = 1/0
    ADCON0 = 0b10000001; // F/8, Channel 0 (RA0/AN0), Status=End, Active=A/D on
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

void motor_stop_1s()
{
    RD4 = 1;
    RD7 = 1; // zastaveni motoru
    // fout =   fclk/ (4*prescaler*(65536-TMR1) * Count) =   18432000 / (4*1*(65536-19456) * 100) = 1Hz -> t = 1s
    for(int i=0; i<100; ++i) {
        TMR1 = 19456; // pocatecni hodnota citace
        while(!TMR1IF);
        TMR1IF = 0;
    }
}

void time_delay_2s()
{
    // fout =   fclk/ (4*prescaler*(65536-TMR1) * Count) =   18432000 / (4*1*(65536-19456) * 200) = 0.5Hz -> t = 2s
    for(int i=0; i<200; ++i) {
        TMR1 = 19456; // pocatecni hodnota citace
        while(!TMR1IF);
        TMR1IF = 0;
    }
}

void motor_running_10s()
{
    // fout =   fclk/ (4*prescaler*(65536-TMR1) * Count) =   18432000 / (4*1*(65536-19456) * 1000) = 0.1Hz -> t = 10s
    for(int i=0; i<1000; ++i) {
        TMR1 = 19801; // pocatecni hodnota citace
        while(!TMR1IF);

        if (check_disrupt()) {
            motor_stop_1s(); //  zastav motor
            time_delay_2s(); // na 2 sekundy
            motor_run(); // pokracovat
        }

        TMR1IF = 0;
    }
}

int measure()
{
    ADCON0bits.GO_DONE = 1;  // Spusi A/D prevod
    while(ADCON0bits.GO_DONE); // Ceka na konec A/D prevodu
    int value = (ADRESH << 2) | ( (ADRESL & 0xc0) >> 6); // 8 bitu z ADRESH a 2 bity z ADRESL
    return value;
}

int check_disrupt()
{
     motor.uin = measure();
     if ( (motor.uin ^ motor.uin_prev) & 0b1000000000) // Napeti prekrocilo polovicni hranici (MAX je 5V, polovina 2,5), pokud se lisi v nejvyssim bitu, tak prekroceni
     {
         motor.uin_prev = motor.uin;
         return 1;
     }
     else {
         motor.uin_prev = motor.uin;
         return 0;
     }
}


void main(void)
{
    adconverter_configure(); // nastaveni A/D prevodniku
    GIE = 0; // zakazat preruseni
    ports_configure(); // nastaveni vstup/vystup u pouzivanych portu
    timer1_configure(); // nastaveni citace/casovace ?.1
   
    motor_set(); // nastavi promenne X a Y
    measure_predelay_20ms(); // kvuli nastaveni A/D prevodniku cekat 20ms pred prvnim prevodem nez se ustali
    motor.uin_prev = motor.uin = measure(); // zmereni napeti na AN0 a ulozi do promennyh

    while(1) {
        //asm("nop");
        motor_run();
        motor_running_10s();

        motor_stop_1s();

        rotate_change();
        motor_run();
        motor_running_10s();

        motor_stop_1s();

        rotate_change();
    }
}