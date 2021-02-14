/*
 * uftp_client.c - A simple UDP client
 * usage: client <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#define BUFSIZE 1024
#define MAX_ATTEMTS 100

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

/*
 * packet - wrapper for Packet Info
 */
typedef struct {
    long int ID;
    char p[BUFSIZE];
    long int length;

}
        packet_info;


int main(int argc, char **argv) {
    int sockfd, portno, n, m, recvlen, serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname, buf[BUFSIZE], inp[BUFSIZE],command[10] = {0}, filename[20] = {0}, key = 10;
    packet_info packet;
    memset(&packet, 0, sizeof(packet));
    long int ACK = 0;
    struct timeval timeout = {0, 700000};
    FILE *fptr;

    /* check command line arguments */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* get host by name: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    memset(&serveraddr, 0, sizeof(serveraddr));
    socklen_t addrlen = sizeof(serveraddr);
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *) server->h_addr,
          (char *) &serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    //Make a connection to the server
    connect(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
    //We continue to keep the connection alive until an exit command is received.
    while (1) {
        // Helps the user to see what all commands are accepted with arguments.
        printf("\nEnter any of the Following Commands  \n 1. get [file_name] \n 2. put [file_name] \n 3. delete [file_name] \n 4. ls \n 5. md \n 5. exit \n");

        memset(inp, 0, 50);
        memset(command, 0, 10);
        memset(filename, 0, 20);
        // Receives the command as user inputs
        scanf(" %[^\n]%*c", inp);
        sendto(sockfd, inp, strlen(inp), 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
        sscanf(inp, "%s %s", command, filename);


        /* Implementation of PUT commands */
        if (((strcmp(command, "put") == 0)) && (*filename != '\0')) // Check if the filename is empty or not for a PUT command
        {
            long int num_packets = 0, dropped = 0;
            fptr = fopen(filename, "rb"); // open the file to read
            /* Set the timeout to receive an acknowledgment */
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout,
                       sizeof(struct timeval));

            if ((fptr == NULL)) // Check to see if file information is correct
            {
                printf("Wrong File name \n");
                /* We send a null size file so that the server is aware of the malfunctioned file */
                sendto(sockfd, &num_packets, sizeof(num_packets), 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
                /* Receive ACK for number of packets */
                recvfrom(sockfd, &num_packets, sizeof(num_packets), 0, (struct sockaddr *) &serveraddr, &addrlen);
            } else {

                int counter = 0, timeout_flag = 0;
                fseek(fptr, 0, SEEK_END);
                size_t file_size = ftell(fptr);
                fseek(fptr, 0, SEEK_SET);
                memset(&packet, 0, sizeof(packet));

                printf("File Size - %ld \n", file_size);
                /* Calculating the Number of packets to be sent to Server
                 * This way we can keep track of each packet even in case of a packet loss
                 * We retry the packet loss again in case we dont receive the acknowledgment
                 */
                num_packets = (file_size / BUFSIZE) + 1;

                /* Start sending the packets to the server/receiver */
                sendto(sockfd, &(num_packets), sizeof(num_packets), 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr));

                /* Continue to wait until the timeout incase of no show of acknowledgement*/
                while (recvfrom(sockfd, &ACK, sizeof(ACK), 0, (struct sockaddr *) &serveraddr, &addrlen) < 0) {

                    sendto(sockfd, &(num_packets), sizeof(num_packets), 0, (struct sockaddr *) &serveraddr,sizeof(serveraddr));
                    counter++;

                    /* We attempt to retry for the MAX_ATTEMTS times.
                     * If it fails then client throws a connection timeout
                     */
                    if (counter == MAX_ATTEMTS) {
                        timeout_flag = 1;
                        printf("File can't be sent  -  Connection Timeout\n");
                        break;
                    }
                }

                printf("packets - %ld \n", num_packets);
                /* Sending the actual packets/data */
                long int i, j;
                for (i = 0; i < num_packets; i++) {
                    memset(&packet, 0, sizeof(packet));
                    packet.length = fread(packet.p, 1, BUFSIZE, fptr), counter = 0;
                    ACK = 0;
                    packet.ID = i;
                    for ( j = 0; j < packet.length; j++) {
                        packet.p[j] ^= key;
                    }

                    /* Send the encrypted data */
                    sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr));

                    /* Throw a connection timeout incase of a timeout flage */
                    if (timeout_flag) {
                        printf("File Not Sent - Connection Timeout\n");
                        break;
                    }

                    /* receive acknowledgement from the server/receiver */
                    recvfrom(sockfd, &ACK, sizeof(ACK), 0, (struct sockaddr *) &serveraddr, &addrlen);

                    /* Send the packets to the receiver
                     * Retry sending the packet until acknowledgement is received
                     * Retry for at a max of MAX_ATTEMTS
                     * Appropriate print statements are provided for easy understanding
                     */
                    while ((ACK != packet.ID)) {
                        sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
                        recvfrom(sockfd, &ACK, sizeof(ACK), 0, (struct sockaddr *) &serveraddr, &addrlen);
                        printf("\t\tPacket no %ld dropped, Total - %ld \n", packet.ID, ++dropped);
                        counter++;
                        if (counter == MAX_ATTEMTS) {
                            timeout_flag = 1;
                            break;
                        }
                    }

                    printf("ACK for Packet, i = %ld ACK = %ld recieved \n", i, ACK);

                    if (i == num_packets - 1)
                        printf("File Sent \n");

                }

            }
            /* Nullifying the timeout*/
            struct timeval timeout = { 0,0 };
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(struct timeval));

        }

        /* Implementation of GET command */
        else if ((strcmp(command, "get") == 0) && (*filename != '\0')) {

            long int num_packets = 0;
            /* Receive the number of expected packets from the server*/
            recvfrom(sockfd, &num_packets, sizeof(num_packets), 0, (struct sockaddr *) &serveraddr, &addrlen);

            /* Acknowledgement from the server */
            sendto(sockfd, &num_packets, sizeof(num_packets), 0, (struct sockaddr *) &serveraddr, addrlen);

            printf("Packets expected - %ld \n", num_packets);


            /* Null check */
            if (num_packets == 0) {
                printf("Empty File - Not Created \n");
            }
            else if (num_packets > 0) {
                int total_bytes = 0;
                long int i, j;
                /* Create a new file to write */
                fptr = fopen(filename, "w");

                /* Write the contents of the packets into the file */
                for (i = 0; i < num_packets; i++) {
                    /* Emptying the packet buffer*/
                    memset(&packet, 0, sizeof(packet));

                    /* Receive the packet */
                    recvlen = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &serveraddr, &addrlen);

                    /* send acknowledgement */
                    sendto(sockfd, &(packet.ID), sizeof(packet.ID), 0, (struct sockaddr *) &serveraddr, addrlen);

                    /* If same packet is received, discard the same
                     * This will not put the program in an infinite loop
                     */
                    if (packet.ID < i)
                    {
                        i--;
                        printf("Same Packet %ld Discarded \n", packet.ID);
                    } else {

                        for (j = 0; j < packet.length; j++) {
                            packet.p[j] ^= key;
                        }
                        /* write the packet to the file */
                        fwrite(packet.p, 1, packet.length, fptr);
                        printf("Packet number = %ld Recieved Size %ld  \n", packet.ID, packet.length);
                        total_bytes += packet.length;
                    }

                }
                int k ;
                for (k = 0; k < MAX_ATTEMTS; k++) {
                    struct timeval tout = {0, 800 };
                    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &tout, sizeof(struct timeval));
                    recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &serveraddr, &addrlen);
                    tout.tv_sec = 0;
                    tout.tv_usec = 0;
                    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &tout, sizeof(struct timeval));
                }
                printf("Total bytes - %d\n", total_bytes);
                fclose(fptr);
            }

        }

        /* Implementation of DELETE command */
        else if ((strcmp(command, "delete") == 0) && (*filename != '\0')) {
            char ret[50];
            memset(ret, 0, 50);
            /* Receive the delete response from the server */
            recvfrom(sockfd, ret, 50, 0, (struct sockaddr *) &serveraddr, &addrlen);
            printf(" %s \n", ret);
        }

        /* Implementation of MD5SUM  command (Optional)*/
        else if ((strcmp(command, "md") == 0) && (*filename != '\0')) {
            char cmd[50] = "md5sum ";
            strcat(cmd, filename);
            printf("\n md5sum of %s on Client Side \n\n\t", filename);
            system(cmd);
        }

        /* Implementation of LS command */
        else if (strcmp(command, "ls") == 0) {
            char filelist[200] = {
                    0
            };
            memset(filelist, 0, 200);
            recvfrom(sockfd, filelist, 200, 0, (struct sockaddr *) &serveraddr, &addrlen);
            printf("\n \nList of files is: \n%s \n", filelist);

        }

        /* Implementation of EXIT command */
        else if (strcmp(command, "exit") == 0) {
            return 0;
        }

        /* Implementation of other command */
        else {
            printf("Error (Wrong Command or Wrong filename) \n");
        }
    }

}