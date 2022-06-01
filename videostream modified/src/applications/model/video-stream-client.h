/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef VIDEO_STREAM_CLIENT_H
#define VIDEO_STREAM_CLIENT_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/address.h"
#include "ns3/traced-callback.h"

#include <fstream>
#include <unordered_map>
#include <cmath>

#define MAX_PACKET_SIZE 30000      
#define TOTAL_VIDEO_FRAME 300   
#define RESOLUTION 300001          // {76800, 230400, 409920, 921600, 2073600}

namespace ns3 {

class Socket;
class Packet;

class VideoStreamClient : public Application
{
public:

  static TypeId GetTypeId (void);
  VideoStreamClient ();
  virtual ~VideoStreamClient ();

  void SetRemote (Address ip, uint16_t port);
 
  void SetRemote (Address addr);

protected:
  virtual void DoDispose (void);

private: 
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void Send (void);

  uint32_t ReadFromBuffer (void);

  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket> m_socket;           
  Address m_peerAddress;          
  uint16_t m_peerPort;           

  uint16_t m_initialDelay; 
  uint16_t m_stopCounter;  
  uint16_t m_rebufferCounter;  
  

  uint32_t m_resolution;  
  uint32_t m_videoLevel;  

  uint32_t m_frameRate;  
  double m_videoSpeed;  
  uint32_t m_speedxframeRate;

  uint32_t m_frameSize;  
  uint32_t m_lastRecvFrame;  
  uint32_t m_lastBufferSize;  
  uint32_t m_currentBufferSize;  

  uint32_t m_videotime;

  EventId m_bufferEvent;  
  EventId m_sendEvent;   

  bool m_FramePacketCounter[TOTAL_VIDEO_FRAME][RESOLUTION/MAX_PACKET_SIZE + 1];
  uint32_t m_resolutionArray[6]; 
};
} // namespace ns3

#endif /* VIDEO_STREAM_CLIENT_H */
