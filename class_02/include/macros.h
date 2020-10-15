#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1
#define FALSE 0
#define TRUE 1
#define BYTE 1

#define BIT(n) 1 << n

/* Macros for the llopen */ 
#define TRANSMITTER         0
#define RECEPTOR            1 

#define TRIES           3               /* Tries to read the receptor answers*/ 
#define TIMEOUT         3               /* Time to wait for the receptor answers*/


/* MACROS FOR THE PROTOCOL*/

#define FLAG 0x7E

/*Address Field*/

#define ADDR_CMD_EMI    0x03    /* Command sent by the emissor*/
#define ADDR_ANS_EMI    0x01    /* Answer sent by the receptor*/

#define ADDR_CMD_REC    0x01    /* Command sent by the emissor*/
#define ADDR_ANS_REC    0x03    /* Aswser sent by the receptor*/

/**Command Field*/

#define CMD_SET         0x03    /* SET command*/
#define CMD_DISC        0x0B    /* DISC command*/
#define CMD_UA          0x07    /* UA command*/ 
#define CMD_RR1         0x85    /* RR*/ 
#define CMD_RR0         0x05    /* RR*/ 
#define CMD_REJ1        0x81    /* REJ*/ 
#define CMD_REJ0        0x01    /* REJ*/


#define MAX_SIZE 255 /* Max size of the package */
