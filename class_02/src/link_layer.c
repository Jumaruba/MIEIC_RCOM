#include "../include/link_layer.h"

int numTransmissions = 0;


int llopen(byte *port, int flag, struct termios *oldtio, struct termios *newtio)
{
    int fd = -1;
    int res = -1;
    
    if (flag != TRANSMITTER && flag != RECEPTOR){
        PRINT_ERR("Actual flag %d. Must be 1 or 0.", flag); 
        return -1; 
    }

    if (TRANSMITTER == flag) { 

        fd = openDescriptor(port, oldtio, newtio); 

        // Install the alarm. 
        if (signal(SIGALRM, handle_alarm_timeout) == SIG_ERR){
            printf("Not possible to install signal, SIG_ERR."); 
            exit(-1); 
        }
        siginterrupt(SIGALRM, TRUE);

        // Establishment of the connection.  
        while (res != 0) { 
            alarm(3);
            if (send_frame_nnsp(fd, A, CMD_SET) < 0)
                PRINT_ERR("Not possible to send CMD_SET. Sending again after timeout..."); 
            else PRINT_SUC("Written CMD_SET.");
            
            if((res = read_frame_timeout_sp(fd, CMD_UA)) >= 0)
                PRINT_SUC("Received UA.");
        }

        if (res == 0) alarm_off();
    }

    else if (RECEPTOR == flag)
    {
        fd = openDescriptor(port, oldtio, newtio);
        while(res < 0){
            // Establishment of the connection. 
            read_frame_nn(fd, CMD_SET);
            PRINT_SUC("Received CMD_SET with success.");

            if ((res = send_frame_nnsp(fd, A, CMD_UA)) < 0)
                PRINT_ERR("Error sending answer to the emissor.");
        } 
    }
    return fd;
}

int llwrite(int fd, byte *data, int *data_length) {
    static int s_writer = 0, r_writer = 1;  

    byte * frame  = (byte*) malloc(MAX_SIZE_ARRAY*sizeof(byte));
    int res = -1 ; 
    
    if (*data_length < 0) {
        printf("Length must be positive");
        return -1;
    }

    int frame_length = create_frame_i(data, frame, *data_length, CMD_S(s_writer));     

    while(res != 0){
        // Creating the info to send
        alarm(3); 

        write(fd, frame, frame_length); 
        
        byte CMD; 
        res = read_frame_timeout_nn(fd, &CMD); 

        if (CMD == CMD_RR(r_writer)){ 
            alarm(0); 
            r_writer = SWITCH(r_writer); 
            s_writer = SWITCH(s_writer); 
            return 0; 
        }
        
    } 
    
}

int llread(int fd, byte * data){   
    int tries = 0;                    
    int data_length = -1;
    static int s_reader = 0, r_reader = 1;  // s and r arguments. 

    while(tries < TRIES){  
        if ((data_length = read_frame_i(fd, data, CMD_S(s_reader))) > 0){
    
            byte_destuffing(data, &data_length);   
            // Check the bcc2. 
            byte check_BCC2 = 0x00; 
            create_BCC2(data, &check_BCC2, data_length-1);  

            if (check_BCC2 != data[data_length-1]) {
                send_frame_nnsp(fd, A, CMD_REJ(r_reader));  
                tries ++; 
            }
            else{ 
                send_frame_nnsp(fd, A, CMD_RR(r_reader));  
                r_reader = SWITCH(r_reader); 
                s_reader = SWITCH(s_reader); 
                return data_length;
            }
        }
        else{
            send_frame_nnsp(fd, A, CMD_REJ(r_reader)); 
            tries++; 
        }
    }
    return -1;
}

