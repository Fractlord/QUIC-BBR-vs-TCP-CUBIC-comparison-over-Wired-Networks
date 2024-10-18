
/* 
===================================================================
    Client-Router-Server Point-to-Point Topology
===================================================================

        +--------+          +---------+          +--------+
        | Client |----------| Router  |----------| Server |
        +--------+          +---------+          +--------+

    - Client, Router, and Server are connected via point-to-point links.
    - The QUIC protocol is simulated on the client-server path.
    - The router forwards packets between the client and server.

    Topology configuration:
    ------------------------
    Link 1 (Client <-> Router)
    Link 2 (Router <-> Server)
    
===================================================================
*/








#include <fstream>
#include <string>
#include <sys/stat.h>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/quic-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/quic-bbr.h"
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("QuicClientRouterServerExample");

// Packet size in bytes (assuming a common MTU size for QUIC packets)
const uint32_t PACKET_SIZE = 1500;

// Global variables to store the cwnd, RTT, and packet loss tracking
double g_cwnd = 0;
double g_rtt = 0;
uint32_t packetsSent = 0;
uint32_t packetsReceived = 0;

// Trace callback functions to update the global variables
void CwndTracer(uint32_t oldCwnd, uint32_t newCwnd) {
    g_cwnd = newCwnd / PACKET_SIZE;  // Convert to packets
    // Uncomment for debugging
    // NS_LOG_INFO("Cwnd updated: " << g_cwnd << " packets");
}

void RttTracer(Time oldRtt, Time newRtt) {
    g_rtt = newRtt.GetSeconds();  // Convert to seconds
    // Uncomment for debugging
    // NS_LOG_INFO("RTT updated: " << g_rtt << " seconds");
}

// Track packets sent
void PacketSentCallback(Ptr<const Packet> packet) {
    packetsSent++;
    // Uncomment for debugging
    // std::cout << "Packet sent. Total packets sent: " << packetsSent << std::endl;
}

// Track packets received
void PacketReceivedCallback(Ptr<const Packet> packet, const Address &address) {
    packetsReceived++;
    // Uncomment for debugging
    // std::cout << "Packet received. Total packets received: " << packetsReceived << std::endl;
}

// Function to calculate and log packet loss
void CalculatePacketLoss(std::ofstream &packetLossFile) {
    double time = Simulator::Now().GetSeconds();
    if (packetsSent > 0) {
        double packetLoss = ((packetsSent - packetsReceived) / static_cast<double>(packetsSent)) * 100;
        packetLossFile << time << "\t" << packetLoss << std::endl;
        // Uncomment for debugging
        // std::cout << "Time: " << time << " Packet Loss (%): " << packetLoss << std::endl;
    } else {
        // Uncomment for debugging
        // std::cout << "No packets sent yet at time: " << time << std::endl;
    }

    // Schedule packet loss calculation every second
    Simulator::Schedule(Seconds(1.0), &CalculatePacketLoss, std::ref(packetLossFile));
}

// Function to trace metrics similar to the ring topology code
void TraceMetrics(Ptr<PacketSink> sink, std::ofstream &throughputFile, std::ofstream &rttFile, std::ofstream &cwndFile) {
    static uint64_t lastTotalRx = 0;

    double timeInSeconds = Simulator::Now().GetSeconds();

    // Calculate throughput in Mbps
    uint64_t totalRx = sink->GetTotalRx();
    double throughput = ((totalRx - lastTotalRx) * 8.0) / 1e6; // Mbps
    lastTotalRx = totalRx;

    // Write metrics to files
    throughputFile << timeInSeconds << "\t" << throughput << std::endl;
    rttFile << timeInSeconds << "\t" << (g_rtt * 1000) << std::endl; // RTT in milliseconds
    cwndFile << timeInSeconds << "\t" << g_cwnd << std::endl;

    // Schedule next call to TraceMetrics
    Simulator::Schedule(Seconds(1.0), &TraceMetrics, sink, std::ref(throughputFile), std::ref(rttFile), std::ref(cwndFile));
}

void AttachTraces(Ptr<Application> app) {
    Ptr<BulkSendApplication> bulkSendApp = DynamicCast<BulkSendApplication>(app);
    if (bulkSendApp) {
        Ptr<Socket> socket = bulkSendApp->GetSocket();
        if (socket) {
            Ptr<QuicSocketBase> quicSocket = DynamicCast<QuicSocketBase>(socket);
            if (quicSocket) {
                quicSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndTracer));
                quicSocket->TraceConnectWithoutContext("RTT", MakeCallback(&RttTracer));
                NS_LOG_INFO("Successfully attached traces for cwnd and RTT.");
            } else {
                NS_LOG_INFO("Socket not available yet, retrying...");
                Simulator::Schedule(Seconds(0.1), &AttachTraces, app);
            }
        } else {
            NS_LOG_INFO("Socket is null, retrying...");
            Simulator::Schedule(Seconds(0.1), &AttachTraces, app);
        }
    } else {
        NS_LOG_ERROR("Failed to get BulkSendApplication.");
    }
}

