/**********************************************************************
 * file:  sr_router.c
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    /* REQUIRES */
    assert(sr);

    /* Initialize cache and cache cleanup thread */
    sr_arpcache_init(&(sr->cache));

    pthread_attr_init(&(sr->attr));
    pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_t thread;

    pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);

    /* Add initialization code here! */

} /* -- sr_init -- */

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
  /* REQUIRES */
  assert(sr);
  assert(packet);
  assert(interface);

  printf("*** -> Received packet of length %d \n",len);

  /* fill in code here */
  if (len < sizeof(sr_ethernet_hdr_t)) {
    // drop, invalid len
    return;
  }

  struct sr_if* intfc = sr_get_interface(sr, interface);
  if (intfc == NULL) {
    return; // drop bad interface
  }
  
  uint16_t type = ethertype(packet);
  if (type == ethertype_arp) {
    if (len < sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t)) {
      return; // drop packet, invalid size
    }
    handle_arp(sr, packet, len, intfc);
  } else if (type == ethertype_ip) {
    if (len < sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t)) {
      return; // drop packet, invalid size
    }
    handle_ip(sr, packet, len, intfc);
  } else {
    return; // drop unknown types
  }
} /* end sr_handlepacket */

void handle_arp(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        struct sr_if* interface/* lent */) {
  sr_ethernet_hdr_t *req_eth = (sr_ethernet_hdr_t *)packet;
  sr_arp_hdr_t *req_arp = (sr_arp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));

  // reply handling
  if (ntohs(req_arp->ar_op) == arp_op_reply) {
    handle_arp_reply(sr, interface, req_arp);
    return;
  } else if (ntohs(req_arp->ar_op) != arp_op_request) {
    return;
  }

  if (req_arp->ar_tip != interface->ip) {
    return;
  }

  unsigned int reply_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);
  uint8_t *reply = malloc(reply_len);

  sr_ethernet_hdr_t *reply_eth = (sr_ethernet_hdr_t *)reply;
  sr_arp_hdr_t *reply_arp = (sr_arp_hdr_t *)(reply + sizeof(sr_ethernet_hdr_t));

  /* fill 3 ethernet fields */
  memcpy(reply_eth->ether_shost, interface->addr, ETHER_ADDR_LEN);
  memcpy(reply_eth->ether_dhost, req_eth->ether_shost, ETHER_ADDR_LEN);
  reply_eth->ether_type = req_eth->ether_type;
  /* fill 9 arp fields */
  reply_arp->ar_hrd = req_arp->ar_hrd;
  reply_arp->ar_pro = req_arp->ar_pro;
  reply_arp->ar_hln = req_arp->ar_hln;
  reply_arp->ar_pln = req_arp->ar_pln;
  reply_arp->ar_op = htons(arp_op_reply);
  memcpy(reply_arp->ar_sha, interface->addr, ETHER_ADDR_LEN);
  reply_arp->ar_sip = interface->ip;
  memcpy(reply_arp->ar_tha, req_arp->ar_sha, ETHER_ADDR_LEN);
  reply_arp->ar_tip = req_arp->ar_sip;

  sr_send_packet(sr, reply, reply_len, interface->name);
  free(reply);
}

void handle_arp_reply(struct sr_instance* sr, struct sr_if* interface, sr_arp_hdr_t *req_arp) {
  if (req_arp->ar_tip != interface->ip) {
    return;
  }
  // insert ip, mac pair to cache
  struct sr_arpreq *req = sr_arpcache_insert(&sr->cache, req_arp->ar_sha, req_arp->ar_sip);
  if (!req) return;

  // loop thru packets
  struct sr_packet *p;
  for (p = req->packets; p; p = p->next) {
    sr_ethernet_hdr_t *eth = (sr_ethernet_hdr_t *)p->buf;
    struct sr_if *out = sr_get_interface(sr, p->iface);
    // mac addr
    memcpy(eth->ether_dhost, req_arp->ar_sha, ETHER_ADDR_LEN);
    memcpy(eth->ether_shost, out->addr, ETHER_ADDR_LEN);
    sr_send_packet(sr, p->buf, p->len, p->iface); // send
  }
  sr_arpreq_destroy(&sr->cache, req);
}

void handle_ip(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        struct sr_if* interface/* lent */) {
  sr_ip_hdr_t *ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
  unsigned int hdr_len = ip_hdr->ip_hl * 4;

  if (ip_hdr->ip_v != 4 || hdr_len < sizeof(sr_ip_hdr_t)) {
    return; // invalid vers or header len
  }
  if (len < sizeof(sr_ethernet_hdr_t) + ntohs(ip_hdr->ip_len) ||
      ntohs(ip_hdr->ip_len) < hdr_len) {
    return; // invalid size
  }

  uint16_t received_sum = ip_hdr->ip_sum;
  ip_hdr->ip_sum = 0;
  if (cksum(ip_hdr, hdr_len) != received_sum) {
    return; // invalid checksum
  }
  ip_hdr->ip_sum = received_sum;

  if (get_interface_from_ip(sr, ip_hdr->ip_dst)) {

// find best prefix match for ip address
struct sr_rt *longest_prefix_match(struct sr_instance* sr, uint32_t ip) {
  struct sr_rt *best = NULL;
  struct sr_rt *rt;
  // iterate over routing table
  for (rt = sr->routing_table; rt; rt = rt->next) {
    // test bitwise & ip/mask for match with dest
    uint32_t mask = rt->mask.s_addr;
    if ((ip & mask) != (rt->dest.s_addr & mask)) {
      continue;
    }
    // if match, check if longer than current match
    if (!best || ntohl(mask) > ntohl(best->mask.s_addr)) {
      best = rt;
    }
  }
  return best;
}
  } else {
    // TODO: forward: TTL, lpm, send_ip_packet
  }
}

/* Add any additional helper methods here & don't forget to also declare
them in sr_router.h.

If you use any of these methods in sr_arpcache.c, you must also forward declare
them in sr_arpcache.h to avoid circular dependencies. Since sr_router
already imports sr_arpcache.h, sr_arpcache cannot import sr_router.h -KM */
