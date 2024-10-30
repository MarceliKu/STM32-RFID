# STM32-RFID
This small repository contains two projects for STM32 development board NUCLEO-G474RE.
The main purpose of creating these projects was to verify and demonstrate how to use the 7941W module to read and write RFID tags.  
 
## 7941W - a dual-band RFID reader and writer module
7941W module this is a dual frequency 13.56 MHz and 125 KHz RFID Reader Writer Wireless Module. 
Communication with this module is provided by UART.

Perhaps the weakest point of the 7941W modules is the very limited specification. I have not been able to find anything more than that on this page: [here](http://www.icstation.com/dual-frequency-rfid-reader-writer-wireless-module-uart-1356mhz-125khz-icidmifare-card-p-12444.html). 

Here is the complete set of information I was able to find.

### Features:
- Voltage: DC 5V
- Current: 50mA
- Distance: Mifare>3cm; EM>5cm
- Size: 47mm x 26mm x 5mm
- Interface: UART, Wiegand
- Support Chips: ISO/IEC 14443 A/MIFARE, NTAG, MF1xxS20, MF1xxS70, MF1xxS50
- EM4100, T5577 read and write function
- Operating Temperature: -25-85 Celsius

### Connector pins:
1. `5V`: DC 5V power supply pin; if you use linearity power, it will gain better effects
2. `RX`: receive pin
3. `TX`: transmit pin
4. `GND`: power supply ground pin
5. `IO`: purpose unknown

### UART serial port communication protocol introduction:

**Send message structure:**                                                
|Protocol Header|Address|Command|Data Length|Data       |XOR Check|
|---------------|-------|-------|-----------|-----------|---------|
|AB BA	        |1 Byte	|1 Byte	|1 Byte	    |1-255 Bytes|1 Byte   |

**Receive message structure:**                                                
|Protocol Header|Address|Command|Data Length|Data       |XOR Check|
|---------------|-------|-------|-----------|-----------|---------|
|CD DC	        |1 Byte	|1 Byte	|1 Byte	    |1-255 Bytes|1 Byte   |

**Explanations:**
- Send header consists of two bytes: 0xAB, 0xBA
- Receive header consists of: (0xCD 0xDC)
- Address default value: 0x00 (I haven't found any information or examples with different value than this.) 

Possible Send Commands:
1. 0x10 read UID number
2. 0x11 write UID number (4 bytes), use default password ffffffffffff
3. 0x12 read specified sector
4. 0x13 write specified sector
5. 0x14 modify the password of group A or group B
6. 0x15 read ID number
7. 0x16 write T5577 number
8. 0x17 read all sector data (M1-1K card)

Return commands meaning:
1. 0x81 return operation succeeded
2. 0x80 return operation failed

Data Length: means number of bytes following "Data Length"; if it’s 0, then the following data will not occur.

Data bytes in send messages:
1. Read Specified Sector: the first byte of the data represents sector; the second byte means the certain block of the sector; the third byte means A or B group password (0x0A/0x0B);
    then it comes with password of 6 bytes.
2. Write Specified Sector: the first byte of the data represents sector; the second byte means the certain block of the sector; the third byte means A or B group password (0x0A/0x0B);
    then it comes with password of 6 bytes and block data of 16 bytes.
3. Modify Password: the first byte means the certain sector; the second byte means A or B group password (0x0A/0x0B); then it comes with old password of 6 byte and new password.

Data bytes in receive messages:
1. Read specified sector return data format, the first byte is sector; the second byte is the certain block of sector; then it comes with block data of 16 bytes.

XOR check byte is calculated from message bytes except protocol header.

**Examples of send messages:**
```
AB BA 00 10 00 10
```
```
AB BA 00 11 04 6D E9 5C 17 DA
```
```
AB BA 00 12 09 00 01 0A FF FF FF FF FF FF 10
```
```
AB BA 00 13 19 00 01 0A FF FF FF FF FF FF 00 01 02 03 04 05 06 07 08 09 01 02 03 04 05 06 07
```
```
AB BA 00 14 0E 00 0A FF FF FF FF FF FF 01 02 03 04 05 06 17
```
```
AB BA 00 15 00 15
```
```
AB BA 00 16 05 2E 00 B6 A3 02 2A
```
```
AB BA 17 07 0A FF FF FF FF FF FF 1A
```


## Necessary hardware and connections
The nesessary hardware and connections for these projects have been minimized to make easy start with  

![NucleoSTM32-7491W](https://github.com/user-attachments/assets/31dc22c6-c049-4d99-920a-c960bb15a859)


## RFID_7941W_Replicator
Teh first project is created as an example of 

## RFID_7941W_Console
Second project is a bit more complicated but
