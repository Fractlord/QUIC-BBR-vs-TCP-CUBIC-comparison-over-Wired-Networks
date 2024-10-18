/* 
===================================================================
                        Star Topology (8 Nodes)
===================================================================

                           +--------+
                           | Server |
                           +--------+
                                |
                                |
                           +--------+
                           | Router |
                           +--------+
                          /    |    \
            -------------/     |     \-------------
          /              /     |     \             \
    +--------+      +--------+  |  +--------+      +--------+
    | Client |      | Client |  |  | Client |      | Client |
    +--------+      +--------+  |  +--------+      +--------+
          \              \     |     /             /     
            -------------\-----+-----/-------------
                          \    |    / 
                           +--------+ 
                           | Client | 
                           +--------+
                           +--------+ 
                           | Client | 
                           +--------+

    - 6 Clients connected to the central Router.
    - The Router is connected to the Server at the top.
    - All links are point-to-point between clients and the router, 
      and between the router and the server.
   
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

NS_LOG_COMPONENT_DEFINE("QuicStarTopologyExample");

// Packet size in bytes (assuming a common MTU size for QUIC packets)
const uint32_t PACKET_SIZE = 1500;

// Global variables to store cwnd, RTT, and packet loss tracking
double g_cwnd = 0;
double g_rtt = 0;
uint32_t packetsSent = 0;
uint32_t packetsReceived = 0;

// Trace callback functions to update the global variables
void CwndTracer(uint32_t oldCwnd, uint32_t newCwnd) {
    g_cwnd = newCwnd / PACKET_SIZE;  // Convert to packets
}

void RttTracer(Time oldRtt, Time newRtt) {
    g_rtt = newRtt.GetSeconds();  // Convert to seconds
}

// Track packets sent
void PacketSentCallback(Ptr<const Packet> packet) {
    packetsSent++;
}

// Track packets received
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

// Function to trace metrics (Throughput, RTT, cwnd)
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

    Simulator::Schedule(Seconds(1.0), &TraceMetrics, sink, std::ref(throughputFile), std::ref(rttFile), std::ref(cwndFile));
}

// Attach traces for cwnd and RTT
void AttachTraces(Ptr<Application> app) {
    Ptr<BulkSendApplication> bulkSendApp = DynamicCast<BulkSendApplication>(app);
    if (bulkSendApp) {
        Ptr<Socket> socket = bulkSendApp->GetSocket();
        if (socket) {
            Ptr<QuicSocketBase> quicSocket = DynamicCast<QuicSocketBase>(socket);
            if (quicSocket) {
                quicSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndTracer));
                quicSocket->TraceConnectWithoutContext("RTT", MakeCallback(&RttTracer));
            } else {
                Simulator::Schedule(Seconds(0.1), &AttachTraces, app);
            }
        } else {
            Simulator::Schedule(Seconds(0.1), &AttachTraces, app);
        }
    }
}

// Ensure the output directory exists
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
    std::string pacingRate = "25Mbps";
    uint32_t maxPackets = 0;
    uint32_t NUM_NODES = 8; // 6 Clients + 1 Router + 1 Server
    double DURATION = 60.0;

    Time::SetResolution(Time::NS);
    LogComponentEnable("QuicSocketBase", LOG_LEVEL_DEBUG);
    LogComponentEnable("QuicStarTopologyExample", LOG_LEVEL_INFO);

    CommandLine cmd;
    cmd.AddValue("tracing", "Enable or disable tracing", tracing);
    cmd.AddValue("maxBytes", "Total bytes to send", maxBytes);
    cmd.AddValue("maxPackets", "Total packets to send", maxPackets);
    cmd.AddValue("QUICFlows", "Number of QUIC flows", QUICFlows);
    cmd.AddValue("Pacing", "Enable or disable pacing in QUIC", isPacingEnabled);
    cmd.AddValue("PacingRate", "Pacing rate", pacingRate);
    cmd.Parse(argc, argv);

    if (maxPackets != 0) {
        maxBytes = 500 * maxPackets;
    }

    Config::SetDefault("ns3::TcpSocketState::MaxPacingRate", StringValue(pacingRate));
    Config::SetDefault("ns3::TcpSocketState::EnablePacing", BooleanValue(isPacingEnabled));

    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(NUM_NODES); // Create 8 nodes: 6 Clients, 1 Router, 1 Server

    Ptr<Node> router = nodes.Get(0);
    NodeContainer clients;
    for (uint32_t i = 1; i < NUM_NODES - 1; ++i) {
        clients.Add(nodes.Get(i));
    }
    Ptr<Node> server = nodes.Get(NUM_NODES - 1);

    InternetStackHelper stack;
    stack.Install(nodes);

    QuicHelper quic;
    quic.InstallQuic(nodes);

    Config::SetDefault("ns3::QuicL4Protocol::SocketType", StringValue("ns3::QuicBbr"));

    NS_LOG_INFO("Create channels.");
    PointToPointHelper pointToPointClientToRouter;
    pointToPointClientToRouter.SetDeviceAttribute("DataRate", StringValue("15Mbps"));
    pointToPointClientToRouter.SetChannelAttribute("Delay", StringValue("3ms"));

    PointToPointHelper pointToPointRouterToServer;
    pointToPointRouterToServer.SetDeviceAttribute("DataRate", StringValue("15Mbps"));
    pointToPointRouterToServer.SetChannelAttribute("Delay", StringValue("3ms"));

    Ipv4AddressHelper address;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;

    for (uint32_t i = 0; i < clients.GetN(); ++i) {
        devices = pointToPointClientToRouter.Install(clients.Get(i), router);
        std::string subnet = "10.1." + std::to_string(i + 1) + ".0";
        address.SetBase(subnet.c_str(), "255.255.255.0");
        interfaces = address.Assign(devices);
    }

    devices = pointToPointRouterToServer.Install(router, server);
    address.SetBase("10.1.0.0", "255.255.255.0");
    interfaces = address.Assign(devices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NS_LOG_INFO("Create Applications.");
    ApplicationContainer sourceApps;
    ApplicationContainer sinkApps;

    for (uint32_t i = 0; i < QUICFlows; ++i) {
        uint16_t port = 10000 + i;

        BulkSendHelper source("ns3::QuicSocketFactory",
                              InetSocketAddress(interfaces.GetAddress(1), port));

        source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
        ApplicationContainer clientApp = source.Install(clients.Get(i));
        sourceApps.Add(clientApp);

        Ptr<Application> app = clientApp.Get(0);
        Simulator::Schedule(Seconds(0.1), &AttachTraces, app);

        PacketSinkHelper sink("ns3::QuicSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port));
        sinkApps.Add(sink.Install(server));
    }

    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(DURATION));
    sourceApps.Start(Seconds(1));
    sourceApps.Stop(Seconds(DURATION - 1));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    std::string outputDir = "/path/to/sourcens3/folder/desired/output/file/"; //CHANGE THIS 
    EnsureDirectoryExists(outputDir);

    std::ofstream throughputFile(outputDir + "quicbbr.throughput");
    std::ofstream rttFile(outputDir + "quicbbr.rtt");
    std::ofstream cwndFile(outputDir + "quicbbr.cwnd");
    std::ofstream packetLossFile(outputDir + "quicbbr.packetloss");

    if (!throughputFile.is_open() || !rttFile.is_open() || !cwndFile.is_open() || !packetLossFile.is_open()) {
        NS_LOG_ERROR("Could not open output files");
        return 1;
    }

    Ptr<PacketSink> sinkPtr = DynamicCast<PacketSink>(sinkApps.Get(0));

    Simulator::Schedule(Seconds(1.0), &TraceMetrics, sinkPtr, std::ref(throughputFile), std::ref(rttFile), std::ref(cwndFile));
    Simulator::Schedule(Seconds(1.0), &CalculatePacketLoss, std::ref(packetLossFile));

    sourceApps.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&PacketSentCallback));
    sinkApps.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&PacketReceivedCallback));

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

