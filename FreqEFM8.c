// FreqEFM8.c: Measure the frequency of a signal on pin T0.
//
// By:  Jesus Calvino-Fraga (c) 2008-2018
//
// The next line clears the "C51 command line options:" field when compiling with CrossIDE
//  ~C51~
  
#include <EFM8LB1.h>
#include <stdio.h>

#define SYSCLK      72000000L  // SYSCLK frequency in Hz
#define BAUDRATE      115200L  // Baud rate of UART in bps


unsigned char overflow_count;
const double r1 = 1000;
const double r2 = 1000;

void LCD_pulse(void)
{
    LCD_E = 1;
    Timer3us(40);
    LCD_E = 0;
}

void LCD_byte(unsigned char x)
{
    // The accumulator in the C8051Fxxx is bit-addressable
    ACC = x; // Send high nibble
    LCD_D7 = ACC_7;
    LCD_D6 = ACC_6;
    LCD_D5 = ACC_5;
    LCD_D4 = ACC_4;
    LCD_pulse();
    Timer3us(40);

    ACC = x; // Send low nibble
    LCD_D7 = ACC_3;
    LCD_D6 = ACC_2;
    LCD_D5 = ACC_1;
    LCD_D4 = ACC_0;
    LCD_pulse();
}

void WriteCommand(unsigned char x)
{
    LCD_RS = 0;
    LCD_byte(x);
    waitms(5);
}

void WriteData(unsigned char x)
{
    LCD_RS = 1;
    LCD_byte(x);
    waitms(2);
}

void LCD_4BIT(void)
{
    LCD_E = 0; // Resting state of LCD's enable is zero
    // LCD_RW = 0; // Not needed in this program, connect to GND
    waitms(20);

    // Ensure LCD starts in 8-bit mode before switching to 4-bit mode
    WriteCommand(0x33);
    WriteCommand(0x33);
    WriteCommand(0x32); // Switch to 4-bit mode

    // Configure LCD settings
    WriteCommand(0x28); // 4-bit mode, 2-line display, 5x8 font
    WriteCommand(0x0C); // Display ON, cursor OFF
    WriteCommand(0x01); // Clear screen command (takes some time)
    waitms(20); // Wait for the clear screen command to finish
}

void LCDprint(char *string, unsigned char line, bit clear)
{
    int j;

    // Set cursor position
    WriteCommand(line == 2 ? 0xC0 : 0x80);
    waitms(5);

    // Print the string
    for (j = 0; string[j] != 0; j++) 
        WriteData(string[j]);

    // Clear the rest of the line if required
    if (clear) 
        for (; j < CHARS_PER_LINE; j++) 
            WriteData(' ');
}


char _c51_external_startup (void)
{
	// Disable Watchdog with key sequence
	SFRPAGE = 0x00;
	WDTCN = 0xDE; //First key
	WDTCN = 0xAD; //Second key
  
	VDM0CN |= 0x80;
	RSTSRC = 0x02;

	#if (SYSCLK == 48000000L)	
		SFRPAGE = 0x10;
		PFE0CN  = 0x10; // SYSCLK < 50 MHz.
		SFRPAGE = 0x00;
	#elif (SYSCLK == 72000000L)
		SFRPAGE = 0x10;
		PFE0CN  = 0x20; // SYSCLK < 75 MHz.
		SFRPAGE = 0x00;
	#endif
	
	#if (SYSCLK == 12250000L)
		CLKSEL = 0x10;
		CLKSEL = 0x10;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 24500000L)
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 48000000L)	
		// Before setting clock to 48 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x07;
		CLKSEL = 0x07;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 72000000L)
		// Before setting clock to 72 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x03;
		CLKSEL = 0x03;
		while ((CLKSEL & 0x80) == 0);
	#else
		#error SYSCLK must be either 12250000L, 24500000L, 48000000L, or 72000000L
	#endif
	
	P0MDOUT |= 0x10; // Enable UART0 TX as push-pull output
	XBR0     = 0x01; // Enable UART0 on P0.4(TX) and P0.5(RX)                     
	XBR1     = 0X10; // Enable T0 on P0.0
	XBR2     = 0x40; // Enable crossbar and weak pull-ups

	#if (((SYSCLK/BAUDRATE)/(2L*12L))>0xFFL)
		#error Timer 0 reload value is incorrect because (SYSCLK/BAUDRATE)/(2L*12L) > 0xFF
	#endif
	// Configure Uart 0
	SCON0 = 0x10;
	CKCON0 |= 0b_0000_0000 ; // Timer 1 uses the system clock divided by 12.
	TH1 = 0x100-((SYSCLK/BAUDRATE)/(2L*12L));
	TL1 = TH1;      // Init Timer1
	TMOD &= ~0xf0;  // TMOD: timer 1 in 8-bit auto-reload
	TMOD |=  0x20;                       
	TR1 = 1; // START Timer1
	TI = 1;  // Indicate TX0 ready
	
	return 0;
}
 
