#ifndef VIDEO_STREAM_SERVER_H
#define VIDEO_STREAM_SERVER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/string.h"
#include "ns3/ipv4-address.h"
#include "ns3/traced-callback.h"

#include <fstream>
#include <unordered_map>

#define MAX_PACKET_SIZE 30000 
#define TOTAL_VIDEO_FRAME 300 

namespace ns3 {

class Socket;
class Packet;

  class VideoStreamServer : public Application
  {
  public:

    static TypeId GetTypeId (void);

    VideoStreamServer ();

    virtual ~VideoStreamServer ();

    void SetFrameFile (std::string frameFile);

    std::string GetFrameFile (void) const;

    void SetMaxPacketSize (uint32_t maxPacketSize);

    uint32_t GetMaxPacketSize (void) const;

  protected:
    virtual void DoDispose (void);

  private:

    virtual void StartApplication (void);
    virtual void StopApplication (void);

    typedef struct ClientInfo
    {
      Address m_address; 
      uint32_t m_sent;
      uint32_t m_videoLevel;
      uint32_t m_frameRate;
      EventId m_sendEvent;
    } ClientInfo;

    void SendPacket (ClientInfo *client, uint frame_idx, uint packet_idx);
  
    void Send (uint32_t ipAddress);

    void HandleRead (Ptr<Socket> socket);

    Time m_interval; 
    Ptr<Socket> m_socket;

    uint16_t m_port;
    Address m_local; 

    std::string m_frameFile; 
    
    std::unordered_map<uint32_t, ClientInfo*> m_clients; 
  };

} // namespace ns3


#endif /* VIDEO_STREAM_SERVER_H */
