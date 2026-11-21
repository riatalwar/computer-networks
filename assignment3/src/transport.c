/*
 * transport.c 
 *
 * EN.601.414/614: HW#3 (STCP)
 *
 * This file implements the STCP layer that sits between the
 * mysocket and network layers. You are required to fill in the STCP
 * functionality in this file. 
 *
 */


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "mysock.h"
#include "stcp_api.h"
#include "transport.h"


enum { 
    CSTATE_ESTABLISHED
    // closed
    // initialized (?)

};    /* obviously you should have more states */


/* this structure is global to a mysocket descriptor */
typedef struct
{
    bool_t done;    /* TRUE once connection is closed */

    int connection_state;   /* state of the connection (established, etc.) */
    tcp_seq initial_sequence_num;

    /* any other connection-wide global variables go here */
} context_t;


static void generate_initial_seq_num(context_t *ctx);
static void control_loop(mysocket_t sd, context_t *ctx);


/* initialise the transport layer, and start the main loop, handling
 * any data from the peer or the application.  this function should not
 * return until the connection is closed.
 */
void transport_init(mysocket_t sd, bool_t is_active)
{
    context_t *ctx;

    ctx = (context_t *) calloc(1, sizeof(context_t));
    assert(ctx);

    generate_initial_seq_num(ctx);

    /* XXX: you should send a SYN packet here if is_active, or wait for one
     * to arrive if !is_active.  after the handshake completes, unblock the
     * application with stcp_unblock_application(sd).  you may also use
     * this to communicate an error condition back to the application, e.g.
     * if connection fails; to do so, just set errno appropriately (e.g. to
     * ECONNREFUSED, etc.) before calling the function.
     */
    // void stcp_set_context(mysocket_t sd, const void *stcp_state);
    // void *stcp_get_context(mysocket_t my_sd);

    if (is_active) {
        // send syn packet
            // with seq num generated above
            // allocate header, empty payload
            // ssize_t stcp_network_send(mysocket_t sd, const void *src, size_t src_len, ...);
            // if return -1, failure

        // wait for syn ack
            // expect seq num + 1
            // get seqy num + 1
            // expect no data beyond header
            // -1 for failure
            // ssize_t stcp_network_recv(mysocket_t sd, void *dst, size_t max_len)

        // send ack
            // with seqy num + 1
            // no actual data in src, header only
            // ssize_t stcp_network_send(mysocket_t sd, const void *src, size_t src_len, ...);
            // if return -1, failure

    } else {
        // wait for syn
            // get synx num
            // expect no data beyond header
            // -1 for failure
            // ssize_t stcp_network_recv(mysocket_t sd, void *dst, size_t max_len)

        // send syn ack
            // with synx num + 1
            // no data to send, only SYN numbers in header
            // ssize_t stcp_network_send(mysocket_t sd, const void *src, size_t src_len, ...);
            // if return -1, failure

        // wait for ack
            // done, expect synack syny + 1
            // no data expected beyond header
            // -1 for failure
            // ssize_t stcp_network_recv(mysocket_t sd, void *dst, size_t max_len)

    }
    ctx->connection_state = CSTATE_ESTABLISHED;
    stcp_unblock_application(sd);

    control_loop(sd, ctx);

    /* do any cleanup here */
    free(ctx);
}


/* generate random initial sequence number for an STCP connection */
static void generate_initial_seq_num(context_t *ctx)
{
    assert(ctx);

#ifdef FIXED_INITNUM
    /* please don't change this! */
    ctx->initial_sequence_num = 1;
#else
    /* you have to fill this up */
    int num = rand() % 256;
    ctx->initial_sequence_num = num;
#endif
}


/* control_loop() is the main STCP loop; it repeatedly waits for one of the
 * following to happen:
 *   - incoming data from the peer
 *   - new data from the application (via mywrite())
 *   - the socket to be closed (via myclose())
 *   - a timeout
 */
static void control_loop(mysocket_t sd, context_t *ctx)
{
    assert(ctx);

    while (!ctx->done)
    {
        unsigned int event;

        /* see stcp_api.h or stcp_api.c for details of this function */
        /* XXX: you will need to change some of these arguments! */
        event = stcp_wait_for_event(sd, ANY_EVENT, NULL);

        /* check whether it was the network, app, or a close request */
        if (event & APP_DATA)
        {
            /* the application has requested that data be sent */
            /* see stcp_app_recv() */
            // dst is buffer to receive data
            // maxlen 536 bytes can be sent at once (excluding header size)
            // either get amount of data received, or -1 for failure
            // size_t stcp_app_recv(mysocket_t sd, void *dst, size_t max_len);

            // send out src data from dst, length is the output of app recv
            //ssize_t stcp_network_send(mysocket_t sd, const void *src, size_t src_len, ...);
            // if return -1, failure
            // else returns bytes sent
            // if bytes sent < expected, resend?
        }

        if (event & NETWORK_DATA) {
            /* received data from STCP peer */
            // dst is buffer to receive data
            // maxlen 536 bytes can be received at once (excluding header size)
            // either get amount of data received, or -1 for failure
            // ssize_t stcp_network_recv(mysocket_t sd, void *dst, size_t max_len)

            // send out src data from dst, length is the output of app recv
            // void stcp_app_send(mysocket_t sd, const void *src, size_t src_len);
            // assume success because void?

        }

        if (event & APP_CLOSE_REQUESTED) {

        }
        //void stcp_fin_received(mysocket_t sd);

        /* etc. */
    }
}


/**********************************************************************/
/* our_dprintf
 *
 * Send a formatted message to stdout.
 * 
 * format               A printf-style format string.
 *
 * This function is equivalent to a printf, but may be
 * changed to log errors to a file if desired.
 *
 * Calls to this function are generated by the dprintf amd
 * dperror macros in transport.h
 */
void our_dprintf(const char *format,...)
{
    va_list argptr;
    char buffer[1024];

    assert(format);
    va_start(argptr, format);
    vsnprintf(buffer, sizeof(buffer), format, argptr);
    va_end(argptr);
    fputs(buffer, stdout);
    fflush(stdout);
}



