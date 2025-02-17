#include "networking.h"

struct RequestProperties {
    int nthreads;
    float timeout;
    int retry;
};


// Function to scan all interfaces and retrieve name, IP, MAC, and mask
std::unordered_map<std::string, InterfaceInfo> getAllInterfaces() {
    std::unordered_map<std::string, InterfaceInfo> interfaces;
    struct ifaddrs* ifaddr;

    // Get the list of interfaces
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return interfaces;
    }

    for (struct ifaddrs* iter = ifaddr; iter != NULL; iter = iter->ifa_next) {
        if (!iter->ifa_addr) continue;
        if (!(iter->ifa_flags & IFF_UP) || (iter->ifa_flags & IFF_LOOPBACK) || (iter->ifa_flags & IFF_NOARP) || (iter->ifa_flags & IFF_BROADCAST)) continue;

        InterfaceInfo info;

        // Get IPv4 address
        if (iter->ifa_addr->sa_family == AF_INET) {
            char ip[INET_ADDRSTRLEN];
            struct sockaddr_in* addr = (struct sockaddr_in*)iter->ifa_addr;
            inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);
            info.ip = ip;

            // Get subnet mask
            struct sockaddr_in* netmask = (struct sockaddr_in*)iter->ifa_netmask;
            char mask[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &netmask->sin_addr, mask, INET_ADDRSTRLEN);
            info.mask = mask;

            // Get MAC address
            info.mac = Crafter::GetMyMAC(iter->ifa_name);

            interfaces[iter->ifa_name] = info;
        }
    }

    freeifaddrs(ifaddr);
    return interfaces;
}

std::vector<std::string> calculateSubnetIPs(const std::string& ipAddress, const std::string& netmask) {
    std::vector<std::string> ips;

    struct in_addr ip, mask;
    inet_pton(AF_INET, ipAddress.c_str(), &ip);
    inet_pton(AF_INET, netmask.c_str(), &mask);

    uint32_t ipAddr = ntohl(ip.s_addr);
    uint32_t maskAddr = ntohl(mask.s_addr);
    uint32_t network = ipAddr & maskAddr;
    uint32_t broadcast = network | ~maskAddr;

    for (uint32_t current = network + 1; current < broadcast; ++current) {
        struct in_addr addr;
        addr.s_addr = htonl(current);
        ips.push_back(inet_ntoa(addr));
    }

    return ips;
}

std::unordered_map<std::string, std::string> ARPPing(std::string name, InterfaceInfo iface, RequestProperties prop) {
    using namespace std;
    using namespace Crafter;

    std::unordered_map<std::string, std::string> macToIP;

    vector<string> net = calculateSubnetIPs(iface.ip, iface.mask);

    Ethernet ether_header;

    ether_header.SetSourceMAC(iface.mac);                      // <-- Set our MAC as a source
    ether_header.SetDestinationMAC("ff:ff:ff:ff:ff:ff");   // <-- Set broadcast address

    ARP arp_header;

    arp_header.SetOperation(ARP::Request);                 // <-- Set Operation (ARP Request)
        arp_header.SetSenderIP(iface.ip);                          // <-- Set our network data
        arp_header.SetSenderMAC(iface.mac);                        // <-- Set our MAC as a sender

        /* ---------------------------------------------- */

    /* Define the network to scan */
    vector<string>::iterator ip_addr;                     // <-- Iterator

    /* Create a container of packet pointers to hold all the ARP requests */
    vector<Packet*> request_packets;

    /* Iterate to access each string that defines an IP address */
    for(ip_addr = net.begin() ; ip_addr != net.end() ; ip_addr++) {

        arp_header.SetTargetIP(*ip_addr);                   // <-- Set a destination IP address on ARP header

        /* Create a packet on the heap */
        Packet* packet = new Packet;

        /* Push the layers */
        packet->PushLayer(ether_header);
        packet->PushLayer(arp_header);

        /* Finally, push the packet into the container */
        request_packets.push_back(packet);
    }

    /* Create a container of packet with the same size of the request container to hold the responses */
    vector<Packet*> replies_packets(request_packets.size());

    SendRecv(request_packets.begin(), request_packets.end(), replies_packets.begin(), name, prop.timeout, prop.retry, prop.nthreads);

    vector<Packet*>::iterator it_pck;
    int counter = 0;
    for(it_pck = replies_packets.begin() ; it_pck < replies_packets.end() ; it_pck++) {

        Packet* reply_packet = (*it_pck);
        /* Check if the pointer is not NULL */
        if(reply_packet) {
            /* Get the ARP layer of the replied packet */
            ARP* arp_layer = reply_packet->GetLayer<ARP>();
            macToIP[arp_layer->GetSenderMAC()] = arp_layer->GetSenderIP();
        }

    }

    /* Delete the container with the ARP requests */
    for(it_pck = request_packets.begin() ; it_pck < request_packets.end() ; it_pck++)
        delete (*it_pck);

    /* Delete the container with the responses  */
    for(it_pck = replies_packets.begin() ; it_pck < replies_packets.end() ; it_pck++)
        delete (*it_pck);

    return macToIP;
}

std::unordered_map<std::string, std::string> scanInterface(std::string name, InterfaceInfo iface) {
    std::unordered_map<std::string, std::string> macToIP;
    if(iface.ip.empty() || iface.mac.empty() || iface.mask.empty())
        return macToIP;

    std::cout << "-------------------------------------" << std::endl;
    std::cout << "Interface: " << name << std::endl;
    std::cout << "  IP Address : " << iface.ip << std::endl;
    std::cout << "  MAC Address: " << iface.mac << std::endl;
    std::cout << "  Subnet Mask: " << iface.mask << std::endl;
    std::cout << std::endl;

    RequestProperties prop;
    prop.retry = 2;
    prop.timeout = 0.2;
    prop.nthreads = 256;

    std::cout << "Sending the ARP requests:" << std::endl;
    macToIP = ARPPing(name, iface, prop);

    // Add your own IP if it was not scanned
    if(!macToIP.contains(iface.mac))
        macToIP[iface.mac] = iface.ip;

    for (const auto& pair : macToIP) {
        std::cout << "  " << pair.first << " " << pair.second << std::endl;
    }
    std::cout << "Requests completed" << std::endl;
    return macToIP;
}
