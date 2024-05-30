#include "ns3/command-line.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/config.h"
#include "ns3/trace-helper.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/epc-helper.h"
#include "ns3/lte-helper.h"
#include "ns3/point-to-point-epc-helper.h"
#include "ns3/enum.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/application-container.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Simple5GNetworkWithMobilityExample");

uint32_t packetsSent = 0;
uint32_t packetsReceived = 0;
double totalDelay = 0.0;

void PacketSentCallback(Ptr<const Packet> packet) {
    packetsSent++;
    NS_LOG_INFO("Packet sent at " << Simulator::Now().GetSeconds() << " seconds");
}

void PacketReceivedCallback(Ptr<const Packet> packet, const Address &address) {
    packetsReceived++;
    double delay = Simulator::Now().GetSeconds() - packet->GetUid();
    totalDelay += delay;
    NS_LOG_INFO("Packet received at " << Simulator::Now().GetSeconds() << " seconds with delay " << delay);
}

int main(int argc, char *argv[]) {
    // Log ayarları
    LogComponentEnable("Simple5GNetworkWithMobilityExample", LOG_LEVEL_ALL);
    LogComponentEnable("EpcHelper", LOG_LEVEL_INFO);
    LogComponentEnable("LteHelper", LOG_LEVEL_ALL);
    LogComponentEnableAll(LOG_PREFIX_FUNC);
    LogComponentEnableAll(LOG_PREFIX_NODE);
    LogComponentEnableAll(LOG_PREFIX_TIME);

    // Komut satırı parametreleri
    std::string traceFile = "/home/cagla/ns-3-dev/scratch/mobility.ns2";
    CommandLine cmd;
    cmd.AddValue("traceFile", "NS2 movement trace file", traceFile);
    cmd.Parse(argc, argv);

    // Trace file check
    std::ifstream file(traceFile.c_str());
    if (!file.is_open()) {
        NS_LOG_UNCOND("Could not open trace file " << traceFile << " for reading, aborting here");
        return 1;
    } else {
        NS_LOG_UNCOND("Trace file opened successfully!");
        file.close();
    }

    /************************
     *  LTE ve EPC Yardımcıları  *
     ************************/
    NS_LOG_INFO("Setting up LTE and EPC helpers...");

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    /************************
     *  Mobilite Ayarları  *
     ************************/
    NS_LOG_INFO("Setting up mobility...");

    MobilityHelper mobility;
    Ns2MobilityHelper ns2 = Ns2MobilityHelper(traceFile);
    ns2.Install();  // İz dosyasından mobilite modelini yükle

    /************************
     *  UE ve eNodeB oluşturma *
     ************************/
    NS_LOG_INFO("Creating UE and eNodeB nodes...");

    NodeContainer ueNodes;
    NodeContainer enbNodes;
    ueNodes.Create(2);
    enbNodes.Create(1);

    // Mobilite modeli kur
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);

    // İz dosyasından düğümlere mobilite modeli ata
    ns2.Install();

    /************************
     *  Net Cihazları Kurulum *
     ************************/
    NS_LOG_INFO("Installing LTE Devices...");

    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

    /************************
     *  IP Ayarları  *
     ************************/
    NS_LOG_INFO("Assigning IP addresses...");

    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        Ptr<Node> ueNode = ueNodes.Get(i);
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ueNode->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    /************************
     *  Uygulama Kurulumu  *
     ************************/
    NS_LOG_INFO("Installing applications...");

    uint16_t dlPort = 1234;
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    UdpServerHelper dlPacketSinkHelper(dlPort);
    serverApps.Add(dlPacketSinkHelper.Install(ueNodes.Get(0)));

    UdpClientHelper dlClient(ueIpIface.GetAddress(0), dlPort);
    dlClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    dlClient.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(dlClient.Install(pgw));

    // Paket gönderimi ve alımını izlemek için bağlantı yap
    Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&PacketReceivedCallback));

    serverApps.Start(Seconds(0.01));
    clientApps.Start(Seconds(0.01));

    /****************
     *  Simülasyon  *
     ****************/
    Simulator::Stop(Seconds(800));
    Simulator::Run();
    Simulator::Destroy();

    // PDR ve ortalama gecikmeyi hesapla ve yazdır
    double pdr = static_cast<double>(packetsReceived) / packetsSent;
    double avgDelay = packetsReceived > 0 ? totalDelay / packetsReceived : 0.0;
    std::cout << "Packet Delivery Rate: " << pdr << std::endl;
    std::cout << "Average Delay: " << avgDelay << " seconds" << std::endl;

    return 0;
}
