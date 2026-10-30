# IP forwarding
- get ethernet frame w src/dest mac addr (sr_handlepacket - sr_router.c)
- verify checksum (min length)
- change mac addr to next hop
- if need to forward, update TTL, checksum, and forward
- TTL expired -> ICMP time exceeded msg
- use routing table and longest prefix match for forwarding
- ARP cache for next hop, otherwise request and queue packet
- handle errors with ICMP msgs
- ip dest
  - own router's IPs:
    - ICMP echo + valid checksum -> echo reply
    - TCP/UDP payload -> port unreachable
    - else: ignore
  - other dest: forward as above (sr_send_packet - sr_vns_comm.c)

# ARP (address resolution)
- cache arp replies w 15sec timeout (done by provided cache)
- when fwding: check cache, miss ->
  - arp request to target IP 1/sec until reply (sr_arpcache_sweepreqs - sr_arpcache.c)
- 5 arp requests w/o reply -> ICMP dest host unreachable (sr_arpcache_sweepreqs - sr_arpcache.c)
- arp request -> arp reply IFF target is one of router's IPs
- cache arp reply IFF target is own router's IPs