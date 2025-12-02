//---------------------------------------------------------------------
// Assignment : PA-04 Threads - UDP
// Date       : 12/1/2025
// Author     : Kyle Mirra      Akwasi Okyere
// File Name  : procurement.c
//---------------------------------------------------------------------

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "wrappers.h"
#include "message.h"

#define MAXFACTORIES    20

typedef struct sockaddr SA ;

/*-------------------------------------------------------*/
int main( int argc , char *argv[] )
{
    int     numFactories ,      // Total Number of Factory Threads
            activeFactories ,   // How many are still alive and manufacturing parts
            iters[ MAXFACTORIES+1 ] = {0} ,  // num Iterations completed by each Factory
            partsMade[ MAXFACTORIES+1 ] = {0} , totalItems = 0;

    struct timeval startTime, endTime; // starting and ending time
    long elapsedMS; // total time taken

    socklen_t addrLen;

    char  *myName = "Kyle Mirra and Akwasi Okyere" ; 
    printf("\nThis is procurement. ( by %s )\n\n", myName);
    fflush( stdout ) ;
    
    if ( argc < 4 )
    {
        printf("PROCUREMENT Usage: %s  <order_size> <FactoryServerIP>  <port>\n" , argv[0] );
        exit( -1 ) ;  
    }

    unsigned        orderSize  = atoi( argv[1] ) ;
    char	       *serverIP   = argv[2] ;
    unsigned short  port       = (unsigned short) atoi( argv[3] ) ;
 

    /* Set up local and remote sockets */
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0) {
        err_sys("Error creating socket");
    }

    // Prepare the server's socket address structure
    struct sockaddr_in srvrSkt;
    memset((void *) &srvrSkt, 0, sizeof(srvrSkt));
    srvrSkt.sin_family = AF_INET;
    srvrSkt.sin_port = htons(port);
    if (inet_pton(AF_INET, serverIP, (void *) &srvrSkt.sin_addr.s_addr) != 1) {
        err_sys("Invalid IP Address");
    }

    // Send the initial request to the Factory Server
    msgBuf  msg1;
    msg1.orderSize = htonl(orderSize);
    msg1.purpose = htonl(REQUEST_MSG);

    if (sendto(sd, (void *) &msg1, sizeof(msg1), 0, (SA *) &srvrSkt, sizeof(srvrSkt)) < 0) {
        err_sys("Error sending request message");
    }

    printf("Attempting factory server at %s : %hu\n", serverIP, port);
    printf("\nPROCUREMENT Sent this message to the FACTORY server: "  );
    printMsg( & msg1 );  puts("");

    /* Now, wait for order confirmation from the Factory server */
    msgBuf  msg2;
    printf ("\nPROCUREMENT is now waiting for order confirmation ...\n" );

    addrLen = sizeof(srvrSkt);
    if (recvfrom(sd, (void *) &msg2, sizeof(msg2), 0, (SA *) &srvrSkt, &addrLen) < 0) {
        err_sys("Error receiving order confirmation message");
    }

    printf("PROCUREMENT ( by %s ) received this from the FACTORY server: ", myName);
    printMsg( & msg2 );  puts("\n");

    gettimeofday(&startTime, NULL); 
    numFactories = ntohl(msg2.numFac);
    activeFactories = numFactories;

    // Monitor all Active Factory Lines & Collect Production Reports
    while ( activeFactories > 0 ) // wait for messages from sub-factories
    {
        // Receive the update message
        msgBuf updtMsg;
        if (recvfrom(sd, (void *) &updtMsg, sizeof(updtMsg), 0, (SA *) &srvrSkt, &addrLen) < 0) {
            err_sys("Error receiving update message");
        }

        int facID = ntohl(updtMsg.facID);
        int msgPartsMade = ntohl(updtMsg.partsMade);
        unsigned duration = ntohl(updtMsg.duration);
        msgPurpose_t purpose = ntohl(updtMsg.purpose);

       // Inspect the incoming message
        if (purpose == PRODUCTION_MSG) {
        iters[facID]++;
        partsMade[facID] += msgPartsMade;
        printf("PROCUREMENT ( by %s ): Factory #%-3d produced %-5d parts in %-5d milliSecs\n", myName, facID, msgPartsMade, duration);
        } 
        else if (purpose == COMPLETION_MSG) {
            activeFactories--;
            printf("PROCUREMENT ( by %s ): Factory #%-3d         COMPLETED its task\n", myName, facID);
        }
        else if (purpose == PROTOCOL_ERR){
            printf("PROCUREMENT ( by %s ): Received invalid msg ", myName);
            printMsg(&updtMsg); puts("");
            close(sd);
            exit(1);
        } else {
            printf("PROCUREMENT ( by %s ): Received an invalid message\n", myName);
            close(sd);
            exit(1);
        }
    } 
    // Get ending time and calculate total time
    gettimeofday(&endTime, NULL);
    elapsedMS = (endTime.tv_sec - startTime.tv_sec) * 1000L +
            (endTime.tv_usec - startTime.tv_usec) / 1000L;

    // Print the summary report
    totalItems  = 0 ;
    printf("\n\n****** PROCUREMENT ( by %s ) Summary Report ******\n", myName);
    printf("\tSub-Factory\tParts Made\tIterations\n");

    for (int i = 1; i <= numFactories; i++) {
        printf("\t\t%-3d\t\t%-5d\t\t%-3d\n", i, partsMade[i], iters[i]);
        totalItems += partsMade[i];
    }

    printf("=========================================================\n") ;

    printf("Grand total parts made = %5d vs order size of %5d\n", totalItems, orderSize);
    printf("Order-to-Completion time = %ld milliSeconds\n", elapsedMS);

    printf( "\n>>> PROCUREMENT Terminated\n");

    return 0 ;
}
