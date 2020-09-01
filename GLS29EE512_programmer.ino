const int AddressPins[16] = {22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37};
const int DataPins[8] = {46, 47, 48, 49, 50, 51, 52, 53};
const int WriteEnablePin = 21;
const int OutputEnablePin = 20;
const int ChipEnablePin = 19;

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
  pinMode(OutputEnablePin, OUTPUT);
  pinMode(ChipEnablePin, OUTPUT);

  Serial.begin(57600);
}

void loop() {
  char buf[128];
  ProductId productId = getProductId();
  sprintf(buf, "Manufacturer: %02X, product: %02X", productId.manufacturer, productId.device);
  Serial.println(buf);

  while(true) { delay(1000); }
   
    
  digitalWrite(ChipEnablePin, LOW);
  digitalWrite(OutputEnablePin, LOW);
  digitalWrite(WriteEnablePin, HIGH);
  setDataMode(INPUT);

  
  for(int i = 0; i < 32; i++) {
    writeAddress(i);
    uint8_t data = readData();
    sprintf(buf, "%04X  %02X", 0, data);
    Serial.println(buf);
  }
  
  while(true) {delay(1000);}
}

ProductId getProductId() {
  digitalWrite(ChipEnablePin, LOW);
  digitalWrite(OutputEnablePin, HIGH);
  digitalWrite(WriteEnablePin, LOW);
  setDataMode(OUTPUT);
  writeAddress(0x5555);
  writeData(0xaa);
  writeAddress(0x2aaa);
  writeData(0x55);
  writeAddress(0x5555);
  writeData(0x90);
  delayMicroseconds(10);

  ProductId result;
  setDataMode(INPUT);
  writeAddress(0x0000);
  result.manufacturer = readData();
  writeAddress(0x0001);
  result.device = readData();
  return result;
}

void setDataMode(int mode) {
  for(int i = 0; i < 8; i++) {
    pinMode(DataPins[i], mode);
  }  
}

void writeAddress(int address) {
  for(int i = 0; i < 16; i++) {
    bool bitSet = (address & (1 << i)) != 0;
    digitalWrite(AddressPins[i], bitSet ? HIGH : LOW);
  }
}

uint8_t readData() {
  uint8_t result = 0;
  for(int i = 0; i < 8; i++) {
    int bitValue = digitalRead(DataPins[i]) == HIGH ? 1 : 0;
    result |= bitValue << i;
  }
  return result;
}

void writeData(uint8_t data) {
  for(int i = 0; i < 8; i++) {
    bool bitSet = (data & (1 << i)) != 0;
    digitalWrite(DataPins[i], bitSet ? HIGH : LOW);
  }
}
