// SID pins:
#define ADR0  2
#define ADR1  3
#define ADR2  4
#define ADR3  5
#define ADR4  6
#define PHI2 10
#define SICS 12
#define SRES 13

// 595 Shift Register pins:
#define DSER  7
#define STCP  8
#define SHCP  9
#define SHOE 11

void sid_clock_init() {
  noInterrupts();
  // Zero all the required bits for using Timer 1 as Phi2:
  TCCR1A &= ~((1 << COM1B1) | (1 << COM1B0) | (1 << WGM11) | (1 << WGM10));
  TCCR1B &= ~((1 << WGM13) | (1 << WGM12) | (1 << CS12) | (1 << CS11) | (1 << CS10));

  /* Toggle Pin 10 on Compare Match, CTC Mode, 1 MHz:
   *
   *           CLOCK                16,000,000
   * -------------------------- = --------------- = 1 MHz
   * 2 * PRESCALE * (1 + OCR1A)   2 * 1 * (1 + 7) 
   */

  TCCR1A |= (0 << COM1B1) | (1 << COM1B0);            // Toggle Pin 10 on Compare Match
  TCCR1A |= (0 << WGM11) | (0 << WGM10);
  TCCR1B |= (0 << WGM13) | (1 << WGM12);              // CTC Mode
  TCCR1B |= (0 << CS12) | (0 << CS11) | (1 << CS10);  // Prescaler = 1
  OCR1A   = 7;                                        // Toggle after 7 (+1) counts
  interrupts();
}

void nopDelay()
{
  for (byte i = 0; i < 10; ++i) __asm("nop\n");
}

void sid_write_byte(byte addr, byte data)
{
  // Set address/SID register:
  digitalWrite(ADR0, (addr &  0x1));
  digitalWrite(ADR1, (addr &  0x2) >> 1);
  digitalWrite(ADR2, (addr &  0x4) >> 2);
  digitalWrite(ADR3, (addr &  0x8) >> 3);
  digitalWrite(ADR4, (addr & 0x10) >> 4);

  // Send data byte to shift register:
  digitalWrite(STCP, LOW);
  shiftOut(DSER, SHCP, MSBFIRST, data);
  digitalWrite(STCP, HIGH);
  digitalWrite(SHOE, LOW);
  nopDelay();

  // Initiate write operation:
  digitalWrite(SICS, LOW);
  nopDelay();

  // Finish write operation:
  digitalWrite(SICS, HIGH);
  digitalWrite(SHOE, HIGH);
  nopDelay();
}

#define TRI 0x1
#define SAW 0x2
#define SQU 0x4
#define NOI 0x8

#define LPF 0x1
#define BPF 0x2
#define HPF 0x4
#define NCF (LPF | HPF)

struct sid_settings {
  byte fmode;
  byte volume;
};

struct osc_settings {
  byte waveform;
  byte filter;
};

static struct sid_settings sid = {
  0,
  0
};

static struct osc_settings osc[3] = {
  { SQU, 1 },
  { SQU, 0 },
  { SQU, 0 }
};

void sid_volume(byte vol)
{
  sid.volume = (vol & 0xf);
  sid_write_byte(24, (sid.fmode << 4) | sid.volume);
}

void sid_filter(unsigned cutoff, byte reso)
{
  sid_write_byte(21, cutoff & 0x7);
  sid_write_byte(22, (cutoff & 0x7f8) >> 3);
  sid_write_byte(23, ((reso & 0xf) << 4) | (osc[2].filter << 2) | (osc[1].filter << 1) | osc[0].filter);
}

void sid_filter_mode(byte mode)
{
  sid.fmode = (mode & 0x7);
  sid_write_byte(24, (sid.fmode << 4) | sid.volume);
}

void sid_adsr(byte att, byte dec, byte sus, byte rel, byte vox = 0)
{
  byte vos = vox * 7;
  sid_write_byte(vos + 5, ((att & 0x0f) << 4) | (dec & 0x0f));
  sid_write_byte(vos + 6, ((sus & 0x0f) << 4) | (rel & 0x0f));
}

void sid_note(unsigned freq, byte vox = 0)
{
  byte vos = vox * 7;
  sid_write_byte(vos, (freq & 0xff));
  sid_write_byte(vos + 1, (freq >> 8));
  sid_write_byte(vos + 4, (osc[vox].waveform << 4) | 1);
}

void sid_stop(byte vox)
{
  byte vos = vox * 7;
  sid_write_byte(vos + 4, (osc[vox].waveform << 4) | 0);
}

void sid_stop_all()
{
  for (byte i = 0; i < 3; ++i)
    sid_stop(i);
}

void setup()
{
  // Set all digital pins as OUTPUTs:
  for (byte i = 2; i <= 13; ++i)
    pinMode(i, OUTPUT);

  // Initial control pin settings
  digitalWrite(SICS, HIGH);
  digitalWrite(SHOE, HIGH);
  digitalWrite(STCP, HIGH);

  sid_clock_init();
  digitalWrite(SRES, HIGH);

  // SID sound settings
  sid_stop_all();
  sid_volume(15);
  sid_adsr(2, 4, 10, 4);
  delay(1000);
}

void loop()
{
  static bool res = true;
  static byte mode = LPF;
  sid_filter_mode(mode);
  sid_filter(0x3ff, (res ? 15 : 0));
  sid_note(2000);
  delay(500);
  sid_stop(0);
  sid_filter(0x1ff, (res ? 15 : 0));
  sid_note(1000);
  delay(500);
  sid_stop(0);
  sid_filter(0xff, (res ? 15 : 0));
  sid_note(3000);
  delay(500);
  sid_stop(0);
  delay(500);

  res = !res;
  switch (mode) {
    case LPF:
      mode = BPF;
      break;
    case BPF:
      mode = HPF;
      break;
    case HPF:
      mode = NCF;
      break;
    case NCF:
      mode = LPF;
      break;
  }
}
