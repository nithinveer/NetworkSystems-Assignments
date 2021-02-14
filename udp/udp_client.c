/*
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
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

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}


typedef struct {
    long int ID;
    char p[BUFSIZE];
    long int length;

}
        packet_info;

/*
// Function to convert website name to ip address
int hostname_to_ip(char * hostname, char * ip) {
  struct hostent * he;
  struct in_addr ** addr_list;
  int i;

  if ((he = gethostbyname(hostname)) == NULL) {
    // get the host info
    herror("gethostbyname");
    return 1;
  }

  addr_list = (struct in_addr ** ) he -> h_addr_list;

  for (i = 0; addr_list[i] != NULL; i++) {
    //Return the first one;
    strcpy(ip, inet_ntoa( * addr_list[i]));
    return 0;
  }

  return 1;
}
*/


int main(int argc, char **argv) {
    int sockfd, portno, n, m, recvlen;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE], inp[BUFSIZE];
    char command[10] = {
            0
    }, filename[20] = {
            0
    };
    packet_info packet;
    char key = 10;
    memset(&packet, 0, sizeof(packet));
    long int ACK = 0;
    struct timeval timeout = {
            0,
            700000
    };
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

    /* gethostbyname: get the server's DNS entry */
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

    connect(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));

    while (1) {
        // Command Menu
        printf("\n Enter any of the Following Commands  \n 1. get [file_name] \n 2. put [file_name] \n 3. delete [file_name] \n 4. ls \n 5. md \n 5. exit \n");

        memset(inp, 0, 50);
        memset(command, 0, 10);
        memset(filename, 0, 20);
        // Recieve the command
        scanf(" %[^\n]%*c", inp);
        sendto(sockfd, inp, strlen(inp), 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
        sscanf(inp, "%s %s", command, filename);
        // Put the file into the server
        if (((strcmp(command, "put") == 0)) && (*filename != '\0')) // Check if the filename is empty or not
        {
            long int num_packets = 0, dropped = 0;
            fptr = fopen(filename, "rb"); // open the file to read

            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout,
                       sizeof(struct timeval)); // Set the timeout for Acknowlegment

            if ((fptr == NULL)) // Check if the filename was right or wrong
            {
                printf("Wrong File name \n");
                // Send filesize as zero so that server will know that filename was wrong
                sendto(sockfd, &num_packets, sizeof(num_packets), 0, (struct sockaddr *) &serveraddr,
                       sizeof(serveraddr));
                recvfrom(sockfd, &num_packets, sizeof(num_packets), 0, (struct sockaddr *) &serveraddr,
                         &addrlen); // Recieve ACK for number of packets
            } else {

                int counter = 0, timeout_flag = 0;
                fseek(fptr, 0, SEEK_END);
                size_t file_size = ftell(fptr);
                fseek(fptr, 0, SEEK_SET);
                memset(&packet, 0, sizeof(packet));

                printf("File Size - %ld \n", file_size);
                num_packets = (file_size / BUFSIZE) + 1; // Number of packets to send

                // Send the number of packets to the reciever
                sendto(sockfd, &(num_packets), sizeof(num_packets), 0, (struct sockaddr *) &serveraddr,
                       sizeof(serveraddr));
                // wait for ack and if timeout then resend
                while (recvfrom(sockfd, &ACK, sizeof(ACK), 0, (struct sockaddr *) &serveraddr, &addrlen) < 0) {

                    sendto(sockfd, &(num_packets), sizeof(num_packets), 0, (struct sockaddr *) &serveraddr,
                           sizeof(serveraddr));
                    counter++;
                    // Even after 100 tries if the server fails then give connection timeout
                    if (counter == 100) {
                        timeout_flag = 1;
                        printf("File Not Sent - Connection Timeout\n");
                        break;
                    }
                }

                printf("packets - %ld \n", num_packets);
                // Send the data
                for (long int i = 0; i < num_packets; i++) {
                    memset(&packet, 0, sizeof(packet));
                    packet.length = fread(packet.p, 1, BUFSIZE, fptr), counter = 0;
                    ACK = 0;
                    packet.ID = i;
                    // Encrypt the data by Xoring
                    for (long int j = 0; j < packet.length; j++) {
                        packet.p[j] ^= key;
                    }

                    // Send the encrypted data
                    sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr));

                    // If timeout flag is sent then give connection timeout
                    if (timeout_flag) {
                        printf("File Not Sent - Connection Timeout\n");
                        break;
                    }
                    // recieve ACK from the reciever
                    recvfrom(sockfd, &ACK, sizeof(ACK), 0, (struct sockaddr *) &serveraddr, &addrlen);
                    // Keep re sending packets until ack for current packet is recieved
                    while ((ACK != packet.ID)) {
                        sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
                        recvfrom(sockfd, &ACK, sizeof(ACK), 0, (struct sockaddr *) &serveraddr, &addrlen);
                        printf("\t\tPacket no %ld dropped, Total - %ld \n", packet.ID, ++dropped);
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

        }

            // client
        else if ((strcmp(command, "get") == 0) && (*filename != '\0')) {

            long int num_packets = 0;
            // Recieve the number of packets of the file
            recvfrom(sockfd, &num_packets, sizeof(num_packets), 0, (struct sockaddr *) &serveraddr, &addrlen);
            //Server  ACK
            sendto(sockfd, &num_packets, sizeof(num_packets), 0, (struct sockaddr *) &serveraddr, addrlen);

            printf("Packets expected - %ld \n", num_packets);

            if (num_packets == 0) {
                printf("Empty File - Not Created \n"); // if 0 packets are recieved that means file can not be created
            } else if (num_packets > 0) {
                int total_bytes = 0;

                // Open the new file to write
                fptr = fopen(filename, "w");
                // Write the number of packets to file
                for (long int i = 0; i < num_packets; i++) {
                    // clear the recieving buffer before writing
                    memset(&packet, 0, sizeof(packet));
                    // recieve the packet
                    recvlen = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &serveraddr, &addrlen);

                    // send acknowledgement
                    sendto(sockfd, &(packet.ID), sizeof(packet.ID), 0, (struct sockaddr *) &serveraddr, addrlen);

                    if (packet.ID <
                        i) // If repeated paxket is recived then discard it and reduce the for loop count of recieved packet
                    {
                        i--;
                        printf("Same Packet %ld Discarded \n", packet.ID);
                    } else {
                        // Decrypt the data
                        for (long int j = 0; j < packet.length; j++) {
                            packet.p[j] ^= key;
                        }
                        // Write the data to the file
                        fwrite(packet.p, 1, packet.length, fptr);
                        printf("Packet number = %ld Recieved Size %ld  \n", packet.ID, packet.length);
                        total_bytes += packet.length;
                    }

                }
                for (int k = 0; k < 100; k++) {
                    struct timeval tout = {
                            0,
                            800
                    };
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

            // client - delete command
        else if ((strcmp(command, "delete") == 0) && (*filename != '\0')) {
            char ret[50];
            memset(ret, 0, 50);
            // Print the server reply for delete command
            recvfrom(sockfd, ret, 50, 0, (struct sockaddr *) &serveraddr, &addrlen);
            printf(" %s \n", ret);
        }
            // Get the md5sum for the file on the client side
        else if ((strcmp(command, "md") == 0) && (*filename != '\0')) {
            char cmd[50] = "md5sum ";
            strcat(cmd, filename);
            printf("\n md5sum of %s on Client Side \n\n\t", filename);
            system(cmd);
        }
            // get the list of files on the server side and print it
        else if (strcmp(command, "ls") == 0) {
            char filelist[200] = {
                    0
            };
            memset(filelist, 0, 200);
            recvfrom(sockfd, filelist, 200, 0, (struct sockaddr *) &serveraddr, &addrlen);
            printf("\n \nList of files is: \n%s \n", filelist);

        } else if (strcmp(command, "exit") == 0) {
            return 0;
        }
            // Error in the command or filename
        else {

            printf("Error (Wrong Command or Wrong filename) \n");

        }
    }

}