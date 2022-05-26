#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VideoStream");

int main(int argc, char *argv[])
{
    uint32_t type = 1; //Topology type

    CommandLine cmd;
    cmd.AddValue("type","Topology type", type);
    
    cmd.Parse(argc,argv);

    if(type==1){//Type1 ==> Simple 1:1 P2P
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("5ms"));

        NetDeviceContainer devices;
        devices = p2p.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper addr;
        addr.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = addr.Assign(devices);

        /*
            App
        */


        p2p.EnablePcapAll("type1");

    }
    else if(type==2){//Type2 ==> Simple 1:1 wifi
        NodeContainer wifiStaNode;
        wifiStaNode.Create(1);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("VideoStream");
        mac.SetType("ns3::StaWifiMac","Ssid",SsidValue(ssid),
        "ActiveProbing",BooleanValue(false));
        

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
        "DataMode",StringValue("HtMcs7"),"ControlMode", StringValue("HtMcs0"));

        NetDeviceContainer staDevice;
        staDevice = wifi.Install(phy,mac,wifiStaNode);

        mac.SetType("ns3::ApWifiMac","Ssid",SsidValue(ssid),
        "BeaconInterval",TimeValue(MicroSeconds(102400)),
        "BeaconGeneration",BooleanValue(true));

        NetDeviceContainer apDevice;
        apDevice = wifi.Install(phy,mac,wifiApNode);

        InternetStackHelper stack;
        stack.Install(wifiApNode);
        stack.Install(wifiStaNode);

        Ipv4AddressHelper addr;
        addr.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer StaInterface = addr.Assign(staDevice);
        Ipv4InterfaceContainer ApInterface = addr.Assign(apDevice);

        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

        positionAlloc->Add(Vector(0.0, 0.0, 0.0));
        positionAlloc->Add(Vector(1.0, 0.0, 0.0));

        mobility.SetPositionAllocator(positionAlloc);

        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiApNode);
        mobility.Install(wifiStaNode);
        /*
            App
        */

    }

    Simulator::Run();
    Simulator::Stop();
    Simulator::Destroy();
    return 0;
}