//TODO: confirm with the professor. Case we don't the UA from the emissor, do we turn off?  
//TODO: Case we do not receive the DISC from the emissor, do we turn off? -> doing NOT turning off
//TODO: is there timeout here? I did it with.  
//TODO: if timeout, we just exit the program or we init the llclose? I'm doing exit(-1)
int llclose(int fd, int flag, struct termios *oldtio){ 

    int res = -1; 
    int failed_sending = 0, failed_reading = 0; 

    if (flag != TRANSMITTER && flag != RECEPTOR){
        PRINT_ERR("Invalid flag.");
        return -1; 
    }
    if (flag == TRANSMITTER){ 
        alarm(3); 
        while(res < 0 && failed_sending < TRIES_SEND){                   
            if ((res = send_frame_nnsp(fd, A, CMD_DISC)) < 0){
                PRINT_ERR("Emissor failed sending CMD_DISC. Sending again...");  
                sleep(DELAY_US);   /* Wait a little before sending again.*/  
                failed_sending++; 
                continue; 
            }else PRINT_SUC("Emissor has sent CMD_DISC."); 
            failed_sending = 0;     /* Has sent information, resetart failed_sending.*/ 
            if((res = read_frame_nn(fd, CMD_DISC)) < 0){
                PRINT_ERR("Emissor failed in receive CMD_DISC. Sending CMD_DISC again...");  
                continue; 
            } else PRINT_SUC("Emissor has read CMD_."); 
            // Here doesn't matter if it was sent or not. The emissor must turn off. 
            if (send_frame_nnsp(fd, A, CMD_UA) < 0 )
                PRINT_ERR("Emissor failed in sending CMD_UA. Turning off.");
            else PRINT_SUC("Emissor has read CMD_UA."); 
            
        } 
        if (failed_sending == TRIES_SEND){
            PRINT_ERR("Emissor unable to send CMD_DISC. Not turning off."); 
            return -1; 
        }
        else 
            return closeDescriptor(fd, oldtio); 

    }else if (flag == RECEPTOR){    
        while(res != 0 && failed_reading < TRIES_READ){
            if((res = read_frame_nn(fd, CMD_DISC)) < 0){
                PRINT_ERR("Receptor failed reading CMD_DISC"); 
                failed_reading++;
                continue; 
            }
            else PRINT_SUC("Receptor read CMD_DISC"); 
        }
        printf("%d\n", failed_sending);  
        res = -1; 
        while(res < 0 && failed_sending < TRIES_SEND){
            if ((res = send_frame_nnsp(fd, A, CMD_DISC)) < 0){
                PRINT_ERR("Receptor failed sending CMD_DISC. Trying again..."); 
                failed_sending++; 
                continue; 
            }
            else PRINT_SUC("Receptor sent CMD_UA"); 
        }
        return closeDescriptor(fd, oldtio); 
    } 
    return -1; 

}

int send_frame_nnsp(int fd, byte ADDR, byte CMD)
{
    byte frame[5];
    frame[0] = FLAG;
    frame[1] = ADDR;
    frame[2] = CMD;
    frame[3] = frame[1] ^ frame[2];
    frame[4] = FLAG;

    return write(fd, frame, 5);
}

int read_frame_nn(int fd, byte CMD)
{
    int curr_state = 0; /* byte that is being read. From 0 to 4.*/
    byte data;

    while (TRUE) { 

        if (read(fd, &data, 1) == -1) 
            return -1; 

        switch (curr_state) { 

        // RECEIVE FLAG
        case 0: 

            printf("case 0: %02x\n", data);
            if (data == FLAG)
                curr_state++;
            break;

        // RECEIVE ADDR
        case 1:
            printf("case 1: %02x\n", data);
            if (data == A)
                curr_state++;
            else if (data != FLAG)
                curr_state = 0;
            break;

        // RECEIVE CMD
        case 2:
            printf("case 2: %02x\n", data);
            if (data == CMD)
                curr_state++;
            else if (data == FLAG)
                curr_state = 1;
            else
                curr_state = 0;
            break;
        // RECEIVE BCC
        case 3:
            printf("case 3: %02x\n", data);
            if (data == (CMD ^ A))
                curr_state++;
            else if (data == FLAG)
                curr_state = 1;
            else
                curr_state = 0;
            break;

        // RECEIVE FLAG
        case 4:
            printf("case 4: %02x\n", data);
            if (data == FLAG) return 0; 
            else curr_state = 0;
        }
    }
    return -1;
}

