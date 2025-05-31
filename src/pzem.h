void preTransmission() /* transmission program when triggered*/
{
    /* 1- PZEM-017 DC Energy Meter */
    if (millis() - startMillis1 > 5000) // Wait for 5 seconds as ESP Serial cause start up code crash
    {
        digitalWrite(MAX485_RE, 1); /* put RE Pin to high*/
        // digitalWrite(MAX485_DE, 1); /* put DE Pin to high*/
        delay(1); // When both RE and DE Pin are high, converter is allow to transmit communication
    }
}

void postTransmission() /* Reception program when triggered*/
{

    /* 1- PZEM-017 DC Energy Meter */
    if (millis() - startMillis1 > 5000) // Wait for 5 seconds as ESP Serial cause start up code crash
    {
        delay(3);                   // When both RE and DE Pin are low, converter is allow to receive communication
        digitalWrite(MAX485_RE, 0); /* put RE Pin to low*/
                                    // digitalWrite(MAX485_DE, 0); /* put DE Pin to low*/
    }
}

void setShunt(uint8_t slaveAddr) // Change the slave address of a node
{

    /* 1- PZEM-017 DC Energy Meter */

    static uint8_t SlaveParameter = 0x06;     /* Write command code to PZEM */
    static uint16_t registerAddress = 0x0003; /* change shunt register address command code */

    uint16_t u16CRC = 0xFFFF;                 /* declare CRC check 16 bits*/
    u16CRC = crc16_update(u16CRC, slaveAddr); // Calculate the crc16 over the 6bytes to be send
    u16CRC = crc16_update(u16CRC, SlaveParameter);
    u16CRC = crc16_update(u16CRC, highByte(registerAddress));
    u16CRC = crc16_update(u16CRC, lowByte(registerAddress));
    u16CRC = crc16_update(u16CRC, highByte(NewshuntAddr));
    u16CRC = crc16_update(u16CRC, lowByte(NewshuntAddr));

    preTransmission(); /* trigger transmission mode*/

    PZEMSerial.write(slaveAddr); /* these whole process code sequence refer to manual*/
    PZEMSerial.write(SlaveParameter);
    PZEMSerial.write(highByte(registerAddress));
    PZEMSerial.write(lowByte(registerAddress));
    PZEMSerial.write(highByte(NewshuntAddr));
    PZEMSerial.write(lowByte(NewshuntAddr));
    PZEMSerial.write(lowByte(u16CRC));
    PZEMSerial.write(highByte(u16CRC));
    delay(10);
    postTransmission(); /* trigger reception mode*/
    delay(100);
}

void resetEnergy() // reset energy for Meter 1
{
    uint16_t u16CRC = 0xFFFF;           /* declare CRC check 16 bits*/
    static uint8_t resetCommand = 0x42; /* reset command code*/
    uint8_t slaveAddr = pzemSlaveAddr;  // if you set different address, make sure this slaveAddr must change also
    u16CRC = crc16_update(u16CRC, slaveAddr);
    u16CRC = crc16_update(u16CRC, resetCommand);
    preTransmission();                  /* trigger transmission mode*/
    PZEMSerial.write(slaveAddr);        /* send device address in 8 bit*/
    PZEMSerial.write(resetCommand);     /* send reset command */
    PZEMSerial.write(lowByte(u16CRC));  /* send CRC check code low byte  (1st part) */
    PZEMSerial.write(highByte(u16CRC)); /* send CRC check code high byte (2nd part) */
    delay(10);
    postTransmission(); /* trigger reception mode*/
    delay(100);
}

void changeAddress(uint8_t OldslaveAddr, uint8_t NewslaveAddr) // Change the slave address of a node
{

    /* 1- PZEM-017 DC Energy Meter */

    static uint8_t SlaveParameter = 0x06;        /* Write command code to PZEM */
    static uint16_t registerAddress = 0x0002;    /* Modbus RTU device address command code */
    uint16_t u16CRC = 0xFFFF;                    /* declare CRC check 16 bits*/
    u16CRC = crc16_update(u16CRC, OldslaveAddr); // Calculate the crc16 over the 6bytes to be send
    u16CRC = crc16_update(u16CRC, SlaveParameter);
    u16CRC = crc16_update(u16CRC, highByte(registerAddress));
    u16CRC = crc16_update(u16CRC, lowByte(registerAddress));
    u16CRC = crc16_update(u16CRC, highByte(NewslaveAddr));
    u16CRC = crc16_update(u16CRC, lowByte(NewslaveAddr));
    preTransmission();              /* trigger transmission mode*/
    PZEMSerial.write(OldslaveAddr); /* these whole process code sequence refer to manual*/
    PZEMSerial.write(SlaveParameter);
    PZEMSerial.write(highByte(registerAddress));
    PZEMSerial.write(lowByte(registerAddress));
    PZEMSerial.write(highByte(NewslaveAddr));
    PZEMSerial.write(lowByte(NewslaveAddr));
    PZEMSerial.write(lowByte(u16CRC));
    PZEMSerial.write(highByte(u16CRC));
    delay(10);
    postTransmission(); /* trigger reception mode*/
    delay(100);
}
void pzem_begin()
{
    startMillis1 = millis();
    PZEMSerial.begin(9600); // software serial arduino MAX485
    startMillisPZEM = millis(); /* Start counting time for run code */
    pinMode(MAX485_RE, OUTPUT); /* Define RE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/
    digitalWrite(MAX485_RE, 0); /* Arduino create output signal for pin RE as LOW (no output)*/
    // digitalWrite(MAX485_DE, 0); /* Arduino create output signal for pin DE as LOW (no output)*/

    node.preTransmission(preTransmission); // Callbacks allow us to configure the RS485 transceiver correctly
    node.postTransmission(postTransmission);
    node.begin(pzemSlaveAddr, PZEMSerial);
    delay(1000); /* after everything done, wait for 1 second */

    while (millis() - startMillis1 < 5000)
    {
        delay(500);
        Serial.print(".");
    }

    setShunt(pzemSlaveAddr); // shunt addr
}