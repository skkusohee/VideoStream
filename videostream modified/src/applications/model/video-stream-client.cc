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
                            .AddAttribute("RemoteAddress", "The dest address",
                                          AddressValue(),
                                          MakeAddressAccessor(&VideoStreamClient::m_peerAddress),
                                          MakeAddressChecker())
                            .AddAttribute("RemotePort", "The dest port",
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
    m_frameRate = 5;
    m_videoSpeed = 1.8;
    m_stopCounter = 0;
    m_lastRecvFrame = 0;
    m_rebufferCounter = 0;
    m_videotime = 0;
    m_bufferEvent = EventId();
    m_sendEvent = EventId();
    m_speedxframeRate = m_frameRate*m_videoSpeed;

    m_resolutionArray[0] = 100001; //13
    m_resolutionArray[1] = 150001; //8
    m_resolutionArray[2] = 200001; //7
    m_resolutionArray[3] = 230001; //6
    m_resolutionArray[4] = 250001; //5
    m_resolutionArray[5] = 300001; //4

    m_videoLevel = 5;
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
          NS_FATAL_ERROR("Error: Failed to bind socket");
        }
        m_socket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
      }
      else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
      {
        if (m_socket->Bind6() == -1)
        {
          NS_FATAL_ERROR("Error: Failed to bind socket");
        }
        m_socket->Connect(m_peerAddress);
      }
      else if (InetSocketAddress::IsMatchingType(m_peerAddress) == true)
      {
        if (m_socket->Bind() == -1)
        {
          NS_FATAL_ERROR("Error: Failed to bind socket");
        }
        m_socket->Connect(m_peerAddress);
      }
      else if (Inet6SocketAddress::IsMatchingType(m_peerAddress) == true)
      {
        if (m_socket->Bind6() == -1)
        {
          NS_FATAL_ERROR("Error: Failed to bind socket");
        }
        m_socket->Connect(m_peerAddress);
      }
      else
      {
        NS_ASSERT_MSG(false, "Incompatible address type: " << m_peerAddress);
      }
    }
    m_socket->SetRecvCallback(MakeCallback(&VideoStreamClient::HandleRead, this));
    m_sendEvent = Simulator::Schedule(MilliSeconds(1.0), &VideoStreamClient::Send, this);
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
    sprintf((char *)send_Buffer, "res:%u lrf:%u fr:%u", m_resolution, m_lastRecvFrame, m_speedxframeRate);

    uint32_t RES;
    uint32_t LRF;
    uint32_t FR;
    sscanf((char *)send_Buffer, "res:%u lrf:%u fr:%u", &RES, &LRF, &FR);
    printf("%d, %d, %d\n", RES, LRF, FR);

    Ptr<Packet> firstPacket = Create<Packet>(send_Buffer, MAX_PACKET_SIZE);
    m_socket->Send(firstPacket);
  }
  int flag =0;
  uint32_t VideoStreamClient::ReadFromBuffer(void) {
    if(flag) return(-1);
    printf("확보중인 프레임 : %d, 버퍼링 횟수 : %d\n", m_currentBufferSize, m_rebufferCounter);

    if(m_currentBufferSize < m_speedxframeRate){
      if(m_lastRecvFrame < TOTAL_VIDEO_FRAME){
        m_rebufferCounter++;
        if(m_lastRecvFrame < TOTAL_VIDEO_FRAME){
          uint8_t send_Buffer[MAX_PACKET_SIZE];
          sprintf((char *)send_Buffer, "res:%u lrf:%u fr:%u", m_resolution, m_lastRecvFrame, m_speedxframeRate);
          Ptr<Packet> firstPacket = Create<Packet>(send_Buffer, MAX_PACKET_SIZE);
          m_socket->Send(firstPacket);
        }
        m_bufferEvent = Simulator::Schedule(Seconds(1.0), &VideoStreamClient::ReadFromBuffer, this);
      } else{
        printf("영상끝났음\n");
        m_currentBufferSize = 0;
        m_bufferEvent = Simulator::Schedule(Seconds(1.0), &VideoStreamClient::ReadFromBuffer, this);
        flag =1;
      }

      if (m_rebufferCounter >= 1){

          if (m_videoLevel > 0){
            m_videoLevel--;
             printf("1. videoLevel: %d to %d  \n",m_videoLevel+1, m_videoLevel);

            m_resolution = m_resolutionArray[m_videoLevel];
            uint8_t send_Buffer[MAX_PACKET_SIZE];
            sprintf((char *) send_Buffer, "%hu", m_resolution);
            Ptr<Packet> levelPacket = Create<Packet>(send_Buffer, MAX_PACKET_SIZE);
            m_socket->Send(levelPacket);
          }
        }

      NS_LOG_UNCOND("0\t" << Simulator::Now().GetSeconds() << "\t" << m_rebufferCounter);
      NS_LOG_UNCOND("1\t" << Simulator::Now().GetSeconds() << "\t" << m_videotime);
      NS_LOG_UNCOND("2\t" << Simulator::Now().GetSeconds() << "\t" << m_videoLevel);
      return (-1);
    } else {
      m_videotime += 1;
      printf("                                               영상 소비 %d\n", m_speedxframeRate);
      m_currentBufferSize -= m_speedxframeRate;
      m_rebufferCounter = 0;
      if(m_lastRecvFrame < TOTAL_VIDEO_FRAME){
        uint8_t send_Buffer[MAX_PACKET_SIZE];
        sprintf((char *)send_Buffer, "res:%u lrf:%u fr:%u", m_resolution, m_lastRecvFrame, m_speedxframeRate);
        Ptr<Packet> firstPacket = Create<Packet>(send_Buffer, MAX_PACKET_SIZE);
        m_socket->Send(firstPacket);
      }

      if(m_currentBufferSize >= m_speedxframeRate){
          if(m_videoLevel<5){

            m_videoLevel++;
            printf("2. videoLevel: %d to %d  \n",m_videoLevel-1, m_videoLevel);

            m_resolution = m_resolutionArray[m_videoLevel];
            uint8_t send_Buffer[MAX_PACKET_SIZE];
            sprintf((char *) send_Buffer, "%hu", m_resolution);
            Ptr<Packet> levelPacket = Create<Packet>(send_Buffer, MAX_PACKET_SIZE);
            m_socket->Send(levelPacket);
          }
        }

      NS_LOG_UNCOND("0\t" << Simulator::Now().GetSeconds() << "\t" << m_rebufferCounter);
      NS_LOG_UNCOND("1\t" << Simulator::Now().GetSeconds() << "\t" << m_videotime);
      NS_LOG_UNCOND("2\t" << Simulator::Now().GetSeconds() << "\t" << m_videoLevel);

      m_bufferEvent = Simulator::Schedule(Seconds(1.0), &VideoStreamClient::ReadFromBuffer, this);
      return (m_currentBufferSize);
    }

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
        m_FramePacketCounter[frameNum][packetNum] = true;

        bool BREaK = false;
        for (int frame_idx = m_lastRecvFrame; frame_idx < TOTAL_VIDEO_FRAME; frame_idx++) {
          for (int p_idx = 0; p_idx < int(m_resolution) / MAX_PACKET_SIZE + 1; p_idx++) {
            if(m_FramePacketCounter[frame_idx][p_idx] == false) {
              BREaK = true;
              break;
            } else {
              if (int(m_resolution) % MAX_PACKET_SIZE == 0){
                if (int(m_resolution) / MAX_PACKET_SIZE == p_idx + 1){
                  m_lastRecvFrame = frame_idx + 1;
                  m_currentBufferSize++;
                  break;
                }
              } else {
                if (int(m_resolution) / MAX_PACKET_SIZE == p_idx){
                  m_lastRecvFrame = frame_idx + 1;
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
        
      }
    }
  }

} // namespace ns3
