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
    m_resolution = 76800; // 글로벌 변수 RESOLUTION과 같은값 넣어주세요
    m_frameRate = 1;
    m_videoSpeed = 1;   // 이거 골라서 넣으세용 디폴트 1배속
    m_stopCounter = 0;
    m_lastRecvFrame = 0;
    m_rebufferCounter = 0;
    m_bufferEvent = EventId();
    m_sendEvent = EventId();
    for (size_t i = 0; i < TOTAL_VIDEO_FRAME; i++)
    {
      for (size_t j = 0; j < RESOLUTION/MAX_PACKET_SIZE + 1; j++)
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

  uint32_t VideoStreamClient::ReadFromBuffer(void) {
    // 앞으로 5초어치의 프레임 요청
    if(m_lastRecvFrame < MAX_PACKET_SIZE){
      uint8_t send_Buffer[MAX_PACKET_SIZE];
      sprintf((char *)send_Buffer, "res:%u lrf:%u fr:%u", m_resolution, m_lastRecvFrame, m_frameRate * m_videoSpeed);
      Ptr<Packet> firstPacket = Create<Packet>(send_Buffer, MAX_PACKET_SIZE);
      m_socket->Send(firstPacket);

      m_bufferEvent = Simulator::Schedule(Seconds(1.0), &VideoStreamClient::ReadFromBuffer, this);
      
      return (m_currentBufferSize);
    } else {
      return(-1);
    }

    /*
    // NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << " s, last buffer size: " << m_lastBufferSize << ", current buffer size: " << m_currentBufferSize);
    if (m_currentBufferSize < m_frameRate) {
      if (m_lastBufferSize == m_currentBufferSize) {
        m_stopCounter++;
        // If the counter reaches 3, which means the client has been waiting for 3 sec, and no packets arrived.
        // In this case, we think the video streaming has finished, and there is no need to schedule the event.
        if (m_stopCounter < 3) {
          m_bufferEvent = Simulator::Schedule(Seconds(1.0), &VideoStreamClient::ReadFromBuffer, this);
        }
      } else {
        NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << " s: Not enough frames in the buffer, rebuffering!");
        m_stopCounter = 0; // reset the stopCounter
        m_rebufferCounter++;
        m_bufferEvent = Simulator::Schedule(Seconds(1.0), &VideoStreamClient::ReadFromBuffer, this);
      }
      m_lastBufferSize = m_currentBufferSize;
      return (-1);
    } else {
      NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << " s: Play video frames from the buffer");
      if (m_stopCounter > 0)
        m_stopCounter = 0; // reset the stopCounter
      if (m_rebufferCounter > 0)
        m_rebufferCounter = 0; // reset the rebufferCounter
      m_currentBufferSize -= m_frameRate * m_videoSpeed;

      m_bufferEvent = Simulator::Schedule(Seconds(1.0), &VideoStreamClient::ReadFromBuffer, this);
      m_lastBufferSize = m_currentBufferSize;
      return (m_currentBufferSize);
    }
    */
  }

  void VideoStreamClient::HandleRead(Ptr<Socket> socket) {
    NS_LOG_FUNCTION(this << socket);

    Ptr<Packet> packet;
    Address from;
    Address localAddress;
    while ((packet = socket->RecvFrom(from))) {
      socket->GetSockName(localAddress);
      if (InetSocketAddress::IsMatchingType(from)) {
        uint8_t recvData[packet->GetSize()];
        packet->CopyData(recvData, packet->GetSize());
        uint32_t frameNum;
        uint32_t packetNum; 
        sscanf((char *)recvData, "fn:%u pn:%u", &frameNum, &packetNum);
        // 들어온거 체크
        m_FramePacketCounter[frameNum][packetNum] = true;

        // printf("recv %d, %d\n", frameNum, packetNum);
        // R 5
        // M 2
        // p_idx = {0, 1, 2}      {0, 1, 2}차있어야함
        
        // R 4
        // M 2
        // p_idx = {0, 1, 2}      {0, 1}차있어야함

        // 마지막 프레임의 모든패킷이 다 들어왔는지 체크
        bool BREaK = false;
        for (int frame_idx = m_lastRecvFrame; frame_idx < TOTAL_VIDEO_FRAME; frame_idx++) {
          //printf("%d\n", m_lastRecvFrame);
          for (int p_idx = 0; p_idx < RESOLUTION / MAX_PACKET_SIZE + 1; p_idx++) {
            // false 있으면 바로나가
            if(m_FramePacketCounter[frame_idx][p_idx] == false) {
              BREaK = true;
              break;
            // false 없고
            } else {
              // RESOLUTION 가 MAX_PACKET_SIZE로 떨어질때
              if (RESOLUTION % MAX_PACKET_SIZE == 0){
                // 프레임의 마지막 패킷이 도착한것이 확인되었을 경우 (R 4, M 2, p_idx = 1)
                if (RESOLUTION / MAX_PACKET_SIZE == p_idx + 1){
                  m_lastRecvFrame = frame_idx + 1;
                  break;
                }
              // RESOLUTION 가 MAX_PACKET_SIZE로 안떨어질때
              } else {
                if (RESOLUTION / MAX_PACKET_SIZE == p_idx){
                  m_lastRecvFrame = frame_idx + 1;
                  break;
                }
              }
            }
          }
          if(BREaK == true){
            break;
          }
        }
        // 프레임 사이즈 변경은 고민해보시고 작성해주세요!
        // ** Your Code Here ** //
      }
    }
  }

} // namespace ns3








/*
        if (frameNum == m_lastRecvFrame) {
          m_frameSize += packet->GetSize();
        } else {
          if (frameNum > 0) {
            NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s client received frame " << frameNum - 1 << " and " << m_frameSize << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(from).GetPort());
          }
          m_currentBufferSize++;
          m_lastRecvFrame = frameNum;
          m_frameSize = packet->GetSize();
        }

        // The rebuffering event has happend 3+ times, which suggest the client to lower the video quality.
        if (m_rebufferCounter >= 3) {
          if (m_videoLevel > 1) {
            NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s: Lower the video quality level!");
            m_videoLevel--;
            // reflect the change to the server
            uint8_t dataBuffer[10];
            sprintf((char *)dataBuffer, "%hu", m_videoLevel);
            Ptr<Packet> levelPacket = Create<Packet>(dataBuffer, 10);
            socket->SendTo(levelPacket, 0, from);
            m_rebufferCounter = 0;
          }
        }

        // If the current buffer size supports 5+ seconds video, we can try to increase the video quality level.
        if (m_currentBufferSize > 5 * m_frameRate) {
          if (m_videoLevel < MAX_VIDEO_LEVEL) {
            m_videoLevel++;
            // reflect the change to the server
            uint8_t dataBuffer[10];
            sprintf((char *)dataBuffer, "%hu", m_videoLevel);
            Ptr<Packet> levelPacket = Create<Packet>(dataBuffer, 10);
            socket->SendTo(levelPacket, 0, from);
            m_currentBufferSize = m_frameRate;
            NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s: Increase the video quality level to " << m_videoLevel);
          }
        }
      }
    }
  }

} // namespace ns3
*/