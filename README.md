# English version
Polish below
## What is this?
This project defines a wrapper on DNS resolution, allowing the user to
define "fake" domain name addresses which map to mac addresses on a
local network. DNS calls on these addresses then return the IP address
of the device with that mac.
## Where does this work?
The project was intended for Arch Linux, but you might be able to get it
to work on other Linux distributions.
## Usage
### Installation
To install the application on ArchLinux simply run:
```sh
$ yay -S local-dns
```

### Setup

For the operating system to incorporate this into DNS resolution we need
to set it up correctly. The most straightforward way is to use
networkmanager gui, for example on Gnome you should go to your internet
settings, than IPv4, disable Automatic DNS and set its IP to 127.0.0.1

### Configuration
To configure the program open each of the files in /etc/local-dns/ and
fill them in, following the instructions in the comments.

### Logs
For logs check */var/log/local-dns* path.

### Startup
Installation defines a systemd process. For the project to work you need
to start it up:
```sh
$ sudo systemctl start local-dns.service
```
And for it to start on startup of system:
```sh
$ sudo systemctl enable local-dns.service
```

## How does this work?
### DNS wrapping
This project defines a custom DNS server on localhost that answers
standard DNS queries by passing them to a real DNS server and acting as
a proxy to that server. When it receives a DNS name ending with
.localdns it answers with the IP assigned to the mac address defined in
/etc/local_dns/DnsMapUser.config
### Getting the IP of a MAC address
The server sends one ARP ping packet to every single IP on the subnet
specified by the user, concurrently. Another thread is constantly
listening for the reply packets, hence if that MAC is present on the
subnet, eventually we will receive an answer from the target IP.
Additionally, the answers are cached consistently so that we can just
retrieve the IP from it if the actual ARP pings were executed relatively
recently ("relatively" is configured by the user, default is 30 days).
