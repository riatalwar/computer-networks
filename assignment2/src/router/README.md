# README for Assignment 2: Router

Name: Ria Talwar

JHED: rtalwar3

---

**DESCRIBE YOUR CODE AND DESIGN DECISIONS HERE**

This will be worth 10% of the assignment grade.

Some guiding questions:
- What files did you modify (and why)?
- What helper method did you write (and why)?
- What logic did you implement in each file/method?
- What problems or challenges did you encounter?

## sr_router.c
As instructed, we needed to implement the handlepacket functionality, which takes a packet, determines its validity, and handles it based on type. I created helper functions for the two separate cases in order to decompose the logic a bit. Additionally, factoring out some of the ICMP handling and message logic was helpful as it is reused in multiple situations, as we have several different messages established in the instructions.

I also implemented a longest prefix matching function just because it's an isolated check that can be done independently and helps decompose the complex functions.

## sr_protocol.h
I defined some constants here for the ICMP messages to make it clearer what was happening in the code (e.g. type of message sent).

## sr_arpcache.c
Here, I implemented the sweepreqs function, factoring out the request handling into a separate function. sweepreqs just needed to go through all requests, and the handler logic follows that illustrated in the header file. I also factored out the arp request sending logic, as it's an isolated component that can be handled separately to build and send the request. This can also reuse the send packet and icmp error functions that we previously defined in sr_router.c, showing how those helper functions can simplify the implementation for the homework.