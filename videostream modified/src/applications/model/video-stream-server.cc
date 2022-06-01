#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/address-utils.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "video-stream-server.h"

namespace ns3 {

  NS_LOG_COMPONENT_DEFINE("VideoStreamServerApplication");

  NS_OBJECT_ENSURE_REGISTERED(VideoStreamServer);

  TypeId VideoStreamServer::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::VideoStreamServer")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<VideoStreamServer>()
                            .AddAttribute("Interval", "Time to wait",
                                          TimeValue(Seconds(0.01)),
                                          MakeTimeAccessor(&VideoStreamServer::m_interval),
                                          MakeTimeChecker())
                            .AddAttribute("Port", "Port for listening incoming packets",
                                          UintegerValue(5000),
                                          MakeUintegerAccessor(&VideoStreamServer::m_port),
                                          MakeUintegerChecker<uint16_t>())
        ;
    return tid;
  }

  VideoStreamServer::VideoStreamServer() {
    NS_LOG_FUNCTION(this);
    m_socket = 0;
  }

  VideoStreamServer::~VideoStreamServer() {
    NS_LOG_FUNCTION(this);
    m_socket = 0;
  }

  void VideoStreamServer::DoDispose(void) {
    NS_LOG_FUNCTION(this);
    Application::DoDispose();
  }
  void VideoStreamServer::StartApplication(void) {
    NS_LOG_FUNCTION(this);
    if (m_socket == 0) {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket(GetNode(), tid);
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
      if (m_socket->Bind(local) == -1) {
        NS_FATAL_ERROR("Error: Failed to bind socket");
      } if (addressUtils::IsMulticast(m_local)) {
        Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket>(m_socket);
        if (udpSocket) {
          udpSocket->MulticastJoinGroup(0, m_local);
        } else {
          NS_FATAL_ERROR("Error: Failed to join multicast group");
        }
      }
    }

    m_socket->SetAllowBroadcast(true);

    m_socket->SetRecvCallback(MakeCallback(&VideoStreamServer::HandleRead, this));
  }

  void VideoStreamServer::StopApplication() {
    NS_LOG_FUNCTION(this);

    if (m_socket != 0) {
      m_socket->Close();
      m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
      m_socket = 0;
    }
    for (auto iter = m_clients.begin(); iter != m_clients.end(); iter++) {
      Simulator::Cancel(iter->second->m_sendEvent);
    }
  }

  uint32_t VideoStreamServer::GetMaxPacketSize(void) const {
    return MAX_PACKET_SIZE;
  }

  void VideoStreamServer::Send(uint32_t ipAddress) {
    NS_LOG_FUNCTION(this);

    uint32_t resolution;
    ClientInfo *clientInfo = m_clients.at(ipAddress);

    NS_ASSERT(clientInfo->m_sendEvent.IsExpired());
    resolution = clientInfo->m_videoLevel;

    for (uint frame_idx = clientInfo->m_sent; frame_idx < clientInfo->m_sent + clientInfo->m_frameRate * 5; frame_idx++) {
      if (frame_idx == TOTAL_VIDEO_FRAME) {
        break;
      } else {
        for (uint packet_idx = 0; packet_idx < resolution / MAX_PACKET_SIZE + 1; packet_idx++) {
          if (resolution % MAX_PACKET_SIZE == 0 && packet_idx + 1 == resolution / MAX_PACKET_SIZE) {
            break;
          } else {
            SendPacket(clientInfo, frame_idx, packet_idx);
            NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s server sent frame " << frame_idx << " and " << resolution << " bytes to " << InetSocketAddress::ConvertFrom(clientInfo->m_address).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(clientInfo->m_address).GetPort());
          }
        }
      }
    }
  }

  void VideoStreamServer::SendPacket(ClientInfo *client, uint frame_idx, uint packet_idx) {
    uint8_t send_Buffer[MAX_PACKET_SIZE];
    sprintf((char *)send_Buffer, "fn:%u pn:%u", frame_idx, packet_idx);
    Ptr<Packet> firstPacket = Create<Packet>(send_Buffer, MAX_PACKET_SIZE);
    m_socket->Send(firstPacket);
    if (m_socket->SendTo(firstPacket, 0, client->m_address) < 0) {
      NS_LOG_INFO("Error while sending " << MAX_PACKET_SIZE << "bytes to " << InetSocketAddress::ConvertFrom(client->m_address).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(client->m_address).GetPort());
    }
  }

  void VideoStreamServer::HandleRead(Ptr<Socket> socket) {
    NS_LOG_FUNCTION(this << socket);

    Ptr<Packet> packet;
    Address from;
    Address localAddress;
    while ((packet = socket->RecvFrom(from)))
    {
      socket->GetSockName(localAddress);
      if (InetSocketAddress::IsMatchingType(from))
      {
        NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s server received " << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(from).GetPort());

        uint32_t ipAddr = InetSocketAddress::ConvertFrom(from).GetIpv4().Get();

        uint32_t RES;
        uint32_t LRF;
        uint32_t FR;
        uint8_t recvData[MAX_PACKET_SIZE];
        packet->CopyData(recvData, MAX_PACKET_SIZE);
        sscanf((char *)recvData, "res:%u lrf:%u fr:%u", &RES, &LRF, &FR);

        if (m_clients.find(ipAddr) == m_clients.end()) {
          ClientInfo *newClient = new ClientInfo();
          newClient->m_sent = LRF;
          newClient->m_videoLevel = RES;
          newClient->m_frameRate = FR;
          newClient->m_address = from;
          m_clients[ipAddr] = newClient;
          if(LRF < TOTAL_VIDEO_FRAME){
            newClient->m_sendEvent = Simulator::Schedule(Seconds(0.0), &VideoStreamServer::Send, this, ipAddr);
          }
        } else {
          NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s server received video level " << RES);
          m_clients.at(ipAddr)->m_sent = LRF;
          m_clients.at(ipAddr)->m_videoLevel = RES;
          m_clients.at(ipAddr)->m_frameRate = FR;
          if(LRF < TOTAL_VIDEO_FRAME){
            m_clients.at(ipAddr)->m_sendEvent = Simulator::Schedule(Seconds(0.0), &VideoStreamServer::Send, this, ipAddr);
          }
        }
      }
    }
  }

} // namespace ns3
