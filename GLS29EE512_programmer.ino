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

char inputBuffer[128];

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

  Serial.begin(500000);

  ProductId productId = getProductId();
  char buf[32];
  sprintf(buf, "Manufacturer: %02X, product: %02X", productId.manufacturer, productId.device);
  Serial.println(buf);
  if(productId.manufacturer != 0xbf || productId.device != 0x5d) {
    while(true) {
      Serial.println("Expected BF/5D, cowardly refusing all further commands!");
      readSerialCommand(inputBuffer, sizeof(inputBuffer));
      Serial.println(buf);
    }
  }  
}

void loop() {
  Serial.print("ready>");
  readSerialCommand(inputBuffer, sizeof(inputBuffer));
  Serial.println();

  // Parse command
  int cmdEnd = findTokenEnd(inputBuffer, sizeof(inputBuffer), 0);
  if(cmdEnd == 0) {
    return;
  }
  if(!strncmp("help", inputBuffer, cmdEnd)) {
    printHelp();
    return;
  }
  if(!strncmp("write", inputBuffer, cmdEnd)) {
    int page;
    if(!parseNumberArgument(inputBuffer, sizeof(inputBuffer), cmdEnd, &page)) {
      return;
    }
    if(page < 0 || page >= 512) {
      Serial.println("Page must be between 0 and 511 inclusive");
      return;
    }
    Serial.print("INPUT 128 BYTES#");
    Serial.setTimeout(1000 * 60);  // 1 minute
    size_t bytesRead = Serial.readBytes(inputBuffer, 128);
    if(bytesRead != 128) {
      Serial.println("Timed out waiting for 128 bytes");
      return;
    }    
    writePage(128 * page, inputBuffer);
    Serial.println("OK");
    return;
  }
  if(!strncmp("verify", inputBuffer, cmdEnd)) {
    int page;
    if(!parseNumberArgument(inputBuffer, sizeof(inputBuffer), cmdEnd, &page)) {
      return;
    }
    Serial.print("INPUT 128 BYTES#");
    Serial.setTimeout(1000 * 60);  // 1 minute
    size_t bytesRead = Serial.readBytes(inputBuffer, 128);
    if(bytesRead != 128) {
      Serial.println("Timed out waiting for 128 bytes");
      return;
    }    
    if(verifyPage(128 * page, inputBuffer)) {
      Serial.println("OK");
    } else {
      Serial.println("FAIL");
    }
    return;
  }
  if(!strncmp("hexdump", inputBuffer, cmdEnd)) {
    hexDump();
    return;
  }
  Serial.println("Unrecognized command");
  return;
}

void hexDump() {
  delay(10);
  char buf[128];
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
  result.manufacturer = slowReadAddr(0x0000);
  result.device = slowReadAddr(0x0001);

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
  return PIN_DATA;
}

// Required for reading the device ID
inline uint8_t slowReadAddr(uint16_t address) {
  PORT_ADDR_HIGH = address >> 8;
  PORT_ADDR_LOW = address & 0xff;
  delayMicroseconds(3);
  return PIN_DATA;
}


inline void setDataWriteMode() {
  DDR_DATA = 0xff;
}

inline void setDataReadMode() {
  DDR_DATA = 0x00;
  PORT_DATA = 0x00;  // disable pull-up resistors
}

void readSerialCommand(char *buf, int size) {
  Serial.setTimeout(1000 * 3600);  // 1 hour

  // Loop until we read something
  size_t bytesRead = Serial.readBytesUntil('\n', buf, size - 1);
  // Add null termination, possibly overwriting a trailing carriage return
  if(bytesRead > 0 && buf[bytesRead-1] == '\r') {
    buf[bytesRead-1] = '\0';
  } else {
    buf[bytesRead] = '\0';
  }
}

int findTokenEnd(char *s, int size, int startAfter) {
  char *end = s + size;
  char *p = s;
  if(startAfter > 0) {
    if(s[startAfter] == '\0') {
      return startAfter;
    }
    p += startAfter + 1;
  }
  while(p != end && *p != '\0' && *p != ' ' && *p != '\n') {
    p++;
  }
  return p - s;
}

void printHelp() {
  Serial.println("GLS29EE512 programmer");
  Serial.println("Connect pins a7..a0 to PORTA7..0");
  Serial.println("Connect pins a15..a8 to PORTC7..0");
  Serial.println("Connect pins d7..d0 to PORTL7..0");
  Serial.println("Connect pins WE,CE,OE to PORTG2..0");
  Serial.println();
  Serial.println("Commands:");
  Serial.println("help - print this text");
  Serial.println("write <page> - write a 128 byte page where <page> is a number between 0 and 511");
  Serial.println("               inclusive; after the newline, write all 128 data bytes");
  Serial.println("verify <page> - verify a 128 byte page; after the newline send bytes to verify");
}

bool parseNumberArgument(char *s, int size, int startAfter, int *resultPtr) {
  int argEnd = findTokenEnd(s, size, startAfter);
  if(argEnd <= startAfter + 1) {
    Serial.println("Missing argument");
    return false;
  }
  char *arg = inputBuffer + startAfter + 1;
  inputBuffer[argEnd] = '\0';
  char *parseEnd = NULL;
  long int result = strtol(arg, &parseEnd, 10);
  if(parseEnd != inputBuffer + argEnd) {
    Serial.println("Invalid argument");
    return false;
  }
  *resultPtr = result;
  return true;
}

bool verifyPage(uint16_t baseAddr, uint8_t *expected) {
  setDataReadMode();
  PORT_CTRL = CHIP_ENABLED | OUTPUT_ENABLED | WRITE_DISABLED;
  for(int i = 0; i < 128; i++) {
    uint8_t value = readAddr(baseAddr + i);
    if(value != expected[i]) {
      return false;
    }
  }
  return true;
}
