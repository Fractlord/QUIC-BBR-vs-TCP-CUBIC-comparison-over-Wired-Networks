/* 
===================================================================
                        Bus Topology (6 Nodes)
===================================================================

+--------+      +--------+      +--------+      +--------+      +--------+      +--------+
| Client |------| Client |------| Client |------| Client |------| Client |------| Server |
+--------+      +--------+      +--------+      +--------+      +--------+      +--------+

    - 5 Clients are connected in a bus topology using CSMA (Carrier Sense Multiple Access).
    - All clients share a common communication medium (bus).
    - The server is located at the end of the bus, receiving data from the clients.
    - The simulation tracks TCP CUBIC performance using the following metrics:
      - Congestion Window (Cwnd)
      - Round Trip Time (RTT)
      - Throughput
      - Packet Loss
    - Data Rate: 85 Mbps on the CSMA bus
    - Delay: 3 ms propagation delay for all links

===================================================================
*/


#include <fstream>
#include <string>
#include <sys/stat.h>
#include <iomanip>

// ns-3 core and module headers
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/quic-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/quic-bbr.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("QuicBusTopologyExample");

// Packet size in bytes (assuming a common MTU size for QUIC packets)
const uint32_t PACKET_SIZE = 1500; // Make sure this matches your traffic

// Global variables to store the cwnd, RTT, and packet loss tracking
double g_cwnd = 0;
double g_rtt = 0;
uint32_t packetsSent = 0;
uint32_t packetsReceived = 0; // Tracked based on received bytes

// Trace callback functions to update the global variables
void CwndTracer(uint32_t oldCwnd, uint32_t newCwnd) {
    g_cwnd = newCwnd / PACKET_SIZE;  // Convert to packets
    NS_LOG_INFO("Cwnd updated: " << g_cwnd << " packets");
}

void RttTracer(Time oldRtt, Time newRtt) {
    g_rtt = newRtt.GetSeconds();  // Convert to seconds
    NS_LOG_INFO("RTT updated: " << g_rtt << " seconds");
}

// Track packets sent
void PacketSentCallback(Ptr<const Packet> packet) {
    packetsSent++;
    NS_LOG_INFO("Packet sent. Total packets sent: " << packetsSent);
}

// Function to calculate and log packet loss
void CalculatePacketLoss(std::ofstream &packetLossFile, Ptr<PacketSink> sink) {
    double time = Simulator::Now().GetSeconds();

    // Get the total number of packets received by converting bytes to packets
    packetsReceived = sink->GetTotalRx() / PACKET_SIZE;

    NS_LOG_INFO("Packets Sent: " << packetsSent << " Packets Received: " << packetsReceived);

    if (packetsSent > 0) {
        double packetLoss = ((packetsSent - packetsReceived) / static_cast<double>(packetsSent)) * 100;
        packetLossFile << time << "\t" << packetLoss << std::endl;
        NS_LOG_INFO("Packet Loss at " << time << " seconds: " << packetLoss << "%");
    } else {
        packetLossFile << time << "\t" << 0.0 << std::endl;
    }

    // Schedule packet loss calculation every second
    Simulator::Schedule(Seconds(1.0), &CalculatePacketLoss, std::ref(packetLossFile), sink);
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
                NS_LOG_INFO("Traces successfully attached for cwnd and RTT.");
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
    }
}

int main(int argc, char *argv[]) {
    bool tracing = false;
    uint32_t maxBytes = 0;  // 0 means unlimited
    uint32_t QUICFlows = 1;
    bool isPacingEnabled = true;
    std::string pacingRate = "135Mbps"; // Increased pacing rate for smoother performance
    uint32_t maxPackets = 0;
    uint32_t NUM_NODES = 6; // Total number of nodes in the bus topology
    double DURATION = 100.0; // Set duration to 100 seconds

    Time::SetResolution(Time::NS);
    CommandLine cmd;
    cmd.AddValue("tracing", "Flag to enable/disable tracing", tracing);
    cmd.AddValue("maxBytes", "Total number of bytes for application to send", maxBytes);
    cmd.AddValue("maxPackets", "Total number of packets for application to send", maxPackets);
    cmd.AddValue("QUICFlows", "Number of application flows between sender and receiver", QUICFlows);
    cmd.AddValue("Pacing", "Flag to enable/disable pacing in QUIC", isPacingEnabled);
    cmd.AddValue("PacingRate", "Max Pacing Rate in bps", pacingRate);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpSocketState::MaxPacingRate", StringValue(pacingRate));
    Config::SetDefault("ns3::TcpSocketState::EnablePacing", BooleanValue(isPacingEnabled));

    NodeContainer nodes;
    nodes.Create(NUM_NODES); // Create 6 nodes for the bus topology

    InternetStackHelper stack;
    stack.Install(nodes);

    QuicHelper quic;
    quic.InstallQuic(nodes);

    Config::SetDefault("ns3::QuicL4Protocol::SocketType", StringValue("ns3::QuicBbr"));

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("85Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("3ms"));

    NetDeviceContainer devices = csma.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    ApplicationContainer sourceApps;
    ApplicationContainer sinkApps;

    uint16_t port = 10000;

    PacketSinkHelper sinkHelper("ns3::QuicSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(NUM_NODES - 1)); // Server
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(DURATION));
    sinkApps.Add(sinkApp);

    Ipv4Address serverAddress = interfaces.GetAddress(NUM_NODES - 1);

    for (uint32_t i = 0; i < NUM_NODES - 1; ++i) {
        BulkSendHelper source("ns3::QuicSocketFactory", InetSocketAddress(serverAddress, port));
        source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
        ApplicationContainer clientApp = source.Install(nodes.Get(i));
        sourceApps.Add(clientApp);
        Simulator::Schedule(Seconds(0.1), &AttachTraces, clientApp.Get(0));
        clientApp.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&PacketSentCallback));
    }

    std::string outputDir = "/path/to/sourcens3/folder/desired/output/file/"; //CHANGE THIS 
    EnsureDirectoryExists(outputDir);

    std::ofstream throughputFile(outputDir + "quicbbr.throughput");
    std::ofstream rttFile(outputDir + "quicbbr.rtt");
    std::ofstream cwndFile(outputDir + "quicbbr.cwnd");
    std::ofstream packetLossFile(outputDir + "quicbbr.packetloss");

    if (!throughputFile.is_open() || !rttFile.is_open() || !cwndFile.is_open() || !packetLossFile.is_open()) {
        NS_LOG_ERROR("Could not open output files for writing");
        return 1;
    }

    Ptr<PacketSink> sinkPtr = DynamicCast<PacketSink>(sinkApp.Get(0));
    Simulator::Schedule(Seconds(1.0), &TraceMetrics, sinkPtr, std::ref(throughputFile), std::ref(rttFile), std::ref(cwndFile));
    Simulator::Schedule(Seconds(1.0), &CalculatePacketLoss, std::ref(packetLossFile), sinkPtr);

    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(DURATION));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(DURATION - 1.0));

    Simulator::Stop(Seconds(DURATION));
    Simulator::Run();

    throughputFile.close();
    rttFile.close();
    cwndFile.close();
    packetLossFile.close();

    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
