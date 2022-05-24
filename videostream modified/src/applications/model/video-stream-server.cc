/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

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
                            .AddAttribute("Interval", "The time to wait between packets",
                                          TimeValue(Seconds(0.01)),
                                          MakeTimeAccessor(&VideoStreamServer::m_interval),
                                          MakeTimeChecker())
                            .AddAttribute("Port", "Port on which we listen for incoming packets.",
                                          UintegerValue(5000),
                                          MakeUintegerAccessor(&VideoStreamServer::m_port),
                                          MakeUintegerChecker<uint16_t>())
        ;
    return tid;
  }

  VideoStreamServer::VideoStreamServer() {
    NS_LOG_FUNCTION(this);
    m_socket = 0;
    // m_frameRate = 25;
    // m_frameSizeList = std::vector<uint32_t>();
  }

  VideoStreamServer::~VideoStreamServer() {
    NS_LOG_FUNCTION(this);
    m_socket = 0;
  }

  void VideoStreamServer::DoDispose(void) {
    NS_LOG_FUNCTION(this);
    Application::DoDispose();
  }
  // 소캣 연결하고 기본적인 세팅 하는 부분
  void VideoStreamServer::StartApplication(void) {
    NS_LOG_FUNCTION(this);
    if (m_socket == 0) {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket(GetNode(), tid);
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
      if (m_socket->Bind(local) == -1) {
        NS_FATAL_ERROR("Failed to bind socket");
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

  // 이제 사용하지 않습니다.
  // 이전에는 프레임마다 화질이 다른 영상을 보내는것을 표현할때 사용했던 것입니다.
  // 저희는 client의 요청에 맞는 영상을 서버가 무조건 가지고 있다고 가정하기 때문에
  // 이런 통제된 변인은 삭제하였습니다.
  std::string VideoStreamServer::GetFrameFile(void) const {
    NS_LOG_FUNCTION(this);
    return m_frameFile;
  }

  uint32_t VideoStreamServer::GetMaxPacketSize(void) const {
    return MAX_PACKET_SIZE;
  }

  // 패킷을 어디서부터 얼마나 보낼지 결정하는 부분
  void VideoStreamServer::Send(uint32_t ipAddress) {
    NS_LOG_FUNCTION(this);

    uint32_t resolution;
    ClientInfo *clientInfo = m_clients.at(ipAddress);

    NS_ASSERT(clientInfo->m_sendEvent.IsExpired());
    // 클라이언트가 요청하는 resolution의 비디오를 보내는 부분입니다.
    resolution = clientInfo->m_videoLevel;//!!여기 m_videoLevel 살리면 될듯 (resolution idx)

    // 클라이언트가 요청한 프레임부터 클라이언트가 5초간 소비할 프레임에 대해 send합니다.
    for (uint frame_idx = clientInfo->m_sent; frame_idx < clientInfo->m_sent + clientInfo->m_frameRate * 5; frame_idx++) {
      // 클라이언트가 영상을 끝까지 수신했다면 더이상 보내지 않습니다.
      if (frame_idx == TOTAL_VIDEO_FRAME) {
        break;
      // 그렇지 않다면 영상을 송신합니다.
      } else {
        // 프레임을 패킷으로 더 잘게 나눠서 송신합니다.
        for (uint packet_idx = 0; packet_idx < resolution / MAX_PACKET_SIZE + 1; packet_idx++) {
          // 프레임이 패킷 사이즈로 나눠떨어지는지 부분입니다. client에서도 찾아보실 수 있습니다.
          if (resolution % MAX_PACKET_SIZE == 0 && packet_idx + 1 == resolution / MAX_PACKET_SIZE) {
            break;
          // 패킷을 송신하는 부분입니다.
          } else {
            SendPacket(clientInfo, frame_idx, packet_idx);
            NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s server sent frame " << frame_idx << " and " << resolution << " bytes to " << InetSocketAddress::ConvertFrom(clientInfo->m_address).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(clientInfo->m_address).GetPort());
          }
        }
      }
    }
  }

  // 패킷을 보내는 부분
  void VideoStreamServer::SendPacket(ClientInfo *client, uint frame_idx, uint packet_idx) {
    // MAX_PACKET_SIZE에 몇번째 프레임의 몇번째 패킷인지를 담아서 송신합니다. 
    uint8_t send_Buffer[MAX_PACKET_SIZE];
    sprintf((char *)send_Buffer, "fn:%u pn:%u", frame_idx, packet_idx);
    Ptr<Packet> firstPacket = Create<Packet>(send_Buffer, MAX_PACKET_SIZE);
    m_socket->Send(firstPacket);
    if (m_socket->SendTo(firstPacket, 0, client->m_address) < 0) {
      NS_LOG_INFO("Error while sending " << MAX_PACKET_SIZE << "bytes to " << InetSocketAddress::ConvertFrom(client->m_address).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(client->m_address).GetPort());
    }
  }

  // 클라이언트로부터 받은 데이터를 해석하는 부분
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
        // 클라이언트로부터 읽어온 데이터를 해석합니다. 요청하는 화질, 요청하는 프레임 시작점, 요청하는 프레임 수 의 정보가 들어있습니다. 
        uint8_t recvData[MAX_PACKET_SIZE];
        packet->CopyData(recvData, MAX_PACKET_SIZE);
        sscanf((char *)recvData, "res:%u lrf:%u fr:%u", &RES, &LRF, &FR);

        // 클라이언트로부터의 첫번째 요청일 경우 client를 새로 등록하는 과정이 필요합니다.
        if (m_clients.find(ipAddr) == m_clients.end()) {
          // 새로운 클라이언트를 등록합니다.
          ClientInfo *newClient = new ClientInfo();
          newClient->m_sent = LRF;
          newClient->m_videoLevel = RES;
          newClient->m_frameRate = FR;
          newClient->m_address = from;
          m_clients[ipAddr] = newClient;
          // 요청에 따라 바로 영상을 보내줍니다.
          if(LRF < TOTAL_VIDEO_FRAME){
            newClient->m_sendEvent = Simulator::Schedule(Seconds(0.0), &VideoStreamServer::Send, this, ipAddr);
          }
        } else {
          // 요청사항을 클라이언트 정보에 기록합니다.
          NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s server received video level " << RES);
          m_clients.at(ipAddr)->m_sent = LRF;
          m_clients.at(ipAddr)->m_videoLevel = RES;
          m_clients.at(ipAddr)->m_frameRate = FR;
          // 요청에 따라 바로 영상을 보내줍니다.
          if(LRF < TOTAL_VIDEO_FRAME){
            m_clients.at(ipAddr)->m_sendEvent = Simulator::Schedule(Seconds(0.0), &VideoStreamServer::Send, this, ipAddr);
          }
        }
      }
    }
  }

} // namespace ns3
