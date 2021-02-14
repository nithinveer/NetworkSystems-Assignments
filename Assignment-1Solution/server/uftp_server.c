/*
 * uftp_server.c - A simple UDP echo server
 * usage: server <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define BUFSIZE 1024
#define MAX_ATTEMTS 100

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(1);
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
    int sockfd, portno, clientlen , optval, n, recvlen;
    struct sockaddr_in serveraddr, remaddr;
    struct sockaddr_in clientaddr;
    socklen_t addrlen = sizeof(remaddr);
    struct hostent *hostp;
    char buf[BUFSIZE], *hostaddrp, filename[20], command[10], key = 10;
    FILE *fptr;
    struct timeval timeout = {0,500000};
    long int ACK = 0;
    packet_info packet;

    /*
     * check command line arguments
     */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    /*
     * socket: create the parent socket
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets
     * us rerun the server immediately after we kill it;
     * otherwise we have to wait about 20 secs.
     * Eliminates "ERROR on binding: Address already in use" error.
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               (const void *) &optval, sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short) portno);

    /*
     * bind: associate the parent socket with a port
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr,
             sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    /*
     * main loop: wait for a datagram, then echo it
     */
    clientlen = sizeof(clientaddr);
    /* We continue to keep the connection alive until an exit command is received
     * Or when we hard terminate the execution
     */.
    while (1) {
        printf("Waiting on port %d\n", portno);

        /* Receive the commands from the client*/
        recvlen = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &remaddr, &addrlen);
        if (recvlen > 0) {
            buf[recvlen] = 0;
            printf("received message: \"%s\"\n", buf);
        }
        memset(command, 0, 10);
        memset(filename, 0, 20);
        sscanf(buf, "%s %s", command, filename);


        /* Implementation of PUT command */
        if (((strcmp(command, "put") == 0))) {

            if (*filename != '\0') {
                long int num_packets = 0;

                /* Receive the number of packets of the file from the client */
                recvfrom(sockfd, &num_packets, sizeof(num_packets), 0, (struct sockaddr *) &remaddr, &addrlen);

                /* Send an acknowledgement back to the client */
                sendto(sockfd, &num_packets, sizeof(num_packets), 0, (struct sockaddr *) &remaddr, addrlen);
                printf("Packets - %ld \n", num_packets);

                /* Null check */
                if (num_packets == 0)
                {
                    printf("Empty File - Not Created \n");
                }
                else if (num_packets > 0) {
                    int total_bytes = 0;
                    long int i, j;

                    /* Open a file to write */
                    fptr = fopen(filename, "w");

                    /* Iterate to write the packets into a file */
                    for (i = 0; i < num_packets; i++) {

                        /* empty the buffer */
                        memset(&packet, 0, sizeof(packet));

                        /* receive the packet from the client*/
                        recvlen = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &remaddr, &addrlen);

                        /* acknowledge the packet to the client */
                        sendto(sockfd, &(packet.ID), sizeof(packet.ID), 0, (struct sockaddr *) &remaddr, addrlen);

                        /* If same packet is received, discard the same
                        * This will not put the program in an infinite loop
                        */
                        if (packet.ID < i)
                        {
                            i--;
                            printf("Same Packet %ld Discarded \n", packet.ID);
                        }
                        else {

                            for ( j = 0; j < packet.length; j++) {
                                packet.p[j] ^= key;
                            }
                            /* Write the contents of the packet into the file */
                            fwrite(packet.p, 1, packet.length, fptr);
                            printf("Packet number = %ld Recieved Size- %ld  \n", packet.ID, packet.length);
                            total_bytes += packet.length;
                        }

                    }
                    printf("Total bytes - %d\n", total_bytes);
                    fclose(fptr);
                }

            }
            else {
                printf("Filename Not given \n");

            }

        }


        /* Implementation of GET command */
        else if ((strcmp(command, "get") == 0)) {

            /* Null check */
            if (*filename != '\0')
            {
                long int num_packets = 0, dropped = 0;

                /* Open the file for reading */
                fptr = fopen(filename, "rb");

                /* Set the timeout for acknowledgement*/
                setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(struct timeval));

                if ((fptr == NULL))
                {
                    printf("Wrong File name \n");
                    /* Send Zero packets as there is no file with the given name*/
                    sendto(sockfd, &num_packets, sizeof(num_packets), 0, (struct sockaddr *) &remaddr, sizeof(remaddr));
                    recvfrom(sockfd, &ACK, sizeof(ACK), 0, (struct sockaddr *) &remaddr, &addrlen);

                }
                else {
                    int counter = 0, timeout_flag = 0;
                    fseek(fptr, 0, SEEK_END);
                    size_t file_size = ftell(fptr);
                    fseek(fptr, 0, SEEK_SET);
                    memset(&packet, 0, sizeof(packet));

                    printf("File Size - %ld \n", file_size);

                    /* Compute the number of packets to be sent to client */
                    num_packets = (file_size / BUFSIZE) + 1;

                    /* Send the packets count to the client */
                    sendto(sockfd, &(num_packets), sizeof(num_packets), 0, (struct sockaddr *) &remaddr, sizeof(remaddr));

                    /* Incase of a timeout resend the packet */
                    while (recvfrom(sockfd, &ACK, sizeof(ACK), 0, (struct sockaddr *) &remaddr, &addrlen) < 0) {

                        sendto(sockfd, &(num_packets), sizeof(num_packets), 0, (struct sockaddr *) &remaddr, sizeof(remaddr));
                        counter++;

                        /* Send the packets to the receiver
                        * Retry sending the packet until acknowledgement is received
                        * Retry for at a max of MAX_ATTEMTS
                        * Appropriate print statements are provided for easy understanding
                        */
                        if (counter == MAX_ATTEMTS) {
                            timeout_flag = 1;
                            printf("File Not Sent - Connection Timeout\n");
                            break;
                        }
                    }

                    printf("packets - %ld \n", num_packets);
                    /* Transmit the data */
                    long int i, j;
                    for (i = 0; i < num_packets; i++) {
                        memset(&packet, 0, sizeof(packet));
                        packet.length = fread(packet.p, 1, BUFSIZE, fptr), counter = 0;
                        ACK = 0;
                        packet.ID = i;

                        for (j = 0; j < packet.length; j++) {
                            packet.p[j] ^= key;
                        }
                        /* Send the data/packets */
                        sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &remaddr, sizeof(remaddr));

                        if (timeout_flag) {
                            printf("File Not Sent - Connection Timeout\n");
                            break;
                        }

                        /* Receive acknowledgement from the client */
                        recvfrom(sockfd, &ACK, sizeof(ACK), 0, (struct sockaddr *) &remaddr, &addrlen);

                        /* Send the packets to the receiver
                         * Retry sending the packet until acknowledgement is received
                         * Retry for at a max of MAX_ATTEMTS
                         * Appropriate print statements are provided for easy understanding
                         */

                        while ((ACK != packet.ID)) {
                            sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &remaddr, sizeof(remaddr));
                            recvfrom(sockfd, &ACK, sizeof(ACK), 0, (struct sockaddr *) &remaddr, &addrlen);
                            printf("\t\tPacket no %ld	 dropped, Total - %ld \n", packet.ID, ++dropped);
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
                /* Set the timeout to zero */
                struct timeval timeout = {0,0};
                setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(struct timeval));
            }
            else {
                printf("Filename Not given \n");

            }

        }

        /* Implementation of DELETE command */
        else if ((strcmp(command, "delete") == 0)) {
            if (*filename == '\0') {
                printf("Filename Not given \n");

            }
            else {
                char cmd[10] = "rm ";
                strcat(cmd, filename);
                int sys_response = system(cmd);
                /*
                 * Perform the delete operation.
                 * Send appropriate message to the client
                 */
                if (sys_response) {
                    printf("Error in Deleting\n");
                    sendto(sockfd, "Error in Deleting", 17, 0, (struct sockaddr *) &remaddr, addrlen);
                }
                else {
                    printf("Successfully deleted %s\n", filename);
                    sendto(sockfd, "Successful", 18, 0, (struct sockaddr *) &remaddr, addrlen);
                }

            }

        }

        /* Implementation of LS command */
        else if (strcmp(command, "ls") == 0) {
            FILE *list;
            char filelist[200] = {0};
            system("ls >> _tmp.log");
            list = fopen("_tmp.log", "r");
            /*
            * Perform the ls operation.
            * dump the response into a dummy file
            */
            int file_response = fread(filelist, 1, 200, list);
            sendto(sockfd, filelist, file_response, 0, (struct sockaddr *) &remaddr, addrlen);
            system("rm _tmp.log");
        }

        /* Implementation of MD5SUM command(Optional) */
        else if (strcmp(command, "md") == 0) {
            if (*filename == '\0') {
                printf("Filename Not given \n");

            }
            else {
                char cmd[50] = "md5sum ";
                strcat(cmd, filename);
                printf("\n md5sum of %s on Server Side \n\n", filename);
                system(cmd);
            }
        }

        /* Implementation of EXIT command(Optional) */
        else if ((strcmp(command, "exit") == 0)) {
            printf("Closing the socket\n");
            close(sockfd);
            return 0;
        }

        /* Any other exceptions or errors */
        else if (recvlen > 0) {
            printf("Command Not Found \n");
        }

    }
}
