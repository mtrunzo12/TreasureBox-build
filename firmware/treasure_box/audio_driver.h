#pragma once
#include <Arduino.h>

// One-pin, variable-duty audio on Uno D11/PB3. Timer0 (millis) and Timer1
// (Servo) remain untouched. Never link Arduino tone(): it also owns Timer2.
struct AudioPlan { uint16_t divisor, period, high; uint8_t clockBits; uint32_t cycles; };
inline AudioPlan audioPlan(uint16_t frequency, uint32_t duration, uint8_t duty) {
  AudioPlan p={0,0,0,0,0};
  if(frequency<80 || frequency>4000 || duration==0) return p;
  if(duty<10) duty=10;
  if(duty>50) duty=50; // no more on-time than the old full-volume square wave
  const uint16_t divisors[]={1,8,32,64,128,256,1024};
  for(uint8_t i=0;i<7;i++) {
    uint32_t denominator=(uint32_t)divisors[i]*frequency;
    uint32_t period=(16000000UL+denominator/2)/denominator;
    if(period<=256 && period>=20) {
      p.divisor=divisors[i]; p.period=period; p.clockBits=i+1;
      p.high=(uint32_t)p.period*duty/100;
      // Compute the stop count from the realized timer frequency.
      uint32_t tickCycles=(uint32_t)p.divisor*p.period;
      p.cycles=((uint64_t)duration*16000+tickCycles-1)/tickCycles;
      break;
    }
  }
  return p;
}

#if defined(__AVR_ATmega328P__) || defined(TREASURE_BOX_TIMER_TEST)
#include <avr/interrupt.h>
volatile uint32_t audioCycles=0;
inline void audioStop() {
  uint8_t oldSREG=SREG; cli();
  TIMSK2=0; TCCR2B=0; TCCR2A=0; audioCycles=0;
  PORTB &= ~(1<<PB3);
  SREG=oldSREG;
}
inline void audioStart(uint16_t frequency,uint32_t duration,uint8_t duty=50) {
  AudioPlan p=audioPlan(frequency,duration,duty);
  uint8_t oldSREG=SREG; cli();
  TIMSK2=0; TCCR2B=0; TCCR2A=0; PORTB &= ~(1<<PB3);
  if(!p.period) { audioCycles=0; SREG=oldSREG; return; }
  TCNT2=0; OCR2A=p.period-1; OCR2B=p.high-1;
  audioCycles=p.cycles;
  TIFR2=(1<<OCF2A)|(1<<OCF2B)|(1<<TOV2); // discard stale pending matches
  TCCR2A=(1<<WGM21); // CTC; both compare outputs disconnected from hardware pins
  PORTB |= (1<<PB3);
  TIMSK2=(1<<OCIE2A)|(1<<OCIE2B);
  TCCR2B=p.clockBits;
  SREG=oldSREG;
}
ISR(TIMER2_COMPB_vect) { PORTB &= ~(1<<PB3); }
ISR(TIMER2_COMPA_vect) {
  if(audioCycles && --audioCycles) PORTB |= (1<<PB3);
  else { PORTB &= ~(1<<PB3); TIMSK2=0; TCCR2B=0; }
}
#elif defined(TREASURE_BOX_TEST)
inline void audioStop() { noTone(11); }
inline void audioStart(uint16_t frequency,uint32_t duration,uint8_t duty=50) {
  AudioPlan p=audioPlan(frequency,duration,duty);
  if(!p.period) { audioStop(); return; }
  tone(11,frequency,duration);
  toneDuties.push_back(duty);
}
#else
#error This audio driver requires an ATmega328P at 16 MHz.
#endif
