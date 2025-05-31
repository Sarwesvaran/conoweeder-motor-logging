#include <Arduino.h>
#include "Wire.h"
#include <SoftwareSerial.h>
#include <ModbusMaster.h>

#define MAX485_RO 16
#define MAX485_RE 18
// #define MAX485_DE  D5
#define MAX485_DI 17
#define HALL_SENSOR_PIN 25

// Google Project ID datacorpindiaeng@gmail.com
#define PROJECT_ID "motor-monitor-1"

// Google Project ID conoweederdc@gmail.com
// #define PROJECT_ID "conoweeder-motor-logger"

// Service Account's client email datacorpindiaeng@gmail.com
#define CLIENT_EMAIL "motor-monitor-1@motor-monitor-1.iam.gserviceaccount.com"

// Service Account's client email conoweederdc@gmail.com
// #define CLIENT_EMAIL "conoweeder-datalogging@conoweeder-motor-logger.iam.gserviceaccount.com"

// Service Account's private key datacorpindiaeng@gmail.com
const char PRIVATE_KEY[] PROGMEM = "-----BEGIN PRIVATE KEY-----\nMIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDR50jVwS1+Q5TX\nLT9J2CN3hgnP6FlmPChaPWk+ainYnMrAG7LhLVOwkYQgRkeZlIVFykyfz4MwBqwQ\nqpEj3tbyArgiC++/JRAR9HheYiig9P7qZyQAVsCgP+1X1TtzvEQqgxn4NmFeELtp\nLnKeUM3WiuTiiQ8YJb6GuZkInxAR0BP84R5dtHEduiQzvMJ7s6dU8ygXqZisy0Bv\nxUyaWEDNXNme9o+3UGQbfJYFue8NmdyPBakq4uiYzKoinC9NLa2Z6WnuobdFtO5s\nuVbeA8IOerLHWG2zI2aIuke/ZRMmEtSBYE8f9EznP96sQ20mJjwJMmQNyQdnosL1\nlvd+XwLdAgMBAAECggEADSLp4oLH+5Z0KrFIvDDcK19Gnu3eJnFOXzNnskHHo6hQ\nYgx5ufDxw2Of/rsXQQvT+WWrPPcqMSfBvftGwRVxo0y7RFo142MI6e7AV7vR6hzb\nFUVmMeU8AetuSahtI5WdOIHi7q9Mkav/ZppDDxhkgXBI2cqVGMBH97uG0czOnyan\nAwnCvTl2M4QKm6/pf7bM3o1Tn+JvEIVCaoupADMOvYfEP2PjhLf2Og6wNlARpH4A\nHDejB3oEE8R4WvlPotn6J+1iB5VMCUavmglH2Ae9CWRSqyZsAYQonjREsdgI1MHP\nkEQyDea6goxz73Zb7YE4y6HPVPzCuNQ65n6AVyZWAQKBgQD0l5VlcGtT9tycpIFd\nANoz2Evvc5RNXGMUG92WFI33Yg4hC+HeqZEpqOP+5ueKAM6b9Je3rNsvk5D5DbvM\nqtSYkD9WSPrl542InAVlGUB2cYJcScfYWwLGYyHR7Vi1IYPVGpd8rqftrDdCcy0U\n3TleGMsOG6xuqymCqf7ND6vnAQKBgQDbsYUovxXH/IA1p7qmwmQSgFrQFKWqgGp8\ni3Nlw8wJx8bS14r52wD8t4JCe+XnDyApxsq9pgk/omzmM4Mw6NcJnx4UdeoAXNRM\nVegJx4kGkp4khyvVexH7CIxDoYFi/i7rTHW54DG5z3Yu0A29AHtgJfs7rmJlZLAx\nvQLAcLeX3QKBgFpMeCA6D7Ww5qOB7CxsuyH20Zn2XqLs37s1ymm07vCMp9C1dU7h\n5TCvJHUfxA1j3OMgur5Zt5Xp8dSwqEuYKeKlJmzxhodIJC1yBF/dobnYkBsINhp/\nBxg80uiQqnY8iFItfg3O4TpSoRgMM3GHdr1xg82Sk+dLk4ikv+fuyDUBAoGBALQN\nueS7N07AUxOmdANYLkw7hjIjueMTfDK218z/u6oIYZtLrEs6goScodV7VeEZNHMI\nygDEo6/TnwzMCyl6q0Lnde+u9Cl360bk4VeigVsxrwqg/fvo8cOcLdj/9Nr9F9cg\nwxuj8x0mOuYC/j94taUHe/Bd66bN7tnU3vx5ZP7BAoGADSUU9+4aEsdxMx/Wb7AP\njRNXiaCeF9scPhu3xQg5VYIvM+61LhdDVSbPmNtliRI0UZguaKClg3g7bSD/osB6\npwYCj8UgGbuTN9W+1HJ1Fy0jj7hBbltR9yEGk91T2ZrqogrrtdT66lYKDHTxPEk6\njXhUXIr48TrqZMWpmTIb9xc=\n-----END PRIVATE KEY-----\n";

