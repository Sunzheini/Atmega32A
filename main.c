#define F_CPU 1000000L //from 1 to 16MHz
#include <avr/io.h> //2.7 - 5.5V
#include <util/delay.h>
#include <avr/interrupt.h> //available 3 ext. interrupts

//Fuse Bits: 
//high fuse 0x99 and low fuse 0xE1
//Internal clock is 1 / 2 / 4 or 8MHz
//Brown-out is (below) 2.7 or 4V
//you can disable reset @ pin9, but not usually helpful
//by default JTAG is enabled and uses C2, 3, 4 and 5

//ne sym minal timer, counter i serial

//MACROS
#define LED0_ON() PORTD |= (1<<4) //DDRB = 0xFF;
#define LED0_OFF() PORTD &= ~(1<<4)
#define LED1_ON() PORTD |= (1<<5)
#define LED1_OFF() PORTD &= ~(1<<5)
#define LED2_ON() PORTD |= (1<<6)
#define LED2_OFF() PORTD &= ~(1<<6)
#define DELAY_1S() _delay_ms(1000)

#define rs_high() PORTD |= (1 << 0);
#define rs_low() PORTD &= ~(1 << 0);

//#define rs_high() PORTC |= (1 << 7);
//#define rs_low() PORTC &= ~(1 << 7);

#define en_high() PORTD |= (1 << 1);
#define en_low() PORTD &= ~(1 << 1);

//#define en_high() PORTC |= (1 << 6);
//#define en_low() PORTC &= ~(1 << 6);

#define lcdport PORTC

//int stop_watch = 0;
int stop_watchflag = 0;
int stop_seconds = 0;
int stop_minutes = 0;
int stop_hours = 0;

//LCD Functions
//-----------------------------------------------------------//

// function to write command on LCD
void lcd_cmd(unsigned char cmd)
{
	lcdport = cmd;
	rs_low();		// rs = 0 to select the command register
	en_high();
	_delay_ms(1);
	en_low();
}

// function to write data on LCD
void lcd_data(unsigned char dat)
{
	lcdport = dat;
	rs_high();		// rs = 0 to select the command register
	en_high();
	_delay_ms(1);
	en_low();
}

void lcd_init()
{
	lcd_cmd(0x38); _delay_ms(5); //2 lines and 5 x 7 matrix
	lcd_cmd(0x02); _delay_ms(5); // Return Home
	lcd_cmd(0x01); _delay_ms(5); //Clear screen
	lcd_cmd(0x0C); _delay_ms(5); //Display On, Cursor Off
	lcd_cmd(0x06); _delay_ms(5); //Increment cursor (shift to right)
	lcd_cmd(0x80); _delay_ms(5); //Force tp beginning of 1st line
}

void cursorxy(char x, char y)
{
	if( ( x < 1 || x > 2) || (y < 1 || y > 16) )
	{
		x = 1;
		y = 1;
	}
	if(x == 1)
	lcd_cmd(0x7F+y);
	else
	lcd_cmd(0xBF+y);
}
//function to print any input value upto the desired digit on lcd
void lcd_print(char row, char coloumn, unsigned int value, int digits)
{
	unsigned int temp;
	unsigned int unit;
	unsigned int tens;
	unsigned int hundred;
	unsigned int thousand;
	unsigned int million;
	
	unsigned char flag=0;
	
	
	if(row==0||coloumn==0)
	{
		cursorxy(1,1);
	}
	else
	{
		cursorxy(row,coloumn);
	}
	if(digits==5 || flag==1)
	{
		million=value/10000+48;
		lcd_data(million);
		flag=1;
	}
	if(digits==4 || flag==1)
	{
		temp = value/1000;
		thousand = temp%10 + 48;
		lcd_data(thousand);
		flag=1;
	}
	if(digits==3 || flag==1)
	{
		temp = value/100;
		hundred = temp%10 + 48;
		lcd_data(hundred);
		flag=1;
	}
	if(digits==2 || flag==1)
	{
		temp = value/10;
		tens = temp%10 + 48;
		lcd_data(tens);
		flag=1;
	}
	if(digits==1 || flag==1)
	{
		unit = value%10 + 48;
		lcd_data(unit);
	}
	if(digits>5)
	{
		lcd_data('e');
	}
}