int read_frame_i(int fd, byte *buffer, byte CMD){
    int curr_state= 0, info_length = -1; 

    byte byte; 

    while(curr_state < 5){
        if (read(fd, &byte, 1) == -1)
            return -1; 

        switch (curr_state)
        { 
            // RECEIVE FLAG
            case 0: 
                info_length = 0; 
                printf("case 0: %02x\n", byte);
                if (FLAG == byte) 
                    curr_state ++;  
                break; 
            // RECEIVE ADDR 
            case 1: 
                printf("case 1: %02x\n", byte); 
                if (A == byte)
                    curr_state ++; 
                else if (FLAG != byte) 
                    curr_state = 0;  
                
                break; 

            // RECEIVE CMD
            case 2: 
                printf("case 2: %02x\n", byte);  
                if (byte == CMD)
                    curr_state++;
                else if (byte == FLAG) 
                    curr_state = 1; 
                else curr_state = 0;

                break; 

            // RECEIVE BCC1
            case 3: 
                printf("case 3: %02x\n", byte);    
                if (byte == (CMD ^ A))
                    curr_state ++; 
                else if (byte == FLAG) 
                    curr_state = 1; 
                else if (CMD == CMD_S(0)){
                    send_frame_nnsp(fd, A, CMD_REJ(1)); 
                    curr_state = 0; 
                }
                else if (CMD == CMD_S(1)){
                    send_frame_nnsp(fd, A, CMD_REJ(0));  
                    curr_state = 0; 
                } 
                break;
            // RECEIVE INFO 
            case 4:
                printf("case 4: %02x\n", byte);
                if (byte != FLAG){
                    buffer[info_length++] = byte;  
                }
                else curr_state ++; 
            
        } 
    }
    return info_length;
}

int read_frame_timeout_nn(int fd, byte *CMD){
   int curr_state = 0; /* byte that is being read. From 0 to 4.*/
    byte byte;

    while (curr_state < 5)
    {
        if (read(fd, &byte, 1) == -1) 
            return -1;

        switch (curr_state)
        {
        // RECEIVE FLAG
        case 0: 

            printf("case 0: %02x\n", byte);
            if (byte == FLAG)
                curr_state++;
            break;

        // RECEIVE ADDR
        case 1:
            printf("case 1: %02x\n", byte);
            if (byte == A)
                curr_state++;
            else if (byte != FLAG)
                curr_state = 0;
            break;

        // RECEIVE CMD
        case 2:
            printf("case 2: %02x\n", byte);
            if (byte == CMD_REJ(0) || byte == CMD_REJ(1) || byte == CMD_RR(0) || byte == CMD_RR(1)){ 
                *CMD = byte; 
                curr_state++;
            } 
            else if (byte == FLAG)
                curr_state = 1;
            else
                curr_state = 0;
            break;
        // RECEIVE BCC
        case 3:
            printf("case 3: %02x\n", byte);
            if (byte == (*CMD ^ A))
                curr_state++;
            else if (byte == FLAG)
                curr_state = 1;
            else
                curr_state = 0;
            break;

        // RECEIVE FLAG
        case 4:
            printf("case 4: %02x\n", byte);
            if (byte == FLAG){
                curr_state++;
                return 0; 
            }
            else
                curr_state = 0;
        }
    }
    return curr_state;
}

int read_frame_timeout_sp(int fd, byte CMD)
{
    int curr_state = 0;     /* byte that is being read. From 0 to 4.*/
    byte data;

    while (TRUE) {

        if (read(fd, &data, 1) == -1)
            return -1;

        switch (curr_state) {
        // RECEIVE FLAG
        case 0:
            printf("case 0: %02x\n", data);
            if (data == FLAG)
                curr_state++;
            break;

        // RECEIVE ADDR
        case 1:
            printf("case 1: %02x\n", data);
            if (data == A)
                curr_state++;
            else if (data != FLAG)
                curr_state = 0;
            break;

        // RECEIVE CMD
        case 2:
            printf("case 2: %02x\n", data);
            if (data == CMD)
                curr_state++;
            else if (data == FLAG)
                curr_state = 1;
            else
                curr_state = 0;
            break;
        // RECEIVE BCC
        case 3:
            printf("case 3: %02x\n", data);
            if (data == (CMD ^ A))
                curr_state++;
            else if (data == FLAG)
                curr_state = 1;
            else
                curr_state = 0;
            break;

        // RECEIVE FLAG
        case 4:
            printf("case 4: %02x\n", data);
            if (data == FLAG) return 0; 
            else curr_state = 0;
        }
    }

    return -1;
}