void EnsureDirectoryExists(const std::string &directory) {
    struct stat info;
    if (stat(directory.c_str(), &info) != 0) {
        // Directory does not exist, create it
        if (mkdir(directory.c_str(), 0755) != 0) {
            NS_LOG_ERROR("Could not create output directory: " + directory);
            exit(1);
        }
    } else if (!(info.st_mode & S_IFDIR)) {
        NS_LOG_ERROR(directory + " exists but is not a directory");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    bool tracing = false;
    uint32_t maxBytes = 0;
    uint32_t QUICFlows = 1;
    bool isPacingEnabled = true;
    std::string pacingRate = "10Mbps"; // Set pacing rate to 10 Mbps
    uint32_t maxPackets = 0;
    uint32_t NUM_NODES = 3; // Total number of nodes: Client, Router, Server
    double DURATION = 100.0; // Set duration to 100 seconds

    Time::SetResolution(Time::NS);
    LogComponentEnable("QuicSocketBase", LOG_LEVEL_DEBUG);
    LogComponentEnable("QuicClientRouterServerExample", LOG_LEVEL_INFO);

    CommandLine cmd;
    cmd.AddValue("tracing", "Flag to enable/disable tracing", tracing);
    cmd.AddValue("maxBytes", "Total number of bytes for application to send", maxBytes);
    cmd.AddValue("maxPackets", "Total number of packets for application to send", maxPackets);
    cmd.AddValue("QUICFlows", "Number of application flows between sender and receiver", QUICFlows);
    cmd.AddValue("Pacing", "Flag to enable/disable pacing in QUIC", isPacingEnabled);
    cmd.AddValue("PacingRate", "Max Pacing Rate in bps", pacingRate);
    cmd.Parse(argc, argv);

    if (maxPackets != 0) {
        maxBytes = 500 * maxPackets;
    }

    Config::SetDefault("ns3::TcpSocketState::MaxPacingRate", StringValue(pacingRate));
    Config::SetDefault("ns3::TcpSocketState::EnablePacing", BooleanValue(isPacingEnabled));

    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(NUM_NODES); // Create nodes: Client, Router, Server

    Ptr<Node> client = nodes.Get(0);
    Ptr<Node> router = nodes.Get(1);
    Ptr<Node> server = nodes.Get(2);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Ensure QUIC is installed on all nodes
    QuicHelper quic;
    quic.InstallQuic(nodes);

    // Set QUIC to use BBR congestion control
    Config::SetDefault("ns3::QuicL4Protocol::SocketType", StringValue("ns3::QuicBbr"));

    NS_LOG_INFO("Create channels.");
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps")); // Adjusted Data Rate
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms")); // Adjusted Delay

    Ipv4AddressHelper address;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;

    // Create point-to-point link between Client and Router
    devices = pointToPoint.Install(client, router);
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces = address.Assign(devices);

    // Create point-to-point link between Router and Server
    devices = pointToPoint.Install(router, server);
    address.SetBase("10.1.2.0", "255.255.255.0");
    interfaces = address.Assign(devices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NS_LOG_INFO("Create Applications.");
    ApplicationContainer sourceApps;
    ApplicationContainer sinkApps;

    // Assuming single flow (QUICFlows =1)
    uint16_t port = 10000;

    // Install PacketSink on Server
    PacketSinkHelper sinkHelper("ns3::QuicSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(server); // Server
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(DURATION));
    sinkApps.Add(sinkApp);

    // Get the IP address of the server's interface connected to the router
    Ptr<Ipv4> ipv4Server = server->GetObject<Ipv4>();
    Ipv4Address serverAddress = ipv4Server->GetAddress(1, 0).GetLocal(); // Assuming interface 1 is the router link

    // Install BulkSendApplication on Client
    BulkSendHelper source("ns3::QuicSocketFactory",
                          InetSocketAddress(serverAddress, port));
    source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer clientApp = source.Install(client); // Client
    sourceApps.Add(clientApp);

    Ptr<Application> app = clientApp.Get(0);

    Simulator::Schedule(Seconds(0.1), &AttachTraces, app); // Schedule trace attachment

    // Open output files
    std::string outputDir = "/path/to/source/ns3folder/desired/output/file/"; //CHANGE THIS
    EnsureDirectoryExists(outputDir);

    std::ofstream throughputFile(outputDir + "quicbbr.throughput");
    std::ofstream rttFile(outputDir + "quicbbr.rtt");
    std::ofstream cwndFile(outputDir + "quicbbr.cwnd");
    std::ofstream packetLossFile(outputDir + "quicbbr.packetloss");
    // Removed connectionFile to align with the ring topology example

    // Ensure the files are open
    if (!throughputFile.is_open() || !rttFile.is_open() || !cwndFile.is_open() || !packetLossFile.is_open()) {
        NS_LOG_ERROR("Could not open output files for writing");
        return 1; // Exit with error
    }

    // Get the PacketSink pointer
    Ptr<PacketSink> sinkPtr = DynamicCast<PacketSink>(sinkApp.Get(0));

    // Schedule the first call to TraceMetrics at t =1 second
    Simulator::Schedule(Seconds(1.0), &TraceMetrics, sinkPtr, std::ref(throughputFile), std::ref(rttFile), std::ref(cwndFile));

    // Schedule packet loss calculation
    Simulator::Schedule(Seconds(1.0), &CalculatePacketLoss, std::ref(packetLossFile));

    // Connect the callbacks for packet tracking
    sourceApps.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&PacketSentCallback));
    sinkApps.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&PacketReceivedCallback));

    // Start and stop applications
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(DURATION));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(DURATION - 1.0));

    if (tracing) {
        AsciiTraceHelper ascii;
        pointToPoint.EnableAsciiAll(ascii.CreateFileStream("quic-pacing.tr"));
        pointToPoint.EnablePcapAll("quic-pacing", false);
    }

    // Run the simulation for the specified duration
    Simulator::Stop(Seconds(DURATION));
    Simulator::Run();

    // Close the output files
    throughputFile.close();
    rttFile.close();
    cwndFile.close();
    packetLossFile.close();

    // Destroy the simulation
    Simulator::Destroy();

    NS_LOG_INFO("Done.");

    return 0;
}