void lcd_out(char x, char y, char *str)
{
	cursorxy(x,y);
	while(*str)
	{
		lcd_data(*str);
		str++;
	}

}

//ADC Function
//-----------------------------------------------------------//

int adc_read(char channel)
{
	unsigned int b,result;
	
//ADMUX Register
//XX - Ref V; X - Adjust; XXXXX - Channel
//00 use AREF pin, 0 use ADJUST, 00111 select channel (pin) 7
	ADMUX = channel;
	
//ADCSRA Register
//X - Enable; X - start conversion (then poll to be 0); X_XXXX - n/a
//_X____ - ADIF Flag - bit is set when conversion is complete	
	ADCSRA = 0x80; //1000 0000 enabled but not started conversion yet
	
//ADCH (2 higher bits) and ADCL (8 lower bits) Result Registers	
	ADCH = 0x00; // initial data is null
	ADCL = 0x00; // initial data is null

//Start	
	ADCSRA |= (1 << 6); // start conversion
	while ( (ADCSRA & 0x40) != 0 ); // wait till conv finished
	b = ADCL;
	result = ADCH;
	result = result << 8;
	result = result | b;
	return result;
}

//ISR on Button0
//-----------------------------------------------------------//

ISR(INT0_vect)  //ISR: if you use delay use very small ones
{				//INT0: PD2, INT1: PD3, INT2: PB2
	PORTD ^= (1<<4); //LED0 toggle
	PORTD ^= (1<<5); //LED1 toggle
	PORTD ^= (1<<6); //LED2 toggle
	stop_watchflag = 1;
	_delay_ms(200);
}

ISR(INT1_vect)  //ISR: if you use delay use very small ones
{				//INT0: PD2, INT1: PD3, INT2: PB2
	stop_watchflag = 0;
	_delay_ms(200);
}

//-----------------------------------------------------------//

