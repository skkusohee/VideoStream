/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "video-stream-client.h"

namespace ns3
{
  NS_LOG_COMPONENT_DEFINE("VideoStreamClientApplication");

  NS_OBJECT_ENSURE_REGISTERED(VideoStreamClient);

  TypeId VideoStreamClient::GetTypeId(void)
  {
    static TypeId tid = TypeId("ns3::VideoStreamClient")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<VideoStreamClient>()
                            .AddAttribute("RemoteAddress", "The destination address of the outbound packets",
                                          AddressValue(),
                                          MakeAddressAccessor(&VideoStreamClient::m_peerAddress),
                                          MakeAddressChecker())
                            .AddAttribute("RemotePort", "The destination port of the outbound packets",
                                          UintegerValue(5000),
                                          MakeUintegerAccessor(&VideoStreamClient::m_peerPort),
                                          MakeUintegerChecker<uint16_t>())

        ;
    return tid;
  }

  VideoStreamClient::VideoStreamClient()
  {
    NS_LOG_FUNCTION(this);
    m_initialDelay = 1;
    m_lastBufferSize = 0;
    m_currentBufferSize = 0;
    m_frameSize = 0;
    //m_resolution = 76800; // 글로벌 변수 RESOLUTION과 같은값 넣어주세요
    m_frameRate = 10;
    m_videoSpeed = 1;     // 이거 골라서 넣으세용 디폴트 1배속
    m_stopCounter = 0;
    m_lastRecvFrame = 0;
    m_rebufferCounter = 0;
    m_videotime = 0;
    m_bufferEvent = EventId();
    m_sendEvent = EventId();
    //!!!
    m_resolutionArray[0] = 76800;
    m_resolutionArray[1] = 230400;
    m_resolutionArray[2] = 409920;
    m_resolutionArray[3] = 921600;
    m_resolutionArray[4] = 2073600;
    m_videoLevel = 1;
    m_resolution = m_resolutionArray[m_videoLevel];
    for (size_t i = 0; i < TOTAL_VIDEO_FRAME; i++)
    {
      for (size_t j = 0; j < m_resolution/MAX_PACKET_SIZE + 1; j++)
      {
        m_FramePacketCounter[i][j] = false;
      }
    }
  }

  VideoStreamClient::~VideoStreamClient()
  {
    NS_LOG_FUNCTION(this);
    m_socket = 0;
  }

  void VideoStreamClient::SetRemote(Address ip, uint16_t port)
  {
    NS_LOG_FUNCTION(this << ip << port);
    m_peerAddress = ip;
    m_peerPort = port;
  }

  void VideoStreamClient::SetRemote(Address addr)
  {
    NS_LOG_FUNCTION(this << addr);
    m_peerAddress = addr;
  }

  void VideoStreamClient::DoDispose(void)
  {
    NS_LOG_FUNCTION(this);
    Application::DoDispose();
  }

