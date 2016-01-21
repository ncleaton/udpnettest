# udpnettest
UDP based network performance tester

This is code for testing an IP network in one direction only, using a stream
of UDP packets. The packets contain sequence numbers and random data, and the
detail of exactly when to send each packet and the size it should have are
read in from a data file at startup. A sample data file is included.

Two programs are provided, udp-sender to read the data file and send the packets,
and udp-receiver to receive the packets and log the timing and sequence number
of each one.

Tested only on Ubuntu Linux, where it can be built simply by typing "make".

An example of use, using UDP port 25001:

First, on receiving side:

    ./udp-receiver 25001 &gt;got-packets.log &amp;

The, on the sending side:

    gzip -dc sample-packets.gz | ./udp-sender TARGET_HOSTNAME 25001

When udp-sender exits (the test in sample-packets.gz takes about 7.5 minutes)
hit Ctrl-C on the receiving side, then compare got-packets.log with the contents
of sample-packets.gz.
