#include "settings.h"

settings globalSettings;
std::unordered_map<std::string, std::string> macToDomain;
std::string CSVFile;


// Function to remove comments and return clean JSON string
std::string openConfig(std::string name) {
    std::string filename = "../config/" + name;
    std::ifstream file(filename);

    // Check if the file is not good or cannot be opened
    if (!file.good() || !file.is_open()) {
        filename = "/etc/local-dns/" + name;
        file.open(filename);

        if (!file.good() || !file.is_open()) {
            std::cerr << "Failed to open " << name << std::endl;
            throw std::runtime_error("Cannot open configuration files.");
        }
    }

    std::ostringstream cleanedJsonStream;
    std::string line;

    while (std::getline(file, line)) {
        // Trim leading whitespace
        size_t firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos) {
            // If line starts with "//", skip it
            if (line[firstNonSpace] == '/' && line[firstNonSpace + 1] == '/') {
                continue;
            }
        }
        // Add the line to the cleaned JSON
        cleanedJsonStream << line << '\n';
    }

    file.close();
    return cleanedJsonStream.str();
}

int maskToPrefixLength(const std::string& mask) {
    std::istringstream iss(mask);
    std::string segment;
    int prefixLength = 0;

    // Split the mask into its four octets
    while (std::getline(iss, segment, '.')) {
        int octet = std::stoi(segment);
        while (octet) {
            prefixLength += (octet & 1);
            octet >>= 1;
        }
    }
    return prefixLength;
}

// Function to compare two masks and return the most restrictive one
std::string getMostRestrictiveMask(const std::string& mask1, const std::string& mask2) {
    int prefix1 = maskToPrefixLength(mask1);
    int prefix2 = maskToPrefixLength(mask2);

    // The most restrictive mask has the higher prefix length
    return (prefix1 >= prefix2) ? mask1 : mask2;
}

// reads DNSMapUser.config
std::unordered_map<std::string, std::string> configDomains() {
    std::unordered_map<std::string, std::string> macToDomain;
    try {
        // Get json out of config
        json jsonData = json::parse(openConfig("DnsMapUser.config"));
        for (auto& [key, valueRaw] : jsonData.items()) {
            if (valueRaw.is_array() && !valueRaw.empty()) {
                std::string value = valueRaw[0];
                macToDomain[value] = key;
            }
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
    }

    return macToDomain;
}

bool updateLocalDNS() {
    std::cout << std::endl << "Scanning " << globalSettings.iface << std::endl;
    std::unordered_map<std::string, std::unordered_map<uint32_t, std::vector<addressesResponse>>> localAddresses;
    std::unordered_map<std::string, std::string> macToIp = scanInterface(globalSettings.iface, globalSettings.ifaceInfo);
    for (auto& [mac, domain] : macToDomain) {
        if(macToIp.contains(mac)) {
            // ip, length(for A record always 4), timestamp of death
            addressesResponse dns(macToIp[mac], 4, time(nullptr) + globalSettings.timeout, true);
            // dnsMapKey class is 1 and type is 1 (A record)
            localAddresses[domain][dnsMapKey(1, 1)].push_back(dns);
            CSVAppendLocal(CSVFile, domain, dns);
        }
    }
    int found = localAddresses.size();
    std::cout << "Found " << found << " out of " << macToDomain.size() << " MAC addresses" << std::endl;

    // Add and override addresses with domains from localAddresses
    for (const auto& pair : localAddresses) {
        addresses[pair.first] = pair.second;
    }

    return static_cast<bool>(found); // true if found > 0
}

void config() {

    try {
        // Get json out of config
        json jsonData = json::parse(openConfig("DnsMapUserSettings.config"));
        globalSettings.timeout = static_cast<time_t>(1 * 86400); // days to seconds
        for (auto& [key, valueRaw] : jsonData.items()) {
            if (valueRaw.is_array() && !valueRaw.empty()) {
                std::string value = valueRaw[0];
                if (key == "iface") {
                    globalSettings.iface = value;
                } else if (key == "ip_mask") {
                    globalSettings.ifaceInfo.mask = value;
                } else if (key == "cache_timeout") {
                    globalSettings.timeout = static_cast<time_t>(stof(value) * 86400); // days to seconds
                } else {
                    std::cerr << "Config Error: " << key << " option not recognized" << std::endl;
                }
            }
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
    }


    // if(globalSettings.ifaceInfo.mask.length() <= 2){
    //     globalSettings.ifaceInfo.mask = "255.255.255.0";
    // }

    macToDomain = configDomains();

    // Checks interface, if it is incorrect choose one based on mac addresses
    std::unordered_map<std::string, InterfaceInfo> interfaces = getAllInterfaces();
    if (!interfaces.contains(globalSettings.iface)) {
        std::cout << "No valid interface provided" << std::endl;
        for (auto& [iface, info] : interfaces) {
            globalSettings.iface = iface;
            globalSettings.ifaceInfo = info;
            globalSettings.ifaceInfo.mask = getMostRestrictiveMask(globalSettings.ifaceInfo.mask, info.mask);
            if(updateLocalDNS())
                break;
        }
    }
    else { // Interface provided corectly 
        globalSettings.ifaceInfo = interfaces[globalSettings.iface];
        globalSettings.ifaceInfo.mask = getMostRestrictiveMask(globalSettings.ifaceInfo.mask, interfaces[globalSettings.iface].mask);
        updateLocalDNS();
    }

}

void printSettings() {
    using namespace std;
    cout << endl
    << "Selected interface " << globalSettings.iface << endl
    << "Local domain address TTL " << globalSettings.timeout << endl
    << "Server MAC address " << globalSettings.ifaceInfo.mac << endl
    << "Server IP address " << globalSettings.ifaceInfo.ip << endl
    << "Subnet Mask " << globalSettings.ifaceInfo.mask << endl << endl;
}

std::string getDnsServerRedirect(std::string dnsServer, std::string upstream_dns) {

    std::string input;
    std::ifstream inputStream("/etc/resolv.conf");
    std::string nameserver_indicator = "nameserver ";

    // while(std::getline(inputStream, input)) {
    //     std::string prefix = input.substr(0, nameserver_indicator.size());
    //     if(prefix == nameserver_indicator) {
    //         std::string dns = input.substr(nameserver_indicator.size());
    //         if(dns != dnsServer) {
    //             return dns;
    //         }
    //     }
    // }

    return upstream_dns;
}
