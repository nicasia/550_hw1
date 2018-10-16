Alexandra Okeson 1723308 amokeson@uw.edu
Ayse Berceste Dincer 1723315 abdincer@uw.edu
Nicasia Beebe-Wang 1723387 nbbwang@uw.edu

Notes about part B:
1) We are unable to connect to an IP address on attu.cs.washington.edu 
(possibly due to permissions issues), but had no problems on the cycle 
servers or our local machines.
2) If no arguments are passed to ./550server, we default to IP 127.0.0.1 and port 9000
3) The makefile uses flag "-std=gnu99" because cycle servers and local machines are running
an old version of Linux. We believe new versions of Linux will simply ignore the flags without
issues, but we're unable to test that. If you have issues with make, try removing that flag.