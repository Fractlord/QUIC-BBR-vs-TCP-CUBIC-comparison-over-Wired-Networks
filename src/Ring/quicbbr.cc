/* 
===================================================================
                        Ring Topology (10 Nodes)
===================================================================

     +--------+      +--------+      +--------+      +--------+
     | Node 1 |------| Node 2 |------| Node 3 |------| Node 4 |
     +--------+      +--------+      +--------+      +--------+
         |                                    |            |
     +--------+      +--------+      +--------+      +--------+
     | Server |------| Node 10|------| Node 9 |------| Node 8 |
     +--------+      +--------+      +--------+      +--------+
                                    |       |
                              +--------+      +--------+
                              | Node 7 |------| Node 6 |
                              +--------+      +--------+

    - 10 nodes connected in a ring topology using point-to-point links.
    - The server is placed at Node 5.
    - All nodes are connected in a ring, and the last node connects to the first node, forming a closed loop.
    - The simulation tracks TCP CUBIC performance using the following metrics:
      - Congestion Window (Cwnd)
      - Round Trip Time (RTT)
      - Throughput
      - Packet Loss
    - Data Rate: 5 Mbps for the links
    - Delay: 15 ms propagation delay for all point-to-point links

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
#include "ns3/flow-monitor-module.h"
#include "ns3/quic-bbr.h"
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("QuicRingTopologyExample");

// Packet size in bytes (assuming a common MTU size for QUIC packets)
const uint32_t PACKET_SIZE = 1500;

// Global variables to store the cwnd, RTT, and packet loss tracking
double g_cwnd = 0;
double g_rtt = 0;
uint32_t packetsSent = 0;
uint32_t packetsReceived = 0;

// Callback to track packets sent
void PacketSentCallback(Ptr<const Packet> packet) {
    packetsSent++;
}

// Callback to track packets received
void PacketReceivedCallback(Ptr<const Packet> packet, const Address &address) {
    packetsReceived++;
}

// Function to calculate and log packet loss
void CalculatePacketLoss(std::ofstream &packetLossFile) {
    double time = Simulator::Now().GetSeconds();
    if (packetsSent > 0) {
        double packetLoss = ((packetsSent - packetsReceived) / static_cast<double>(packetsSent)) * 100;
        packetLossFile << time << "\t" << packetLoss << std::endl;
    }
    Simulator::Schedule(Seconds(1.0), &CalculatePacketLoss, std::ref(packetLossFile));
}

// Trace callback functions to update the global variables
void CwndTracer(uint32_t oldCwnd, uint32_t newCwnd) {
    g_cwnd = newCwnd / PACKET_SIZE;
}

void RttTracer(Time oldRtt, Time newRtt) {
    g_rtt = newRtt.GetSeconds();
}

// Function to trace metrics
void TraceMetrics(Ptr<PacketSink> sink, std::ofstream &throughputFile, std::ofstream &rttFile, std::ofstream &cwndFile) {
    static uint64_t lastTotalRx = 0;

    double timeInSeconds = Simulator::Now().GetSeconds();

    // Calculate throughput in Mbps
    uint64_t totalRx = sink->GetTotalRx();
    double throughput = ((totalRx - lastTotalRx) * 8.0) / 1e6; // Mbps
    lastTotalRx = totalRx;

    // Write metrics to files
    throughputFile << timeInSeconds << "\t" << throughput << std::endl;
    rttFile << timeInSeconds << "\t" << g_rtt * 1000 << std::endl; // RTT in milliseconds
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
    std::string pacingRate = "17Mbps";
    uint32_t maxPackets = 0;
    uint32_t NUM_NODES = 10; // Total number of nodes changed to 10
    double DURATION = 100.0;

    Time::SetResolution(Time::NS);
    LogComponentEnable("QuicRingTopologyExample", LOG_LEVEL_INFO);

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
    nodes.Create(NUM_NODES); // Create 10 nodes

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
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("15ms"));

    NetDeviceContainer devices;
    Ipv4AddressHelper address;

    // Create point-to-point links to form a ring topology
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        NetDeviceContainer link;
        if (i == nodes.GetN() - 1) {
            link = pointToPoint.Install(NodeContainer(nodes.Get(i), nodes.Get(0))); // Connect last node to the first
        } else {
            link = pointToPoint.Install(NodeContainer(nodes.Get(i), nodes.Get(i + 1))); // Connect current node to the next
        }
        devices.Add(link);
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        address.Assign(link);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NS_LOG_INFO("Create Applications.");
    ApplicationContainer sourceApps;
    ApplicationContainer sinkApps;

    uint16_t serverPort = 9;

    // Install PacketSink on the last node
    Address sinkAddr(InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    PacketSinkHelper sinkHelper("ns3::QuicSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(NUM_NODES - 1)); // Install on the last node
    sinkApp.Start(Seconds(0.01));
    sinkApp.Stop(Seconds(DURATION));
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));

    // Get the IP address of the last node (destination)
    Ptr<Ipv4> ipv4 = nodes.Get(NUM_NODES - 1)->GetObject<Ipv4>();
    Ipv4Address destAddress = ipv4->GetAddress(1, 0).GetLocal(); // Assuming interface 1 is the first p2p link

    // Install BulkSendApplication on the first node (source)
    BulkSendHelper source("ns3::QuicSocketFactory",
                          InetSocketAddress(destAddress, serverPort));
    source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer sourceApp = source.Install(nodes.Get(0)); // Install on the first node
    sourceApp.Start(Seconds(0.0));
    sourceApp.Stop(Seconds(DURATION));

    // Ensure output directory exists
    std::string outputDir = "/path/to/sourcens3/folder/desired/output/file/"; //CHANGE THIS 
    EnsureDirectoryExists(outputDir);

    // Open the output files
    std::ofstream cwndFile(outputDir + "quicbbr.cwnd");
    std::ofstream rttFile(outputDir + "quicbbr.rtt");
    std::ofstream throughputFile(outputDir + "quicbbr.throughput");
    std::ofstream packetLossFile(outputDir + "quicbbr.packetloss");
    if (!cwndFile.is_open() || !rttFile.is_open() || !throughputFile.is_open() || !packetLossFile.is_open()) {
        std::cerr << "Error opening output files" << std::endl;
        return 1;
    }

    // Attach callbacks for packet sent and received
    sourceApp.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&PacketSentCallback));
    sinkApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&PacketReceivedCallback));

    // Schedule tracing functions
    Simulator::Schedule(Seconds(0.1), &AttachTraces, sourceApp.Get(0));

    // Schedule packet loss calculation
    Simulator::Schedule(Seconds(1.0), &CalculatePacketLoss, std::ref(packetLossFile));

    // Schedule the first call to TraceMetrics at t = 1 second
    Simulator::Schedule(Seconds(1.0), &TraceMetrics, sink, std::ref(throughputFile), std::ref(rttFile), std::ref(cwndFile));

    // Run the simulation for the specified duration
    Simulator::Stop(Seconds(DURATION));
    Simulator::Run();

    // Close the output files
    cwndFile.close();
    rttFile.close();
    throughputFile.close();
    packetLossFile.close();

    // Destroy the simulation
    Simulator::Destroy();

    NS_LOG_INFO("Done.");

    return 0;
}
