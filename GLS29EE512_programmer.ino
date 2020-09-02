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
  setDataReadMode();

  Serial.begin(57600);
}

void loop() {
  char buf[128];

  ProductId productId = getProductId();
  sprintf(buf, "Manufacturer: %02X, product: %02X", productId.manufacturer, productId.device);
  Serial.println(buf);
  if(productId.manufacturer != 0xbf || productId.device != 0x5d) {
    Serial.println("Expected BF/5D, cowardly refusing all further commands!");
    while(true) { delay(1000); }
  }

  char data[128];
  for(int i = 0; i < 128; i++) {
    data[i] = 128 + i;
  }
  Serial.println("Writing page 1");
  writePage(128, data);

  setDataReadMode();
  PORT_CTRL = CHIP_ENABLED | OUTPUT_ENABLED | WRITE_DISABLED;
  for(int row = 0; row < 64; row++) {
    uint8_t bytes[16];
    for(int col = 0; col < 16; col++) {
      bytes[col] = readAddr(row*16 + col);
    }
    sprintf(buf, "%04X  %02X %02X %02X %02X %02X %02X %02X %02X   %02X %02X %02X %02X %02X %02X %02X %02X",
            row*16,
            bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
            bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    Serial.println(buf);
  }
  
  while(true) {delay(1000);}
}

ProductId getProductId() {
  PORT_CTRL = CHIP_ENABLED | OUTPUT_DISABLED | WRITE_DISABLED;
  setDataWriteMode();
  // Enter software ID mode
  writeAddrData(0x5555, 0xaa);
  writeAddrData(0x2aaa, 0x55);
  writeAddrData(0x5555, 0x90);
  PORT_CTRL = CHIP_DISABLED | OUTPUT_DISABLED | WRITE_DISABLED;
  delayMicroseconds(10);

  setDataReadMode();
  PORT_CTRL = CHIP_ENABLED | OUTPUT_ENABLED | WRITE_DISABLED;
  // Read id bytes
  ProductId result;
  result.manufacturer = readAddr(0x0000);
  result.device = readAddr(0x0001);

  PORT_CTRL = CHIP_ENABLED | OUTPUT_DISABLED | WRITE_DISABLED;
  setDataWriteMode();
  // Exit software ID mode
  writeAddrData(0x5555, 0xaa);
  writeAddrData(0x2aaa, 0x55);
  writeAddrData(0x5555, 0xf0);
  delayMicroseconds(10);

  return result;
}

// Write 128 bytes of data
void writePage(uint16_t baseAddr, uint8_t *data) {
  PORT_CTRL = CHIP_ENABLED | OUTPUT_DISABLED | WRITE_DISABLED;
  setDataWriteMode();
  // Enter software data protection
  writeAddrData(0x5555, 0xaa);
  writeAddrData(0x2aaa, 0x55);
  writeAddrData(0x5555, 0xa0);
  PORT_CTRL = CHIP_ENABLED | OUTPUT_DISABLED | WRITE_DISABLED;

  // Set high bytes of address
  PORT_ADDR_HIGH = baseAddr >> 8;
  
  // Write 128 bytes starting at the base address
  uint8_t addrLow = baseAddr & 0xff;
  for(int i = 0; i < 128; i++) {
    PORT_ADDR_LOW = addrLow + i;
    PORT_DATA = data[i];
    writePulse();
  }

  // Wait for write to complete
  delayMicroseconds(200);

  // Poll for write completed: high data bit should be equal to the high
  // bit in the last byte written)
  const uint8_t highBit = 1 << 7;
  uint8_t expected = data[127] & highBit;
  setDataReadMode();
  uint8_t current;
  do {
    PORT_CTRL = CHIP_ENABLED | OUTPUT_ENABLED | WRITE_DISABLED;
    current = PIN_DATA & highBit;
    PORT_CTRL = CHIP_DISABLED | OUTPUT_DISABLED | WRITE_DISABLED;
  } while(current != expected);
}

inline void writeAddrData(uint16_t address, uint8_t data) {
  PORT_ADDR_HIGH = address >> 8;
  PORT_ADDR_LOW = address & 0xff;
  PORT_DATA = data;
  writePulse();
}

// Pulse the WE line
inline void writePulse() {
  PORT_CTRL = OUTPUT_DISABLED | CHIP_ENABLED | WRITE_ENABLED;
  PORT_CTRL = OUTPUT_DISABLED | CHIP_ENABLED | WRITE_DISABLED;
}

inline uint8_t readAddr(uint16_t address) {
  PORT_ADDR_HIGH = address >> 8;
  PORT_ADDR_LOW = address & 0xff;
  delayMicroseconds(2);  // no idea why I have to wait so long...
  return PIN_DATA;
}

inline void setDataWriteMode() {
  DDR_DATA = 0xff;
}

inline void setDataReadMode() {
  DDR_DATA = 0x00;
  PORT_DATA = 0x00;  // disable pull-up resistors
}
