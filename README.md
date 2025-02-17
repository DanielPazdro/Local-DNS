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
