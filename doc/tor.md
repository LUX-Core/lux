TOR SUPPORT IN LUX
=======================

It is possible to run LUX as a Tor hidden service, and connect to such services.

The following directions assume you have a Tor proxy running on port 9050. Many
distributions default to having a SOCKS proxy listening on port 9050, but others
may not. In particular, the Tor Browser Bundle defaults to listening on a random
port. See [Tor Project FAQ:TBBSocksPort](https://www.torproject.org/docs/faq.html.en#TBBSocksPort)
for how to properly configure Tor.


Run LUX behind a Tor proxy
----------------------------------

The first step is running LUX behind a Tor proxy. This will already make all
outgoing connections be anonymized, but more is possible.
```
-proxy=ip:port  Set the proxy server. If SOCKS5 is selected (default), this proxy
                server will be used to try to reach .onion addresses as well.

-onion=ip:port  Set the proxy server to use for tor hidden services. You do not
                need to set this if it's the same as -proxy. You can use -noonion
                to explicitly disable access to hidden service.

-listen         When using -proxy, listening is disabled by default. If you want
                to run a hidden service (see next section), you'll need to enable
                it explicitly.

-connect=X      When behind a Tor proxy, you can specify .onion addresses instead
-addnode=X      of IP addresses or hostnames in these parameters. It requires
-seednode=X     SOCKS5. In Tor mode, such addresses can also be exchanged with
                other P2P nodes.

-onlynet=tor    Only connect to .onion nodes and drop IPv4/6 connections.
```

An example how to start the client if the Tor proxy is running on local host on
port 9050 and only allows .onion nodes to connect:
```
./luxd -onion=127.0.0.1:9050 -onlynet=tor -listen=0 -addnode=dnetzj6l4cvo2fxy.onion:989
```

In a typical situation, this suffices to run behind a Tor proxy:
```
./luxd -proxy=127.0.0.1:9050
```

Run a LUX hidden server
-------------------------------

If you configure your Tor system accordingly, it is possible to make your node also
reachable from the Tor network. Add these lines to your /etc/tor/torrc (or equivalent
config file):
```
ClientOnly 1
SOCKSPort 9050
SOCKSPolicy accept 127.0.0.1/8
Log notice file /var/log/tor/notices.log
ControlPort 9051
HiddenServiceDir /var/lib/tor/dnet/
HiddenServicePort 989 127.0.0.1:28666
HiddenServiceStatistics 0
ORPort 9001
LongLivedPorts 989
ExitPolicy reject *:*
DisableDebuggerAttachment 0
NumEntryGuards 8
```

The directory can be different of course, but (both) port numbers should be equal to
your luxd's P2P listen port (28666 by default).
```
-externalip=X   You can tell lux about its publicly reachable address using
                this option, and this can be a .onion address. Given the above
                configuration, you can find your onion address in
                /var/lib/tor/lux-service/hostname. Onion addresses are given
                preference for your node to advertize itself with, for connections
                coming from unroutable addresses (such as 127.0.0.1, where the
                Tor proxy typically runs).

-listen         You'll need to enable listening for incoming connections, as this
                is off by default behind a proxy.

-discover       When -externalip is specified, no attempt is made to discover local
                IPv4 or IPv6 addresses. If you want to run a dual stack, reachable
                from both Tor and IPv4 (or IPv6), you'll need to either pass your
                other addresses using -externalip, or explicitly enable -discover.
                Note that both addresses of a dual-stack system may be easily
                linkable using traffic analysis.
```

In a typical situation, where you're only reachable via Tor, this should suffice:
```
./luxd -proxy=127.0.0.1:9050 -externalip=dnetzj6l4cvo2fxy.onion:989 -listen
```

(obviously, replace the Onion address with your own). If you don't care too much
about hiding your node, and want to be reachable on IPv4 as well, additionally
specify:
```
./luxd ... -discover
```

and open port 28666 on your firewall (or use -upnp).

If you only want to use Tor to reach onion addresses, but not use it as a proxy
for normal IPv4/IPv6 communication, use:
```
./luxd -onion=127.0.0.1:9050 -externalip=dnetzj6l4cvo2fxy.onion:989 -discover
```

List of known LUX Tor relays
------------------------------------
```
y5kcscnhpygvvnjn.onion:989
5bmhtjvn2jvwpiej.onion:989
pyfdxkazur3iib7y.onion:989
ok3ym5zy6m5klimk.onion:989
i6vpvzk2jxuqqs5f.onion:989
bgdhpb76fkbw5fmg.onion:989
gtlqzb5zbws5di7g.onion:989
f7j2m26rptm5f7af.onion:989
dnetzj6l4cvo2fxy.onion:989
s3v3n7xhqafg6sb7.onion:989
```
