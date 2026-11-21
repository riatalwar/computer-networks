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
    CSTATE_ESTABLISHED,
    CSTATE_SENT_CLOSE,  // sent close, not acked
    CSTATE_ACKED_CLOSE, // sent close, got ack
    CSTATE_RECV_CLOSE,  // recv close, not sent
    CSTATE_RECV_SENT_CLOSE, // recv close, sent but not acked
    CSTATE_CLOSED       // recv close, sent and got ack
};    /* obviously you should have more states */


/* this structure is global to a mysocket descriptor */
typedef struct
{
    bool_t done;    /* TRUE once connection is closed */

    int connection_state;   /* state of the connection (established, etc.) */
    tcp_seq initial_sequence_num;

    /* any other connection-wide global variables go here */
    tcp_seq current_sequence_num;       // next seq num to send
    tcp_seq next_expected_sequence_num; // expected from peer
    tcp_seq last_acked_sequence_num;    // last acked by peer
    size_t peer_window;                 // peer window sz
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
    ctx->current_sequence_num = ctx->initial_sequence_num;

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
        STCPHeader *syn = (STCPHeader *) calloc(1, sizeof(STCPHeader));
        assert(syn);
        syn->th_seq = htonl(ctx->initial_sequence_num);
        syn->th_flags = TH_SYN;
        syn->th_off = sizeof(STCPHeader) / 4;
        syn->th_win = htons(3072);
        ssize_t sent = stcp_network_send(sd, syn, sizeof(STCPHeader), NULL);
        if (sent == -1 || (unsigned long) sent < sizeof(STCPHeader)) {
            errno = ECONNREFUSED;
            stcp_unblock_application(sd);
            free(syn);
            free(ctx);
            return;
        }
        free(syn);

        // wait for syn ack
        STCPHeader *syn_ack = (STCPHeader *) calloc(1, sizeof(STCPHeader));
        assert(syn_ack);
        ssize_t recv = stcp_network_recv(sd, syn_ack, sizeof(STCPHeader));
        if (recv == -1 || (unsigned long) recv < sizeof(STCPHeader)) {
            errno = ECONNREFUSED;
            stcp_unblock_application(sd);
            free(syn_ack);
            free(ctx);
            return;
        }
        // validate flags and seq num
        tcp_seq syn_num = ntohl(syn_ack->th_seq);
        if (syn_ack->th_flags != (TH_SYN | TH_ACK) || ntohl(syn_ack->th_ack) != ctx->initial_sequence_num + 1) {
            errno = ECONNREFUSED;
            stcp_unblock_application(sd);
            free(syn_ack);
            free(ctx);
            return;
        }
        ctx->current_sequence_num++;
        ctx->last_acked_sequence_num = ntohl(syn_ack->th_ack);
        ctx->peer_window = ntohs(syn_ack->th_win);
        free(syn_ack);

        // send ack
        STCPHeader *ack = (STCPHeader *) calloc(1, sizeof(STCPHeader));
        assert(ack);
        ack->th_seq = htonl(ctx->current_sequence_num);
        ack->th_ack = htonl(syn_num + 1);
        ack->th_flags = TH_ACK;
        ack->th_off = sizeof(STCPHeader) / 4;
        ack->th_win = htons(3072);
        ssize_t sent_ack = stcp_network_send(sd, ack, sizeof(STCPHeader), NULL);
        if (sent_ack == -1 || (unsigned long) sent_ack < sizeof(STCPHeader)) {
            errno = ECONNREFUSED;
            stcp_unblock_application(sd);
            free(ack);
            free(ctx);
            return;
        }
        free(ack);
        ctx->next_expected_sequence_num = syn_num + 1;

    } else {
        // wait for syn
        STCPHeader *syn = (STCPHeader *) calloc(1, sizeof(STCPHeader));
        assert(syn);
        ssize_t recv = stcp_network_recv(sd, syn, sizeof(STCPHeader));
        if (recv == -1 || (unsigned long) recv < sizeof(STCPHeader)) {
            errno = ECONNREFUSED;
            stcp_unblock_application(sd);
            free(syn);
            free(ctx);
            return;
        }
        tcp_seq syn_num = ntohl(syn->th_seq);
        ctx->peer_window = ntohs(syn->th_win);
        free(syn);

        // send syn ack
        STCPHeader *syn_ack = (STCPHeader *) calloc(1, sizeof(STCPHeader));
        assert(syn_ack);
        syn_ack->th_seq = htonl(ctx->initial_sequence_num);
        syn_ack->th_ack = htonl(syn_num + 1);
        syn_ack->th_flags = TH_SYN | TH_ACK;
        syn_ack->th_off = sizeof(STCPHeader) / 4;
        syn_ack->th_win = htons(3072);
        ssize_t sent = stcp_network_send(sd, syn_ack, sizeof(STCPHeader), NULL);
        if (sent == -1 || (unsigned long) sent < sizeof(STCPHeader)) {
            errno = ECONNREFUSED;
            stcp_unblock_application(sd);
            free(syn_ack);
            free(ctx);
            return;
        }
        free(syn_ack);
        ctx->next_expected_sequence_num = syn_num + 1;

        // wait for ack
        STCPHeader *ack = (STCPHeader *) calloc(1, sizeof(STCPHeader));
        assert(ack);
        ssize_t recv_ack = stcp_network_recv(sd, ack, sizeof(STCPHeader));
        if (recv_ack == -1 || (unsigned long) recv_ack < sizeof(STCPHeader)) {
            errno = ECONNREFUSED;
            stcp_unblock_application(sd);
            free(ack);
            free(ctx);
            return;
        }
        if (ack->th_flags != TH_ACK || ntohl(ack->th_ack) != ctx->initial_sequence_num + 1) {
            errno = ECONNREFUSED;
            stcp_unblock_application(sd);
            free(ack);
            free(ctx);
            return;
        }
        ctx->current_sequence_num++;
        ctx->last_acked_sequence_num = ntohl(ack->th_ack);
        ctx->peer_window = ntohs(ack->th_win);
        free(ack);
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
        // basically check if amt of data in flight exceeds peer window sz
        // if that's the case we can't send more data atm
        stcp_event_type_t e = ANY_EVENT;
        tcp_seq in_flight = ctx->current_sequence_num - ctx->last_acked_sequence_num;
        size_t window = MIN(ctx->peer_window, 3072);
        if (in_flight >= window) {
            e = RECV_ONLY;
        }
        event = stcp_wait_for_event(sd, e, NULL);

        /* check whether it was the network, app, or a close request */
        if (event & APP_DATA)
        {
            /* the application has requested that data be sent */
            // read in app data
            char *app_buf = (char *) calloc(1, STCP_MSS);
            assert(app_buf);
            size_t app_len = stcp_app_recv(sd, app_buf, MIN(STCP_MSS, window - in_flight));

            // set header (current seq num, next expected + ack, header offset, window sz)
            STCPHeader *data_hdr = (STCPHeader *) calloc(1, sizeof(STCPHeader));
            assert(data_hdr);
            data_hdr->th_seq = htonl(ctx->current_sequence_num);
            data_hdr->th_ack = htonl(ctx->next_expected_sequence_num);
            data_hdr->th_flags = TH_ACK;
            data_hdr->th_off = sizeof(STCPHeader) / 4;
            data_hdr->th_win = htons(3072);

            // ensure data is sent
            ssize_t sent = stcp_network_send(sd, data_hdr, sizeof(STCPHeader),
                                 app_buf, app_len, NULL);
            if (sent == -1 || (size_t) sent < sizeof(STCPHeader) + app_len) {
                // TODO: handle error
            }
            free(data_hdr);
            free(app_buf);
            ctx->current_sequence_num += app_len;
        }

        if (event & NETWORK_DATA) {
            /* received data from STCP peer */
            // read in network data
            char *net_buf = (char *) calloc(1, sizeof(STCPHeader) + STCP_MSS);
            assert(net_buf);
            ssize_t recvd = stcp_network_recv(sd, net_buf, sizeof(STCPHeader) + STCP_MSS);
            if (recvd == -1 || (size_t) recvd < sizeof(STCPHeader)) {
                // TODO: handle error / drop
            }

            // compute header vs payload segments
            STCPHeader *hdr = (STCPHeader *) net_buf;
            size_t data_start = TCP_DATA_START(net_buf);
            size_t payload_len = recvd - data_start;
            tcp_seq seq = ntohl(hdr->th_seq);
            ctx->peer_window = ntohs(hdr->th_win);

            // ensure in order delivery with matching next expected seq num
            if (payload_len > 0 && seq == ctx->next_expected_sequence_num) {
                stcp_app_send(sd, net_buf + data_start, payload_len);
                ctx->next_expected_sequence_num += payload_len;
            }

            // handle acks by updating last acked
            if (hdr->th_flags & TH_ACK) {
                tcp_seq ack = ntohl(hdr->th_ack);
                if (ack > ctx->last_acked_sequence_num) {
                    ctx->last_acked_sequence_num = ack;
                }
                // check for FIN ack
                if (ack == ctx->current_sequence_num) {
                    if (ctx->connection_state == CSTATE_SENT_CLOSE) {
                        ctx->connection_state = CSTATE_ACKED_CLOSE;
                    } else if (ctx->connection_state == CSTATE_RECV_SENT_CLOSE) {
                        ctx->connection_state = CSTATE_CLOSED;
                        ctx->done = TRUE;
                    }
                }

            }

            // check for FIN
            bool_t fin_recvd = FALSE;
            if ((hdr->th_flags & TH_FIN) && seq + payload_len == ctx->next_expected_sequence_num) {
                fin_recvd = TRUE;
                ctx->next_expected_sequence_num++;
                stcp_fin_received(sd);
                if (ctx->connection_state == CSTATE_ESTABLISHED) {
                    ctx->connection_state = CSTATE_RECV_CLOSE;
                } else if (ctx->connection_state == CSTATE_SENT_CLOSE) {
                    ctx->connection_state = CSTATE_RECV_SENT_CLOSE;
                } else if (ctx->connection_state == CSTATE_ACKED_CLOSE) {
                    ctx->connection_state = CSTATE_CLOSED;
                    ctx->done = TRUE;
                }
            }
            free(net_buf);
        
            // ack packets with data or FIN packets
            if (payload_len > 0 || fin_recvd) {
                // assemble ack header with current seq num and next expected
                STCPHeader *ack_hdr = (STCPHeader *) calloc(1, sizeof(STCPHeader));
                assert(ack_hdr);
                ack_hdr->th_seq = htonl(ctx->current_sequence_num);
                ack_hdr->th_ack = htonl(ctx->next_expected_sequence_num);
                ack_hdr->th_flags = TH_ACK;
                ack_hdr->th_off = sizeof(STCPHeader) / 4;
                ack_hdr->th_win = htons(3072);

                // send ack header
                ssize_t ack_sent = stcp_network_send(sd, ack_hdr, sizeof(STCPHeader), NULL);
                if (ack_sent == -1 || (size_t) ack_sent < sizeof(STCPHeader)) {
                    // TODO: handle error
                }
                free(ack_hdr);
            }
        }

        if (event & APP_CLOSE_REQUESTED) {
            // allocate fin header with FIN flag, otherwise same as usual
            STCPHeader *fin_hdr = (STCPHeader *) calloc(1, sizeof(STCPHeader));
            assert(fin_hdr);
            fin_hdr->th_seq = htonl(ctx->current_sequence_num);
            fin_hdr->th_ack = htonl(ctx->next_expected_sequence_num);
            fin_hdr->th_flags = TH_FIN | TH_ACK;
            fin_hdr->th_off = sizeof(STCPHeader) / 4;
            fin_hdr->th_win = htons(3072);

            // send the FIN header
            ssize_t fin_sent = stcp_network_send(sd, fin_hdr, sizeof(STCPHeader), NULL);
            if (fin_sent == -1 || (size_t) fin_sent < sizeof(STCPHeader)) {
                // TODO: handle error
            }
            free(fin_hdr);
            ctx->current_sequence_num++;
            if (ctx->connection_state == CSTATE_ESTABLISHED) {
                    ctx->connection_state = CSTATE_SENT_CLOSE;
            } else if (ctx->connection_state == CSTATE_RECV_CLOSE) {
                ctx->connection_state = CSTATE_RECV_SENT_CLOSE;
            }
        }

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