int main(void)
{

//Variables
//-----------------------------------------------------------//
	
	//int a = 999; //for counting on LCD
	unsigned int voltage, temp_holder; //for ADC on A5
	
//I/Os
//-----------------------------------------------------------//		
	
	DDRD |= (1<<4); //LED0: PD4
	DDRD |= (1<<5); //LED1: PD5
	DDRD |= (1<<6); //LED2: PD6
	
	DDRB |= (1<<1);
	
	//DDRD &= ~(1<<0); //BUTTON0: PD2 //not used /w interrupts?
	PORTD |= (1<<2); //PULL-UP: PD2, INT0
	
	//DDRD &= ~(1<<2); //BUTTON1: PD2 //not used /w interrupts?
	PORTD |= (1<<3); //PULL-UP: PD3, INT1
		
//LCD
//-----------------------------------------------------------//		
		
	//DDRB = 0xff;
	//lcdport = 0xff;
	DDRC = 0xff; // 1111 1111
	
	DDRD |= (1 << 0);
	DDRD |= (1 << 1);
	
	//DDRC |= (1 << 7);
	//DDRC |= (1 << 6);
	
	lcd_init();
	
	//lcd_cmd(0xC5);
	//lcd_data('W');
	//lcd_data('e');
	//lcd_data('l');
	//lcd_data(' ');
	//lcd_data('c');
	//lcd_data('o');
	//lcd_data('m');
	//lcd_data('e');
	
	
	lcd_out(1,1,"TuGab");
	//lcd_print(2,1,stop_watch,4);
	lcd_print(2,1,stop_minutes,2);
	lcd_data(46);
	lcd_print(2,4,stop_seconds,2);
	_delay_ms(100);
		
//Set-up Interrupts
//-----------------------------------------------------------//
	
//GISR: enable individual interrupt - INT1/INT0/INT2/X XXXX
	GICR |= (1<<6); //INT0 - 0100 0000
	GICR |= (1<<7); //INT0 - 1000 0000
	
	sei(); // globally enable interrupts
	
//MCUCR: MCU Control Register   
//XXXX INT1/INT1/INT0/INT0 (bits 3 and 2: INT1; bits 1 and 0: INT0)
//00: on low level, 01: on logic change, 10: on falling, 11: on rising
	MCUCR = 0x00; //0000 0000

	//cli(); //globally disable interrupts (e.g. after 10 cycles)
									
//-----------------------------------------------------------//		
				
    while (1) 
    {

//Read ADC on A5 every 2 sec
//-----------------------------------------------------------//

//for TMP35
		voltage = adc_read(5);
		voltage = voltage * 4.88;
		
		//lcd_print(1,10,voltage,4);
		//lcd_out(1,15,"mV");
		
		//voltage = voltage/10;
		temp_holder = voltage;
		
		
		voltage = temp_holder % 10;
		temp_holder = temp_holder/10;
		lcd_print(1,10,temp_holder,2);
		lcd_data(46);
		lcd_print(1,13,voltage,1);
			
		lcd_data(223);
		lcd_data('C');
		//lcd_out(2,15," C");

//Print temp
//-----------------------------------------------------------//
		lcd_out(2,10,"       "); _delay_ms(5);

		if (temp_holder >= 26){
			lcd_out(2,10,"Too hot");
			PORTB &= ~(1<<1);
		}
		else if (temp_holder >= 22){
			lcd_out(2,10,"Warm");
			PORTB &= ~(1<<1);
		}
		else if (temp_holder >= 20){
			lcd_out(2,10,"Mild");
			PORTB |= (1<<1);
		}
		else if (temp_holder >= 17){
			lcd_out(2,10,"Cold");
			PORTB |= (1<<1);
		}
		else{
			lcd_out(2,10,"Freezing");
			PORTB |= (1<<1);
		}

//Stopwatch
//-----------------------------------------------------------//
			
		if (stop_watchflag){
			if(stop_seconds<59){
				stop_seconds++;
			}
			else{
				stop_seconds=0;
				if(stop_minutes<59){
					stop_minutes++;
				}
				else{
					stop_minutes = 0;
					stop_hours++;
				}
			}
		}
		else{
			stop_hours = 0;
			stop_minutes=0;
			stop_seconds=0;
		}
		lcd_print(2,1,stop_hours,2);
		lcd_data(46);
		lcd_print(2,4,stop_minutes,2);
		lcd_data(46);
		lcd_print(2,7,stop_seconds,2);
		
//Stopwatch short
//-----------------------------------------------------------//

		//if (stop_watchflag){
			//stop_watch++;		
		//}
		//else{
			//stop_watch = 0;
		//}
		//
		//lcd_print(2,1,stop_watch,4);
		
//-----------------------------------------------------------//	
						
		_delay_ms(1000);
	
//-----------------------------------------------------------//

//original program
		//voltage = adc_read(5);
		//lcd_print(1,11,voltage,4);
		//voltage = voltage * 4.88;
		//lcd_print(2,11,voltage,4);
		//lcd_out(2,15,"mV");
		//_delay_ms(1000);

//Button0 press example (not used /w interrupts)
//-----------------------------------------------------------//

		//if(!(PIND & 0x04)){ //0000 0100
			//LED1_ON();
		//_delay_ms(500);
		//}
		//else{
			//LED1_OFF();
		//}
		
//Led blinking every 1 sec
//-----------------------------------------------------------//

		//LED0_ON();
		//a--;
		//lcd_print(2,1,a,4);
		//DELAY_1S();
		//LED0_OFF();
		//DELAY_1S();	
    }
}
