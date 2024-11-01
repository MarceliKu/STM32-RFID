# STM32-RFID
This small repository contains two projects for STM32 development board NUCLEO-G474RE.
The main purpose of creating these projects was to verify and demonstrate how to use the 7941W module to read and write RFID tags, but not only.

Both projects were created using STM32CubeIDE v.1.12.1 with STM32CubeMX v. 6.8.1-RC4.
These projects do not require any additional hardware besides the Nucleo board and the 7941W module.  Instead of a display, all messages are sent via the UART connector (specifically LPUART1) to a PC, on which you need to run one of the serial terminal programs such as [RealTerm](https://realterm.sourceforge.io/). Of course, the Nucleo board should be connected to the PC via a microUSB cable, which is also used to program the STM32 G474RE microcontroller. RealTerm will also allow you to send commands to the Nucleo board in the form of single bytes.
The connection between the 7941W module and the Nucleo was made via USART1.
 
## 7941W - a dual-band RFID reader and writer module
7941W module this is a dual frequency 13.56 MHz and 125 KHz RFID Reader Writer Wireless Module. 
Communication with this module is provided by UART.

![7941W_1](https://github.com/user-attachments/assets/00245590-f4fe-4613-934a-af5e016b60d1)

Perhaps the weakest point of the 7941W module is the very limited specification. I have not been able to find anything more than that on this page: [here](http://www.icstation.com/dual-frequency-rfid-reader-writer-wireless-module-uart-1356mhz-125khz-icidmifare-card-p-12444.html). 

Here is the complete set of information I was able to find.
(Some of the following information still makes me doubtful, but it was nevertheless enough to build both projects.)

### Features:
- Voltage: DC 5V
- Current: 50 mA
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
1. Read Specified Sector (0x12): the first byte of the data represents sector; the second byte means the certain block of the sector; the third byte means A or B group password (0x0A/0x0B);
    then it comes with password of 6 bytes.
2. Write Specified Sector (0x13): the first byte of the data represents sector; the second byte means the certain block of the sector; the third byte means A or B group password (0x0A/0x0B);
    then it comes with password of 6 bytes and block data of 16 bytes.
3. Modify Password (0x14): the first byte means the certain sector; the second byte means A or B group password (0x0A/0x0B); then it comes with old password of 6 byte and new password.

Data bytes in receive messages:
1. The data received as a response to Read Specified Sector (0x012) has the following format: the first byte is the sector; the second byte is the specified sector block; and then 16 bytes of block data.

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
To make it easier to get started with these projects, the necessary equipment and its connections have been minimized. They are shown in the photo below:   

![NucleoSTM32-7491W](https://github.com/user-attachments/assets/31dc22c6-c049-4d99-920a-c960bb15a859)

 
|7941W module      |NUCLEO-G474RE - Arduino Uno expansion connector |
|------------------|------------------------|
|`5V` DC power supply pin	   | +5V (CN6 pin 5)        |
|`RX`	receive pin            | TX/D1 (CN9 pin 2)      |   
|`TX`	transmit pin           | RX/D0 (CN9 pin 1)      |   
|`GND` power supply ground	  | GND (CN6 pin 6 or 7)   |   
|`IO` pin of unknown purpose | not connected          |   

## RFID_7941W_Replicator
The first project implements the idea of copying the ID of one RFID Tag to another.
Using the capabilities of the 7941W module, the project allows to copy tags for both RFID tags operating at 125 KHz and 13.56 MHz.

Copying is performed in several steps:
1. Waiting for RFID Tag with id to copy. 
2. Reading an id to be copied
3. Waiting for RFID Tag with a different id than the one read above.
4. Write the ID read in step 2 to the RFID tag currently located on the 7941W coil.

In order to conveniently use the copying process, you need to start and properly configure RealTerm.
The most important thing is to configure the connection parameters on the “Port” tab:

![Console_config_port](https://github.com/user-attachments/assets/9ea470e3-3e54-4911-9290-e43fd37a998a)

Then open a connection to the port to which LPUART1 of your board is connected. This can be done by selecting the appropriate item from the “Port” list and clicking the “Open” button.

The full replication process on the console will look as follows: 

![Replicator](https://github.com/user-attachments/assets/eda4e3ae-459e-46ff-8e41-9fa5cd769ada)

The program recognizes the type of RFID tag and allows copying id only to the same type of tag: 125 KHz  -> 125 KHz  or  13.56 MHz -> 13.56 MHz. 

To start the replication process again, simply press the blue button on the Nucleo board at any time.

## RFID_7941W_Console
Second project is a bit more complicated but

![Console_send](https://github.com/user-attachments/assets/82b822d3-f417-4bcc-af59-6e9be7bccfd4)



![Console_option1](https://github.com/user-attachments/assets/c8fc3273-c9ad-4bcf-92d8-f2c25992201b)


 
![Console_option3](https://github.com/user-attachments/assets/c65d426a-f6b0-4ba0-9115-f32d54b14e26)



![Console_option4](https://github.com/user-attachments/assets/d31bb0bb-4842-457e-b359-6069facd185d)



![Console_option6](https://github.com/user-attachments/assets/90ca0be6-b4a1-4817-928a-e6f5bc3063f8)



![Console_option9](https://github.com/user-attachments/assets/62041207-012a-40d7-9f63-49f79f7ccc54)




