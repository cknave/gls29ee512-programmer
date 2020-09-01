const int AddressPins[16] = {22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37};
const int DataPins[8] = {46, 47, 48, 49, 50, 51, 52, 53};
const int WriteEnablePin = 21;
const int OutputEnablePin = 19;
const int ChipEnablePin = 20;

typedef struct {
  uint8_t manufacturer;
  uint8_t device;
} ProductId;

void setup() {
  for(int i = 0; i < 16; i++) {
    pinMode(AddressPins[i], OUTPUT);
  }
  setDataMode(INPUT);
  pinMode(WriteEnablePin, OUTPUT);
  digitalWrite(WriteEnablePin, HIGH);  
  pinMode(OutputEnablePin, OUTPUT);
  digitalWrite(OutputEnablePin, HIGH);
  pinMode(ChipEnablePin, OUTPUT);
  digitalWrite(ChipEnablePin, HIGH);

  Serial.begin(57600);
}

void loop() {
  char buf[128];
  ProductId productId = getProductId();
  sprintf(buf, "Manufacturer: %02X, product: %02X", productId.manufacturer, productId.device);
  Serial.println(buf);

  setDataMode(INPUT);
  for(int row = 0; row < 64; row++) {
    uint8_t bytes[16];
    for(int col = 0; col < 16; col++) {
      bytes[col] = readData(row*16 + col);
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
  // Enter software ID mode
  setDataMode(OUTPUT);
  writeData(0x5555, 0xaa);
  writeData(0x2aaa, 0x55);
  writeData(0x5555, 0x90);  
  digitalWrite(ChipEnablePin, HIGH);
  digitalWrite(OutputEnablePin, HIGH);
  digitalWrite(WriteEnablePin, HIGH);
  delayMicroseconds(10);

  // Read id
  ProductId result;
  setDataMode(INPUT);
  result.manufacturer = readData(0x0000);
  result.device = readData(0x0001);

  // Exit software ID mode
  setDataMode(OUTPUT);
  writeData(0x5555, 0xaa);
  writeData(0x2aaa, 0x55);
  writeData(0x5555, 0xf0);
  digitalWrite(ChipEnablePin, HIGH);
  digitalWrite(OutputEnablePin, HIGH);
  digitalWrite(WriteEnablePin, HIGH);
  delayMicroseconds(10);

  return result;
}

void writeData(uint16_t address, uint8_t data) {
  digitalWrite(ChipEnablePin, HIGH);
  digitalWrite(OutputEnablePin, HIGH);
  digitalWrite(WriteEnablePin, HIGH);
  
  setAddressLines(address);
  setDataLines(data);
  // write pulse
  digitalWrite(ChipEnablePin, LOW);
  digitalWrite(WriteEnablePin, LOW);
  delayMicroseconds(1);
  digitalWrite(WriteEnablePin, HIGH);
}

void setDataMode(int mode) {
  for(int i = 0; i < 8; i++) {
    pinMode(DataPins[i], mode);
  }  
}

void setAddressLines(uint16_t address) {
  for(int i = 0; i < 16; i++) {
    bool bitSet = (address & (1 << i)) != 0;
    digitalWrite(AddressPins[i], bitSet ? HIGH : LOW);
//    char buf[128];
//    sprintf(buf, "[W] a%d = %d", i, bitSet);
//    Serial.println(buf);
  }
}

uint8_t readData(uint16_t address) {
  setAddressLines(address);
  digitalWrite(ChipEnablePin, LOW);
  digitalWrite(OutputEnablePin, LOW);
  digitalWrite(WriteEnablePin, HIGH);
  delayMicroseconds(1);
  uint8_t result = 0;
  for(int i = 0; i < 8; i++) {
    int bitValue = digitalRead(DataPins[i]) == HIGH ? 1 : 0;
    result |= bitValue << i;
//    char buf[128];
//    sprintf(buf, "[R] d%d = %d", i, bitValue);
//    Serial.println(buf);    
  }
  return result;
}

void setDataLines(uint8_t data) {
  for(int i = 0; i < 8; i++) {
    bool bitSet = (data & (1 << i)) != 0;
    digitalWrite(DataPins[i], bitSet ? HIGH : LOW);
//    char buf[128];
//    sprintf(buf, "[W] d%d = %d", i, bitSet);
//    Serial.println(buf);
  }
}