int create_frame_i(byte *data, byte *frame, int data_length, byte CMD)
{ 
    int frame_length = 0;  

    // Stuffing bcc and data.  
    byte *BCC2 = (byte*)malloc(sizeof(byte)); 
    BCC2[0] = 0x00; 
    int bcc_length = 1;  

    create_BCC2(data, BCC2, data_length);  
    byte_stuffing(data, &data_length);  
    byte_stuffing(BCC2, &bcc_length);   

    // Store information 
    frame_length = 5  + bcc_length + data_length;  
    frame[0] = FLAG;
    frame[1] = A;
    frame[2] = CMD; 
    frame[3] = frame[1]^frame[2];  
    // BCC 
    memcpy(&frame[4], data, data_length);    
    memcpy(&frame[4 + data_length], BCC2, bcc_length); 

    frame[frame_length-1] = FLAG;   

    printf("Created frame i\n"); 
    return frame_length; 
}

void create_BCC2(byte * data, byte* buffer, int data_length)
{
    for (int i = 0 ; i < data_length; i++){
        *buffer ^= data[i]; 
    } 
}

int byte_stuffing(byte * frame, int* frame_length)
{ 
    byte * new_frame ;      
    int extra_space = 0;        /* The extra space needed to be added. */
    int new_frame_length = 0;   /* The new length of the string frame. */ 
    int actual_pos = 0;         /* Position in the new_frame. */ 
    int counter = 0;            

    //  First find all the flags and scapes to avoid multiple reallocs. 
    for (int i = 0 ; i < *frame_length; i++)
        if (frame[i] == FLAG || frame[i] == ESC) extra_space++;  


    new_frame_length = extra_space + *frame_length;  
    new_frame = (byte *)malloc(sizeof(byte) * new_frame_length);     

    for (int i = 0 ; i< *frame_length; i++){  
        actual_pos = i + counter; 
        if (frame[i] == FLAG){    
            new_frame[actual_pos] =  ESC; 
            new_frame[actual_pos+1] = XOR_STUFFING(FLAG);  

            counter ++; 
        }
        else if (frame[i] == ESC){
            new_frame[actual_pos] = ESC; 
            new_frame[actual_pos+1] = XOR_STUFFING(ESC); 

            counter ++; 
        } 
        else new_frame[actual_pos] = frame[i];  
    } 


    frame = realloc(frame, new_frame_length); 
    * frame_length  = new_frame_length;

    memcpy(frame, new_frame, new_frame_length); 
    free(new_frame);
    return 0; 
}

int byte_destuffing(byte * frame, int * frame_length){
    
    int new_frame_pos = 0;  
    byte * new_frame = (byte*)malloc(sizeof(byte)*(*frame_length));   

    for (int i = 0 ; i < *frame_length; i++){
        if (frame[i] == ESC){
            if (frame[i+1] == XOR_STUFFING(FLAG))
                new_frame[new_frame_pos] = FLAG; 
            else if (frame[i+1] == XOR_STUFFING(ESC))
                new_frame[new_frame_pos] = ESC;  

            i++;  
        } 
        else new_frame[new_frame_pos] = frame[i]; 
        new_frame_pos ++;
    } 


    memcpy(frame, new_frame, new_frame_pos); 
    *frame_length = new_frame_pos;
    free(new_frame); 
}

int openDescriptor(byte *port, struct termios *oldtio, struct termios *newtio)
{
    int fd = -1; 
    if((fd = open(port, O_RDWR | O_NOCTTY)) < 0) {
        PRINT_ERR("Invalid port: %s", port);
        exit(-1); 
    }

    if (tcgetattr(fd, oldtio) == -1) { 
        PRINT_ERR("tcgetattr.");
        exit(-1);
    }

    bzero(newtio, sizeof(newtio));
    newtio->c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio->c_iflag = IGNPAR;
    newtio->c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio->c_lflag = 0;

    newtio->c_cc[VTIME] = 0; /* inter-character timer unused */
    newtio->c_cc[VMIN] = 1;  /* blocking read until 1 chars received */

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, newtio) == -1) {
        PRINT_ERR("tcsetattr");
        exit(-1);
    }

    PRINT_SUC("Function openDescriptor executed with success."); 

    return fd;
}

int closeDescriptor(int fd, struct termios * oldtio){
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }
    return close(fd);
}

handle_alarm_timeout()
{
    numTransmissions++;
    
    PRINT_ERR("Time out #%d", numTransmissions); 

    if (numTransmissions > TRIES)
    {
        PRINT_ERR("Number of tries exceeded\n"); 
        exit(-1);
    }

}

void alarm_off()
{
    numTransmissions = 0;
    alarm(0);
}
