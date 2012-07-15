#include <arch/antares.h>
#include <avr/boot.h>
#include <generated/usbconfig.h>
#include <arch/vusb/usbportability.h>
#include <arch/vusb/usbdrv.h>
#include <compat/deprecated.h>


#define USBRQ_TYPE_MASK 0x60

#define CONFIG_SERVO_COUNT 16

static uint16_t servo_nextpos[CONFIG_SERVO_COUNT];
static uint16_t servo_pos[CONFIG_SERVO_COUNT];


/* TODO: We support 12 16 18 and 20 Mhz crystal */
/* 12M is quite junky and unusable */



#if F_CPU == 20000000
static uint16_t next_icr=50000;
#endif


#if F_CPU == 18000000
static uint16_t next_icr=45000;
#endif


#if F_CPU == 16000000
static uint16_t next_icr=40000;
#endif

#if F_CPU == 12000000
static uint16_t next_icr=30000;
#endif

char r;

ISR(TIMER1_OVF_vect)
{
	r=1;
	ICR1=next_icr;
}

inline void flip()
{
	/* load up next values, we're phase correct. Somewhat */
	int i;
	for (i=0; i<CONFIG_SERVO_COUNT; i++)
	{
		servo_pos[i]=servo_nextpos[i];
	}
	/* Iterate over enabled channels */
	for (i=0;i<8;i++)
	{
		if (DDRB & (1<<i)) PORTB |= (1<<i);
	}
	for (i=0;i<8;i++)
	{
		if (DDRD & (1<<i)) PORTD |= (1<<i);
	}	
	r=0;
}


inline void loop_outputs()
{
	int i;
	int n;
	
	for (i=0;i<8;i++)
	{
		if (TCNT1 > servo_pos[i])
		{
			if (DDRB & (1<<i)) PORTB &= ~(1<<i);
		}
	}
	
	for (i=8;i<15;i++)
	{
		n=i-8;
		if (TCNT1 > servo_pos[i])
		{
			if (DDRD & (1<<n)) PORTD &= ~(1<<n);
		}
	}
	if (r) flip();
	
}

//If 0 - pin goes Z, else OUTPUT
void set_servo_power(int servo, char power)
{
	if (servo<8)
	{
		if (power)
		{
			DDRB |= (1<<servo);
			PORTB &= ~(1<<servo);
		}
		else
			DDRB &= ~(1<<servo);
	}else
	{
		servo-=8;
		if (power)
		{
			DDRD |= (1<<servo);
			PORTD &= ~(1<<servo);
		}
		else
			DDRD &= ~(1<<servo);
	}
	//PORTB^=1;
}

/* 
typedef struct usbRequest{
    uchar       bmRequestType;
    uchar       bRequest; //servo number
    usbWord_t   wValue; //pwm value
    usbWord_t   wIndex; //0 - disable, 1 enable
    usbWord_t   wLength;
}usbRequest_t;
*/

uchar   usbFunctionSetup(uchar data[8])
{
	usbRequest_t *rq =  (usbRequest_t*) &data[0];
	cli();
	if (rq->bRequest > 16)
	{
		next_icr=rq->wValue.word;
	}else
	{
		set_servo_power((int) rq->bRequest,rq->wIndex.bytes[0]);
		servo_nextpos[(int) rq->bRequest] = rq->wValue.word;
	}
	sei();
	return 0;
}

inline void usbReconnect()
{
	DDRD=0xff;
	PORTD=0x00;
	_delay_ms(CONFIG_START_DELAY);
        DDRD=0x0;
}

ANTARES_INIT_LOW(init)
{
	usbReconnect();
	TCCR1A = (1<<WGM11);
	TCCR1B = (1<<WGM13|1<<WGM12|1 <<CS11);
	TIMSK = (1<<TOIE1);
	ICR1=next_icr; //20ms
	DDRB=0x00;
	DDRD=0x00;
	usbInit();
}


ANTARES_APP(main_app)
{
		usbPoll();
		loop_outputs();
}
