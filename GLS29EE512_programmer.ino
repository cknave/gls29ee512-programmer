// a7..a0 -> pin 29..22
#define PORT_ADDR_LOW PORTA
#define DDR_ADDR_LOW DDRA

// a15..a8 -> pin 30..37
#define PORT_ADDR_HIGH PORTC
#define DDR_ADDR_HIGH DDRC

// d7..d0 -> pin 42..49
#define PORT_DATA PORTL
#define DDR_DATA DDRL
#define PIN_DATA PINL

// _,_,_,_,_,we,ce,oe -> pin x,x,x,x,x,39,40,41
#define PORT_CTRL PORTG 
#define DDR_CTRL DDRG

#define OE_MASK (1<<0)
#define CE_MASK (1<<1)
#define WE_MASK (1<<2)

#define OUTPUT_ENABLED 0
#define OUTPUT_DISABLED OE_MASK
#define CHIP_ENABLED 0
#define CHIP_DISABLED CE_MASK
#define WRITE_ENABLED 0
#define WRITE_DISABLED WE_MASK


typedef struct {
  uint8_t manufacturer;
  uint8_t device;
} ProductId;

void setup() {
  // Set address pins to output zeros
  DDR_ADDR_LOW = 0xff;
  PORT_ADDR_LOW = 0x00;
  DDR_ADDR_HIGH = 0xff;
  PORT_ADDR_HIGH = 0x00;

  // Disable all chip functions (output high)
  DDR_CTRL = OE_MASK | CE_MASK | WE_MASK;
  PORT_CTRL = OUTPUT_DISABLED | CHIP_DISABLED | WRITE_DISABLED;

  // Set data to input
  DDR_DATA = 0x00;
  PORT_DATA = 0x00;

  Serial.begin(57600);
}

void loop() {
  char buf[128];

  while(true) {
  
  ProductId productId = getProductId();
  sprintf(buf, "Manufacturer: %02X, product: %02X", productId.manufacturer, productId.device);
  Serial.println(buf);
  delay(250);
  }

  while(true) { delay(1000); }

//  setDataMode(INPUT);
//  for(int row = 0; row < 64; row++) {
//    uint8_t bytes[16];
//    for(int col = 0; col < 16; col++) {
//      bytes[col] = readData(row*16 + col);
//    }
//    sprintf(buf, "%04X  %02X %02X %02X %02X %02X %02X %02X %02X   %02X %02X %02X %02X %02X %02X %02X %02X",
//            row*16,
//            bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
//            bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
//    Serial.println(buf);
//  }
//  
//  while(true) {delay(1000);}
}

ProductId getProductId() {
  PORT_CTRL = CHIP_ENABLED | OUTPUT_DISABLED | WRITE_DISABLED;
  // Data pins to output mode
  DDR_DATA = 0xff;
  // Enter software ID mode
  writeAddrData(0x5555, 0xaa);
  writeAddrData(0x2aaa, 0x55);
  writeAddrData(0x5555, 0x90);
  PORT_CTRL = CHIP_DISABLED | OUTPUT_DISABLED | WRITE_DISABLED;
  delayMicroseconds(10);

  // Data pins to input mode
  DDR_DATA = 0x00;
  PORT_CTRL = CHIP_ENABLED | OUTPUT_ENABLED | WRITE_DISABLED;
  // Read id bytes
  ProductId result;
  result.manufacturer = readAddr(0x0000);
  result.device = readAddr(0x0001);

  PORT_CTRL = CHIP_ENABLED | OUTPUT_DISABLED | WRITE_DISABLED;
  // Data pins to output mode
  DDR_DATA = 0xff;
  // Exit software ID mode
  writeAddrData(0x5555, 0xaa);
  writeAddrData(0x2aaa, 0x55);
  writeAddrData(0x5555, 0xf0);
  // Disable chip, output, and write
  delayMicroseconds(10);

  return result;
}

inline void writeAddrData(uint16_t address, uint8_t data) {
  PORT_ADDR_HIGH = address >> 8;
  PORT_ADDR_LOW = address & 0xff;
  PORT_DATA = data;
  // write pulse
  PORT_CTRL = OUTPUT_DISABLED | CHIP_ENABLED | WRITE_ENABLED;
  PORT_CTRL = OUTPUT_DISABLED | CHIP_ENABLED | WRITE_DISABLED;
}

inline uint8_t readAddr(uint16_t address) {
  PORT_ADDR_HIGH = address >> 8;
  PORT_ADDR_LOW = address & 0xff;
  delayMicroseconds(2);  // no idea why I have to wait so long...
  return PIN_DATA;
}
