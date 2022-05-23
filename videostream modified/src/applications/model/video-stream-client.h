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

#define MAX_PACKET_SIZE 30000      // Server 쪽이랑 맞춰주세용
#define TOTAL_VIDEO_FRAME 100    // Server 쪽이랑 맞춰주세용
#define RESOLUTION 76800          // 초이스 {76800, 230400, 409920, 921600, 2073600}
// 240p      320 *  240 =   76800
// 360p      640 *  360 =  230400
// 480p      854 *  480 =  409920
// 720p     1280 *  720 =  921600
// 1080p    1920 * 1080 = 2073600

namespace ns3 {

class Socket;
class Packet;

/**
 * @brief A Video Stream Client
 */
class VideoStreamClient : public Application
{
public:
/**
 * @brief Get the type ID.
 * 
 * @return the object TypeId
 */
  static TypeId GetTypeId (void);
  VideoStreamClient ();
  virtual ~VideoStreamClient ();

  /**
   * @brief Set the server address and port.
   * 
   * @param ip server IP address
   * @param port server port
   */
  void SetRemote (Address ip, uint16_t port);
  /**
   * @brief Set the server address.
   * 
   * @param addr server address
   */
  void SetRemote (Address addr);

protected:
  virtual void DoDispose (void);

private: 
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  /**
   * @brief Send the packet to the remote server.
   */
  void Send (void);

  /**
   * @brief Read data from the frame buffer. If the buffer does not have 
   * enough frames, it will reschedule the reading event next second.
   * 
   * @return the updated buffer size (-1 if the buffer size is smaller than the fps)
   */
  uint32_t ReadFromBuffer (void);

  /**
   * @brief Handle a packet reception.
   * 
   * This function is called by lower layers.
   * 
   * @param socket the socket the packet was received to
   */
  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket> m_socket;           //!< Socket
  Address m_peerAddress;          //!< Remote peer address
  uint16_t m_peerPort;            //!< Remote peer port

  uint16_t m_initialDelay;        //!< Seconds to wait before displaying the content
  uint16_t m_stopCounter;         //!< Counter to decide if the video streaming finishes
  uint16_t m_rebufferCounter;     //!< Counter of the rebuffering event

  uint32_t m_resolution;          //!< The quality of the video from the server

  uint32_t m_frameRate;           //!< Number of frames per second to be played
  uint32_t m_videoSpeed;          // 배속을 여기에 넣으세용

  uint32_t m_frameSize;           //!< Total size of packets from one frame
  uint32_t m_lastRecvFrame;       //!< Last received frame number
  uint32_t m_lastBufferSize;      //!< Last size of the buffer
  uint32_t m_currentBufferSize;   //!< Size of the frame buffer

  uint32_t m_videotime;

  EventId m_bufferEvent;          //!< Event to read from the buffer
  EventId m_sendEvent;            //!< Event to send data to the server

  bool m_FramePacketCounter[TOTAL_VIDEO_FRAME][RESOLUTION/MAX_PACKET_SIZE + 1];
};

} // namespace ns3

#endif /* VIDEO_STREAM_CLIENT_H */