// Service Account's private key conoweederdc@gmail.com
// const char PRIVATE_KEY[] PROGMEM = "-----BEGIN PRIVATE KEY-----\nMIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQC5jrMHgS9GlXsq\nBF4K5glbnL1z1cz08j1qoUsTqIvbIs3Vyt/Dn4Yu4bsSNFMAtPD+n3+KHmTLRHaM\nFbWQb1PAPjyv5m6UXB9Hhb2P8LpBvjRCg4jvg3cmC5FRV0cKGRGUS8K3vJ0fHDTe\nZnn6uusxWl2DY/uXUIGyudr88Jo3QuECmpVODZofCMM3WMjM5p4JU80Xfc0kqPXn\n8YkRzf22VrOuZc4cKNGZ2+OXciHvlX7WoCyMl/1vQi5HWwMxcDJe4kNqTZCUXdxz\nk2TYktm+U5dBCqinaDSdwhvMiqkwUd7C860ZVejmXaw/9IFFfIiGAFi9OqRC4hSe\nrCCIXfszAgMBAAECggEADujqdcXpmJBWTy/+pn2JbvUPPAmH6H62dKT1NWUrp7YK\nOA2PIVbmH0pAD9xodvwTCUKjquN6UeL0a5kq96oaXq4hjjjD1eP5F3QloTVEohe4\nd0e+bxBvOH1hddm/eY4+HV5hDQZ40ZEgpmYxkvYGA/cw4GfeRXBQm53XfM+QdLQl\n8dPtmNOUtQFlWwNEQ7ifB/OXXhWcL4yxr5z2GgF5Tz6twf8HJ0lTF2gt9xXAb33e\nAxZ7et1SoILSwM2IbyCuE8SEl0vfdghCmGBNsSTCdXNJgesSZ0sNcxH9armX84R7\n7mvgRgrCdZkbXZSkIHf2kuIcbF1N0HE4hc6EWewEcQKBgQDlef9g+OWf8NhKj7Qn\nADcQJHkDASQ89ruICFqFcKc1VA+LoLoCJW6RYNenEwY2nmKPY/SlrdX3wLW/DvYr\nCVw+m82BRNrLBZk3SVhsK1Oz599nJ0oaZIUqn/d8jTd3P5Rfj1MW3pUxWGXp0FUD\ndZ5Pjd1EVoNdtzT9akk+Y29H1QKBgQDPASyvSerw6lHNg7xkAxcQoxk+GHD7XW3O\ngKoVqrNPTPxW+0HDS5V7AoHihTaQiyHADTzatkWxZMeVpLECtcgOYW+NKpQuX6BZ\nS+yzL5tWu6Gkz+2nVAZ9r6XYrI8/sVOklxmy0YCepZ18TTRDhM3Wb8pxIhYpMfto\nuoJ5gZKC5wKBgF6WyQOp1Mz2s9EYJwQkfI302sS4Rb2zXl2WQ+nZI7NBsR6247Va\nxJa8Omgt2VPOOYYth3x7iUUUdFYKzsGynAlao3zzatRgvGUjoIUF3vy7SUT11v/G\nC3YkdhkrBH/s/zXcmD1HBmxOqcOVElXYt6bHLTusBX4ttHP4ybQjvIz5AoGAHvUV\nAq3yRc81JqAB548Svvk1SDUNiHoDdvOE/DKIGF7yCewPfK5sD3ZjiEXV0C4OgRM5\nu5ewuoPQ2U9E7ZvT7Fqj/R+WA41nhJM3NeDzwTfUR7qU1wZY5ikDi3DbB3J0iNPU\nEwsDyjYIZFODcmmFAwG668pmOAjyiUoDQzqed5cCgYAi8HTtqZYvZpOE2OUciODB\ntLTbJtFXazbPr4xV2rqgM52CqEMZOO5rLTICKXM8Uhq0AQoeYU/JDOUlfnKLW3vJ\nGE5I4tSMw233I2iZzL57pOXe6GvsEBcgMMd+3wkdujrKt/32scBfTsxi8Or7Pvky\n5UjNmgXWOunl78RRZu9C0w==\n-----END PRIVATE KEY-----\n";

// The ID of the spreadsheet where you'll publish the data
const char spreadsheetId[] = "1hmVdTuU23DEGpRJ2CO5B_d5rXRlbKDvbifHDGmDeeYQ";  // ID of Motor 1
// const char spreadsheetId[] = "1b62-0-xNsipY5wJcuc_Lffsp0KV5vZ6EDmTuUMVTr3s";  // ID of Motor 2
// const char spreadsheetId[] = "1C0yorRd_W_cLynwWoery2YBzu0RRpuAi_xy3jpVK8iw"; // ID of Motor 3

// const char spreadsheetId[] = "1771oWV58r2mJsYRpEj9wuJeiDTv88GUy07eUdJ38o3o";  // ID of conoweederdc@gmail.com

SoftwareSerial PZEMSerial(MAX485_RO, MAX485_DI);

// Address PZEM-017 : 0x01-0xF7
static uint8_t pzemSlaveAddr = 0x01; // default address

// shunt -->> 0x0000-100A, 0x0001-50A, 0x0002-200A, 0x0003-300A
// we are using 50A shunt
static uint16_t NewshuntAddr = 0x0001;

ModbusMaster node;

float Previous_Voltage, Previous_Current, Previous_Power, Previous_Energy;
float PZEMVoltage, PZEMCurrent, PZEMPower, PZEMEnergy;

unsigned long startMillisPZEM;         /* start counting time for LCD Display */
unsigned long currentMillisPZEM;       /* current counting time for LCD Display */
const unsigned long periodPZEM = 1000; // refresh every X seconds (in seconds) in LED Display. Default 1000 = 1 second

unsigned long startMillisReadData; /* start counting time for data collection */
unsigned long startMillis1;        // to count time during initial start up (PZEM Software got some error so need to have initial pending time)
