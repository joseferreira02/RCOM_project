// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 5

#define FALSE 0
#define TRUE 1

int alarmEnabled = FALSE;
int alarmCount = 0;


// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);
}

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 30; // Inter-character timer unused
    newtio.c_cc[VMIN] = 5;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    (void)signal(SIGALRM, alarmHandler);

    // Create string to send
    unsigned char set_frame[5] = {0};
    set_frame[0] = 0x7E;
    set_frame[1] = 0x03;
    set_frame[2] = 0x03;
    set_frame[3] = set_frame[1]^set_frame[2];
    set_frame[4] = 0x7E;

    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.
    
    while (alarmCount < 4){
        if (alarmEnabled == FALSE){
            int bytes = write(fd, set_frame, 5);
            if (bytes < 0) printf("ERROR SENDING SET FRAME");

            alarm(3);
            alarmEnabled = TRUE;
            unsigned char buf[BUF_SIZE + 1] = {0};
            int bytes2 = read(fd, buf, BUF_SIZE + 1);
            buf[bytes2] = '\0'; // Set end of string to '\0', so we can printf
            if (bytes2 > 0){
                if (buf[0] == 0x7E && buf[1] == 0x01 && buf[2] == 0x07 && buf[3] == buf[1]^buf[2] && buf[4] == 0x7E){
                        for (int i = 0; i < bytes; i++){
                            printf("buf[%d] = 0x%02X\n", i, buf[i]);
                        }
                        printf("%d bytes written\n", bytes);
                        printf("RECEIVED UA FRAME. ENDING RETRANSMISSION.\n");
                        break;
                }
                else printf("ERROR READING UA FRAME\n");
            }
        }
    }

    if (alarmCount >= 4) printf("ALARM LIMIT REACHED\n");

    // Wait until all bytes have been written to the serial port
    sleep(1);

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
