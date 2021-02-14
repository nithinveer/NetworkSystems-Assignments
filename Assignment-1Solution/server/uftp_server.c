/*
 * udpserver.c - A simple UDP echo server
 * usage: udpserver <port>
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

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(1);
}


typedef struct {
    long int ID;
    char p[BUFSIZE];
    long int length;
}
        packet_info;

int main(int argc, char **argv) {
    int sockfd; /* socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr, remaddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    socklen_t addrlen = sizeof(remaddr);
    struct hostent *hostp; /* client host info */
    char buf[BUFSIZE]; /* message buf */
    char *hostaddrp; /* dotted decimal host addr string */
    int optval; /* flag value for setsockopt */
    int n, recvlen; /* message byte size */
    char filename[20], command[10];
    FILE *fptr;
    struct timeval timeout = {
            0,
            500000
    };
    long int ACK = 0;
    packet_info packet;
    char key = 10;

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
    while (1) {
        printf("\n");
        printf("Waiting on port %d\n", portno);

        recvlen = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &remaddr, &addrlen);
        //printf("received %d bytes\n", recvlen);
        if (recvlen > 0) {
            buf[recvlen] = 0;
            printf("received message: \"%s\"\n", buf);
        }

        memset(command, 0, 10);
        memset(filename, 0, 20);
        sscanf(buf, "%s %s", command, filename);

        if (((strcmp(command, "put") == 0))) {

            if (*filename != '\0') {
                long int num_packets = 0;
                // Recieve the number of packets of the file
                recvfrom(sockfd, &num_packets, sizeof(num_packets), 0, (struct sockaddr *) &remaddr, &addrlen);
                //Server  ACK
                sendto(sockfd, &num_packets, sizeof(num_packets), 0, (struct sockaddr *) &remaddr, addrlen);

                printf("Packets - %ld \n", num_packets);
                if (num_packets == 0) // if 0 packets are recieved that means file can not be created
                {
                    printf("Empty File - Not Created \n");
                } else if (num_packets > 0) {
                    int total_bytes = 0;
                    long int i, j;
                    // Open the new file to write
                    fptr = fopen(filename, "w");
                    // Write the number of packets to file
                    for (i = 0; i < num_packets; i++) {
                        // clear the recieving buffer before writing
                        memset(&packet, 0, sizeof(packet));
                        // recieve the packet
                        recvlen = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &remaddr, &addrlen);

                        // send acknowledgement
                        sendto(sockfd, &(packet.ID), sizeof(packet.ID), 0, (struct sockaddr *) &remaddr, addrlen);

                        if (packet.ID <
                            i) // If repeated paxket is recived then discard it and reduce the for loop count of recieved packet
                        {
                            i--;
                            printf("Same Packet %ld Discarded \n", packet.ID);
                        } else {
                            // Decrypt the data
                            for ( j = 0; j < packet.length; j++) {
                                packet.p[j] ^= key;
                            }
                            // Write the data to the file
                            fwrite(packet.p, 1, packet.length, fptr);
                            printf("Packet number = %ld Recieved Size- %ld  \n", packet.ID, packet.length);
                            total_bytes += packet.length;
                        }

                    }
                    printf("Total bytes - %d\n", total_bytes);
                    fclose(fptr);
                }

            } else {
                printf("Filename Not given \n");

            }

        }

            // server
        else if ((strcmp(command, "get") == 0)) {

            if (*filename != '\0') // Check if the filename is empty or not
            {
                long int num_packets = 0, dropped = 0;
                fptr = fopen(filename, "rb"); // open the file to read

                setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout,
                           sizeof(struct timeval)); // Set the timeout for Acknowlegment

                if ((fptr == NULL)) // Check if the filename was right or wrong
                {
                    printf("Wrong File name \n");
                    // Send filesize as zero so that server will know that filename was wrong
                    sendto(sockfd, &num_packets, sizeof(num_packets), 0, (struct sockaddr *) &remaddr, sizeof(remaddr));
                    recvfrom(sockfd, &ACK, sizeof(ACK), 0, (struct sockaddr *) &remaddr,
                             &addrlen); // Recieve ACK for number of packet

                } else {
                    int counter = 0, timeout_flag = 0;
                    fseek(fptr, 0, SEEK_END);
                    size_t file_size = ftell(fptr);
                    fseek(fptr, 0, SEEK_SET);
                    memset(&packet, 0, sizeof(packet));

                    printf("File Size - %ld \n", file_size);
                    num_packets = (file_size / BUFSIZE) + 1; // Number of packets to send

                    // Send the number of packets to the reciever
                    sendto(sockfd, &(num_packets), sizeof(num_packets), 0, (struct sockaddr *) &remaddr,
                           sizeof(remaddr));
                    // wait for ack and if timeout then resend
                    while (recvfrom(sockfd, &ACK, sizeof(ACK), 0, (struct sockaddr *) &remaddr, &addrlen) < 0) {

                        sendto(sockfd, &(num_packets), sizeof(num_packets), 0, (struct sockaddr *) &remaddr,
                               sizeof(remaddr));
                        counter++;
                        // Even after 100 tries if the reciever fails then give connection timeout
                        if (counter == 100) {
                            timeout_flag = 1;
                            printf("File Not Sent - Connection Timeout\n");
                            break;
                        }
                    }

                    printf("packets - %ld \n", num_packets);
                    // Send the data
                    long int i, j;
                    for (i = 0; i < num_packets; i++) {
                        memset(&packet, 0, sizeof(packet));
                        packet.length = fread(packet.p, 1, BUFSIZE, fptr), counter = 0;
                        ACK = 0;
                        packet.ID = i;
                        // Encrypt the data by Xoring
                        for (j = 0; j < packet.length; j++) {
                            packet.p[j] ^= key;
                        }
                        // Send the encrypted data
                        sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &remaddr, sizeof(remaddr));

                        if (timeout_flag) {
                            printf("File Not Sent - Connection Timeout\n");
                            break;
                        }
                        // recieve ACK from the reciever
                        recvfrom(sockfd, &ACK, sizeof(ACK), 0, (struct sockaddr *) &remaddr, &addrlen);
                        while ((ACK != packet.ID)) {
                            // Keep re sending packets until ack for current packet is recieved
                            sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &remaddr, sizeof(remaddr));
                            recvfrom(sockfd, &ACK, sizeof(ACK), 0, (struct sockaddr *) &remaddr, &addrlen);
                            printf("\t\tPacket no %ld	 dropped, Total - %ld \n", packet.ID, ++dropped);
                            counter++;
                            if (counter == 100) {
                                timeout_flag = 1;
                                break;
                            }
                        }

                        printf("ACK for Packet, i = %ld ACK = %ld recieved \n", i, ACK);

                        if (i == num_packets - 1)
                            printf("File Sent \n");

                    }

                }
                // Remove the timeout for the recvfrom
                struct timeval timeout = {
                        0,
                        0
                };
                setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(struct timeval));
            } else {
                printf("Filename Not given \n");
                //sendto(udp_socket, "Filename Not Given", 18, 0, (struct sockaddr *)&remaddr, addrlen);

            }

        }

            // server - delete command
        else if ((strcmp(command, "delete") == 0)) {
            if (*filename == '\0') {
                printf("Filename Not given \n");
                //sendto(sockfd, "Filename Not Given", 18, 0, (struct sockaddr *)&remaddr, addrlen);
            } else {
                char cmd[10] = "rm ";
                strcat(cmd, filename);
                int ret = system(cmd);
                if (ret) {
                    printf("Error in Deleting\n");
                    sendto(sockfd, "Error in Deleting", 17, 0, (struct sockaddr *) &remaddr, addrlen);
                } else {
                    printf("Successfully deleted %s\n", filename);
                    sendto(sockfd, "Successful", 18, 0, (struct sockaddr *) &remaddr, addrlen);
                }

            }

        }

            // get the list of files on the server side and print it
        else if (strcmp(command, "ls") == 0) {
            FILE *list;
            char filelist[200] = {
                    0
            };
            system("ls >> a.log");
            list = fopen("a.log", "r");
            int rec = fread(filelist, 1, 200, list);
            sendto(sockfd, filelist, rec, 0, (struct sockaddr *) &remaddr, addrlen);

            system("rm a.log");
        }

            // Get the md5sum for the file on the server side
        else if (strcmp(command, "md") == 0) {
            if (*filename == '\0') {
                printf("Filename Not given \n");

            } else {
                char cmd[50] = "md5sum ";
                strcat(cmd, filename);
                printf("\n md5sum of %s on Server Side \n\n", filename);
                system(cmd);
            }
        } else if ((strcmp(command, "exit") == 0)) {
            //sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&remaddr, addrlen);
            printf("Closing the socket\n");
            close(sockfd);
            return 0;
        }

            // Error in the command or filename
        else if (recvlen > 0) {

            printf("Command Not Found \n");
            //sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&remaddr, addrlen);
        }

    }
}