  void VideoStreamClient::StartApplication(void)
  {
    NS_LOG_FUNCTION(this);

    if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket(GetNode(), tid);
      if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
      {
        if (m_socket->Bind() == -1)
        {
          NS_FATAL_ERROR("Failed to bind socket");
        }
        m_socket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
      }
      else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
      {
        if (m_socket->Bind6() == -1)
        {
          NS_FATAL_ERROR("Failed to bind socket");
        }
        m_socket->Connect(m_peerAddress);
      }
      else if (InetSocketAddress::IsMatchingType(m_peerAddress) == true)
      {
        if (m_socket->Bind() == -1)
        {
          NS_FATAL_ERROR("Failed to bind socket");
        }
        m_socket->Connect(m_peerAddress);
      }
      else if (Inet6SocketAddress::IsMatchingType(m_peerAddress) == true)
      {
        if (m_socket->Bind6() == -1)
        {
          NS_FATAL_ERROR("Failed to bind socket");
        }
        m_socket->Connect(m_peerAddress);
      }
      else
      {
        NS_ASSERT_MSG(false, "Incompatible address type: " << m_peerAddress);
      }
    }
    // 소캣 정리하는 부분 콜백
    m_socket->SetRecvCallback(MakeCallback(&VideoStreamClient::HandleRead, this));
    // 요청넣는 부분
    m_sendEvent = Simulator::Schedule(MilliSeconds(1.0), &VideoStreamClient::Send, this);
    // 소비하는 부분
    m_bufferEvent = Simulator::Schedule(Seconds(m_initialDelay), &VideoStreamClient::ReadFromBuffer, this);
  }

  void VideoStreamClient::StopApplication()
  {
    NS_LOG_FUNCTION(this);

    if (m_socket != 0)
    {
      m_socket->Close();
      m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
      m_socket = 0;
    }

    Simulator::Cancel(m_bufferEvent);
  }

  void VideoStreamClient::Send(void)
  {
    NS_LOG_FUNCTION(this);
    NS_ASSERT(m_sendEvent.IsExpired());

    uint8_t send_Buffer[MAX_PACKET_SIZE];
    sprintf((char *)send_Buffer, "res:%u lrf:%u fr:%u", m_resolution, m_lastRecvFrame, m_frameRate * m_videoSpeed);

    //TEST~
    uint32_t RES;
    uint32_t LRF;
    uint32_t FR;
    sscanf((char *)send_Buffer, "res:%u lrf:%u fr:%u", &RES, &LRF, &FR);
    printf("%d, %d, %d\n", RES, LRF, FR);
    //~TEST

    Ptr<Packet> firstPacket = Create<Packet>(send_Buffer, MAX_PACKET_SIZE);
    m_socket->Send(firstPacket);

    // logger
    if (Ipv4Address::IsMatchingType(m_peerAddress))
    {
      NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s client sent 10 bytes to " << Ipv4Address::ConvertFrom(m_peerAddress) << " port " << m_peerPort);
    }
    else if (Ipv6Address::IsMatchingType(m_peerAddress))
    {
      NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s client sent 10 bytes to " << Ipv6Address::ConvertFrom(m_peerAddress) << " port " << m_peerPort);
    }
    else if (InetSocketAddress::IsMatchingType(m_peerAddress))
    {
      NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s client sent 10 bytes to " << InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(m_peerAddress).GetPort());
    }
    else if (Inet6SocketAddress::IsMatchingType(m_peerAddress))
    {
      NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s client sent 10 bytes to " << Inet6SocketAddress::ConvertFrom(m_peerAddress).GetIpv6() << " port " << Inet6SocketAddress::ConvertFrom(m_peerAddress).GetPort());
    }
  }
  // 받아서 정리해놓은 프레임들을 소비하고 서버에 5초간의 영상을 요청하는 역할
  uint32_t VideoStreamClient::ReadFromBuffer(void) {
    //************요약***************
    // 원래는 여기서 프레임이 늘어나는지를 계산해서
    // 늘어나지 않는 상태가 3초면 영상이 끝났다고 판별하는 부분이 있었고
    // 늘어나지만 소비할 수 없을 정도로 늘어나면 버퍼링이 들어간다고 판별하는 부분이 있었습니다.
    // 하지만 새로 짠 코드에서는 TOTAL_VIDEO_FRAME으로 영상의 총 길이를 저장하고 있기 때문에
    // 이를통해 영상이 끝났는지를 판별할 수 있어서 3초동안 정지면 영상끝 부분 제거하였고
    // 버퍼링 측정 코드는 사용하시려면 추가 하셔야 할 수 있습니다.
    printf("확보중인 프레임 : %d, 버퍼링 횟수 : %d\n", m_currentBufferSize, m_rebufferCounter);

    // 소비할 수 없는만큼 버퍼에 영상의 프레임이 남아있는 경우
    if(m_currentBufferSize < m_frameRate * m_videoSpeed){
      // 마지막 수신한 프레임이 끝 프레임이 아니라면 즉 영상을 아직 받아야 하는 상태라면
      if(m_lastRecvFrame < TOTAL_VIDEO_FRAME){
        // 버퍼링 케이스에 해당합니다. 
        m_rebufferCounter++;
        if(m_lastRecvFrame < TOTAL_VIDEO_FRAME){
          // 패킷에 수신하고자 하는 화질, 몇번째 프레임부터 서버에서 송신해주면 되는지, 초당 몇개의 프레임을 소비하는지 의 데이터를 담아서 서버로 요청을 넣습니다.
          // 서버쪽에서는 요청을 받으면 몇번째 프레임부터 client가 받고싶어하는지 에서부터 앞으로 5초간의 영상(초당 몇개 프레임 소비하는지 * 5) 만큼의 영상을 보내주게 됩니다.
          uint8_t send_Buffer[MAX_PACKET_SIZE];
          sprintf((char *)send_Buffer, "res:%u lrf:%u fr:%u", m_resolution, m_lastRecvFrame, m_frameRate * m_videoSpeed);
          Ptr<Packet> firstPacket = Create<Packet>(send_Buffer, MAX_PACKET_SIZE);
          m_socket->Send(firstPacket);
        }
        //NS_LOG_INFO(Simulator::Now().GetSeconds() << "\tBuffercount\t" << m_rebufferCounter);
        m_bufferEvent = Simulator::Schedule(Seconds(1.0), &VideoStreamClient::ReadFromBuffer, this);
      } else{
        printf("영상끝났음\n");
        // 영상이 모두 수신되었고 자투리 부분이 남아있던 것이라면 남은 영상 프레임을 클리어 해줍니다.
        m_currentBufferSize = 0;
      }
      NS_LOG_UNCOND("0\t" << Simulator::Now().GetSeconds() << "\t" << m_rebufferCounter);
      NS_LOG_UNCOND("1\t" << Simulator::Now().GetSeconds() << "\t" << m_videotime);
      m_bufferEvent = Simulator::Schedule(Seconds(1.0), &VideoStreamClient::ReadFromBuffer, this);
      return(-1);
    // 소비 가능한 만큼 영상이 남아있다면
    } else {
      // 1초어치 영상을 소비합니다.
      m_videotime += 1;
      printf("                                               영상 소비 %d\n", m_frameRate * m_videoSpeed);
      m_currentBufferSize -= m_frameRate * m_videoSpeed;
      // 영상이 최근 재생되었으므로 버퍼링 횟수도 초기화 합니다.
      m_rebufferCounter = 0;
      // 마지막 수신한 프레임이 끝 프레임이 아니라면 즉 영상을 아직 받아야 하는 상태라면
      if(m_lastRecvFrame < TOTAL_VIDEO_FRAME){
        // 패킷에 수신하고자 하는 화질, 몇번째 프레임부터 서버에서 송신해주면 되는지, 초당 몇개의 프레임을 소비하는지 의 데이터를 담아서 서버로 요청을 넣습니다.
        // 서버쪽에서는 요청을 받으면 몇번째 프레임부터 client가 받고싶어하는지 에서부터 앞으로 5초간의 영상(초당 몇개 프레임 소비하는지 * 5) 만큼의 영상을 보내주게 됩니다.
        uint8_t send_Buffer[MAX_PACKET_SIZE];
        sprintf((char *)send_Buffer, "res:%u lrf:%u fr:%u", m_resolution, m_lastRecvFrame, m_frameRate * m_videoSpeed);
        Ptr<Packet> firstPacket = Create<Packet>(send_Buffer, MAX_PACKET_SIZE);
        m_socket->Send(firstPacket);
      }
      // 1초뒤에 다시 자신을 실행합니다.
      NS_LOG_UNCOND("0\t" << Simulator::Now().GetSeconds() << "\t" << m_rebufferCounter);
      NS_LOG_UNCOND("1\t" << Simulator::Now().GetSeconds() << "\t" << m_videotime);
      m_bufferEvent = Simulator::Schedule(Seconds(1.0), &VideoStreamClient::ReadFromBuffer, this);
      return (m_currentBufferSize);
    }
  }
  // 패킷을 통해 프레임 조각들을 받아서 프레임을 만드는 역할
  void VideoStreamClient::HandleRead(Ptr<Socket> socket) {
    NS_LOG_FUNCTION(this << socket);

    Ptr<Packet> packet;
    Address from;
    Address localAddress;
    while ((packet = socket->RecvFrom(from))) {
      socket->GetSockName(localAddress);
      if (InetSocketAddress::IsMatchingType(from)) {
        // 서버에서 수신한 패킷에는 영상의 프레임 번호와 패킷 번호가 들어있습니다.
        // 하나의 영상은 사이즈가 클 경우 여러 작은 패킷으로 쪼개져 보내지게 됩니다. 
        uint8_t recvData[packet->GetSize()];
        packet->CopyData(recvData, packet->GetSize());
        uint32_t frameNum;
        uint32_t packetNum;
        sscanf((char *)recvData, "fn:%u pn:%u", &frameNum, &packetNum);
        // 수신한 패킷에 대해서 수신 체크를 해줍니다. 
        m_FramePacketCounter[frameNum][packetNum] = true;

        bool BREaK = false;
        // 최근 들어온 프레임들에 대해서
        for (int frame_idx = m_lastRecvFrame; frame_idx < TOTAL_VIDEO_FRAME; frame_idx++) {
          // 해당 프레임들의 모든 패킷에 대해서 잘 수신되었는지 확인한다.
          for (int p_idx = 0; p_idx < int(m_resolution) / MAX_PACKET_SIZE + 1; p_idx++) {
            // false는 수신이 안된것을 표시하므로 바로 break합니다.
            if(m_FramePacketCounter[frame_idx][p_idx] == false) {
              BREaK = true;
              break;
            // false 없고
            } else {
              // RESOLUTION 가 MAX_PACKET_SIZE로 떨어질때
              if (int(m_resolution) % MAX_PACKET_SIZE == 0){
                // 프레임의 마지막 패킷이 도착한것이 확인되었을 경우 (R 4, M 2, p_idx = 1)
                if (int(m_resolution) / MAX_PACKET_SIZE == p_idx + 1){
                  // 안정적으로 수신한 프레임을 수정합니다.
                  m_lastRecvFrame = frame_idx + 1;
                  // 소비할 프레임 버퍼 사이즈도 증가시켜줍니다.
                  m_currentBufferSize++;
                  break;
                }
              // RESOLUTION 가 MAX_PACKET_SIZE로 안떨어질때
              } else {
                // 프레임의 마지막 패킷이 도착한것이 확인되었을 경우
                if (int(m_resolution) / MAX_PACKET_SIZE == p_idx){
                  // 안정적으로 수신한 프레임을 수정합니다.
                  m_lastRecvFrame = frame_idx + 1;
                  // 소비할 프레임 버퍼 사이즈도 증가시켜줍니다.
                  m_currentBufferSize++;
                  break;
                }
              }
            }
          }
          if(BREaK == true){
            break;
          }
        }
        //!!!
        if (m_rebufferCounter >= 3){
          if (m_videoLevel > 0){
            m_videoLevel--;
            m_resolution = m_resolutionArray[m_videoLevel];
            uint8_t send_Buffer[MAX_PACKET_SIZE];
            sprintf((char *) send_Buffer, "%hu", m_resolution);
            Ptr<Packet> levelPacket = Create<Packet>(send_Buffer, MAX_PACKET_SIZE);
            socket->SendTo(levelPacket, 0, from);
            m_rebufferCounter = 0;
          }
        }
      }
    }
  }

} // namespace ns3