// Uses Timer3 to delay <us> micro-seconds. 
void Timer3us(unsigned char us)
{
	unsigned char i;               // usec counter
	
	// The input for Timer 3 is selected as SYSCLK by setting T3ML (bit 6) of CKCON0:
	CKCON0|=0b_0100_0000;
	
	TMR3RL = (-(SYSCLK)/1000000L); // Set Timer3 to overflow in 1us.
	TMR3 = TMR3RL;                 // Initialize Timer3 for first overflow
	
	TMR3CN0 = 0x04;                 // Sart Timer3 and clear overflow flag
	for (i = 0; i < us; i++)       // Count <us> overflows
	{
		while (!(TMR3CN0 & 0x80));  // Wait for overflow
		TMR3CN0 &= ~(0x80);         // Clear overflow indicator
		if (TF0)
		{
		   TF0=0;
		   overflow_count++;
		}
	}
	TMR3CN0 = 0 ;                   // Stop Timer3 and clear overflow flag
}

void waitms (unsigned int ms)
{
	unsigned int j;
	for(j=ms; j!=0; j--)
	{
		Timer3us(249);
		Timer3us(249);
		Timer3us(249);
		Timer3us(250);
	}
}

void TIMER0_Init(void)
{
	TMOD&=0b_1111_0000; // Set the bits of Timer/Counter 0 to zero
	TMOD|=0b_0000_0101; // Timer/Counter 0 used as a 16-bit counter
	TR0=0; // Stop Timer/Counter 0
}

void main (void) 
{
	unsigned long F;
	
	double capacitance;

	TIMER0_Init();
	LCD_4BIT();

	char buffer[16];  

	waitms(500); // Give PuTTY a chance to start.
	printf("\x1b[2J"); // Clear screen using ANSI escape sequence.

	printf ("EFM8 Frequency measurement using Timer/Counter 0.\n"
	        "File: %s\n"
	        "Compiled: %s, %s\n\n",
	        __FILE__, __DATE__, __TIME__);

	while(1)
	{
		TL0=0;
		TH0=0;
		overflow_count=0;
		TF0=0;
		TR0=1; // Start Timer/Counter 0
		waitms(1000);
		TR0=0; // Stop Timer/Counter 0
		F=overflow_count*0x10000L+TH0*0x100L+TL0;
		
		capacitance = capacitance_calc(F);
		LCDprint("Capacitance:", 1, 1);
		sprintf(buffer, "%.2f uF", capacitance);  
        LCDprint(buffer, 2, 1);

		printf("\rf=%luHz", F);
		printf("\x1b[0K"); // ANSI: Clear from cursor to end of line.
	}
	
}
double capacitance_calc(unsigned long F){


    if (F == 0) return 0.0;
	double capacitance = 1.44 / ((r1 + 2 * r2) * F);

    return capacitance;

}
