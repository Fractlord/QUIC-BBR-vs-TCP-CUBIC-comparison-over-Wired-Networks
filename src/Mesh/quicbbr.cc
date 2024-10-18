/* 
===================================================================
                        Mesh Topology (10 Nodes)
===================================================================

       +--------+       +--------+       +--------+       +--------+
       | Node 1 |-------| Node 2 |-------| Node 3 |-------| Node 4 |
       +--------+       +--------+       +--------+       +--------+
          \  |  /          \  |  /          \  |  /          \  |  /
          +--------+       +--------+       +--------+       +--------+
          | Node 5 |-------| Node 6 |-------| Node 7 |-------| Node 8 |
          +--------+       +--------+       +--------+       +--------+
             \  |  /          \  |  /          \  |  /          \  |  /
             +--------+       +--------+
             | Node 9 |-------| Server |
             +--------+       +--------+

    - 10 nodes connected in a fully meshed topology.
    - Each node is connected to every other node in the network.
    - All links are point-to-point with the following characteristics:
      - Data Rate: 6 Mbps for each link
      - Delay: 15 ms for each link

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

NS_LOG_COMPONENT_DEFINE("QuicMeshTopologyExample");

const uint32_t PACKET_SIZE = 1500; // Packet size in bytes (MTU)

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

// Calculate packet loss
void CalculatePacketLoss(std::ofstream &packetLossFile) {
    double time = Simulator::Now().GetSeconds();
    if (packetsSent > 0) {
        double packetLoss = ((packetsSent - packetsReceived) / static_cast<double>(packetsSent)) * 100;
        packetLossFile << time << "\t" << packetLoss << std::endl;
    }

    // Schedule next packet loss calculation
    Simulator::Schedule(Seconds(1.0), &CalculatePacketLoss, std::ref(packetLossFile));
}

// Trace metrics (congestion window, RTT, throughput)
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

// Attach traces for congestion window and RTT
void AttachTraces(Ptr<Application> app) {
    Ptr<BulkSendApplication> bulkSendApp = DynamicCast<BulkSendApplication>(app);
    if (bulkSendApp) {
        Ptr<Socket> socket = bulkSendApp->GetSocket();
        if (socket) {
            Ptr<QuicSocketBase> quicSocket = DynamicCast<QuicSocketBase>(socket);
            if (quicSocket) {
                quicSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndTracer));
                quicSocket->TraceConnectWithoutContext("RTT", MakeCallback(&RttTracer));
                NS_LOG_INFO("Successfully attached traces for Cwnd and RTT.");
            } else {
                NS_LOG_INFO("QUIC socket not available yet, retrying in 0.2 seconds...");
                Simulator::Schedule(Seconds(0.2), &AttachTraces, app);  // Retry after 0.2 seconds
            }
        } else {
            NS_LOG_INFO("Socket is null, retrying in 0.2 seconds...");
            Simulator::Schedule(Seconds(0.2), &AttachTraces, app);  // Retry after 0.2 seconds
        }
    } else {
        NS_LOG_ERROR("Failed to get BulkSendApplication.");
    }
}

// Ensure the output directory exists
void EnsureDirectoryExists(const std::string &directory) {
    struct stat info;
    if (stat(directory.c_str(), &info) != 0) {
        mkdir(directory.c_str(), 0755);
    }
}

int main(int argc, char *argv[]) {
    uint32_t maxBytes = 0;
    uint32_t NUM_NODES = 10;  // Number of nodes
    double DURATION = 100.0;   // Simulation duration
    bool isPacingEnabled = true;
    std::string pacingRate = "10Mbps";

    Time::SetResolution(Time::NS);
    LogComponentEnable("QuicSocketBase", LOG_LEVEL_DEBUG);
    LogComponentEnable("QuicMeshTopologyExample", LOG_LEVEL_INFO);

    CommandLine cmd;
    cmd.AddValue("maxBytes", "Total number of bytes for application to send", maxBytes);
    cmd.AddValue("Pacing", "Flag to enable/disable pacing in QUIC", isPacingEnabled);
    cmd.AddValue("PacingRate", "Max Pacing Rate in bps", pacingRate);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpSocketState::MaxPacingRate", StringValue(pacingRate));
    Config::SetDefault("ns3::TcpSocketState::EnablePacing", BooleanValue(isPacingEnabled));

    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(NUM_NODES); // Create 10 nodes

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Install QUIC on all nodes
    QuicHelper quic;
    quic.InstallQuic(nodes);

    // Set QUIC to use BBR congestion control
    Config::SetDefault("ns3::QuicL4Protocol::SocketType", StringValue("ns3::QuicBbr"));

    NS_LOG_INFO("Create channels.");
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("6Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("15ms"));

    Ipv4AddressHelper address;
    NetDeviceContainer devices;
    int subnet = 1;

    // Create full mesh topology by connecting each node to every other node
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        for (uint32_t j = i + 1; j < nodes.GetN(); ++j) {
            devices = pointToPoint.Install(NodeContainer(nodes.Get(i), nodes.Get(j)));
            std::ostringstream subnetStream;
            subnetStream << "10.1." << subnet++ << ".0";
            address.SetBase(subnetStream.str().c_str(), "255.255.255.0");
            address.Assign(devices);
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NS_LOG_INFO("Create Applications.");
    ApplicationContainer sourceApps;
    ApplicationContainer sinkApps;
    uint16_t port = 10000;

    // Install PacketSink on the last node (server)
    PacketSinkHelper sinkHelper("ns3::QuicSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(NUM_NODES - 1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(DURATION));
    sinkApps.Add(sinkApp);

    Ptr<Ipv4> ipv4Server = nodes.Get(NUM_NODES - 1)->GetObject<Ipv4>();
    Ipv4Address serverAddress = ipv4Server->GetAddress(1, 0).GetLocal();

    // Install BulkSendApplication on the first node (client)
    BulkSendHelper source("ns3::QuicSocketFactory",
                          InetSocketAddress(serverAddress, port));
    source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer clientApp = source.Install(nodes.Get(0));
    sourceApps.Add(clientApp);

    // Schedule trace attachment
    Ptr<Application> app = clientApp.Get(0);
    Simulator::Schedule(Seconds(0.1), &AttachTraces, app);

    // Open output files
    std::string outputDir = "/path/to/sourcens3/folder/desired/output/file/"; //CHANGE THIS
    EnsureDirectoryExists(outputDir);

    std::ofstream throughputFile(outputDir + "quicbbr.throughput");
    std::ofstream rttFile(outputDir + "quicbbr.rtt");
    std::ofstream cwndFile(outputDir + "quicbbr.cwnd");
    std::ofstream packetLossFile(outputDir + "quicbbr.packetloss");

    // Get the PacketSink pointer
    Ptr<PacketSink> sinkPtr = DynamicCast<PacketSink>(sinkApp.Get(0));

    // Schedule the first call to TraceMetrics at t = 1 second
    Simulator::Schedule(Seconds(1.0), &TraceMetrics, sinkPtr, std::ref(throughputFile), std::ref(rttFile), std::ref(cwndFile));

    // Schedule packet loss calculation
    Simulator::Schedule(Seconds(1.0), &CalculatePacketLoss, std::ref(packetLossFile));

    // Connect the callbacks for packet tracking
    sourceApps.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&PacketSentCallback));
    sinkApps.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&PacketReceivedCallback));

    // Start applications
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(DURATION));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(DURATION - 1.0));

    // Run the simulation for the specified duration
    Simulator::Stop(Seconds(DURATION));
    Simulator::Run();

    // Close the output files
    throughputFile.close();
    rttFile.close();
    cwndFile.close();
    packetLossFile.close();

    Simulator::Destroy();

    return 0;
}
