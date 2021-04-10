/* tcpCompare.cc
* Written by Nicholas Himes and John Bumgardner
* ECE 773 Spring 2021
* 
*/

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("tcpCompare");

// Global Variables
Time mostRecentSimTime;
long int simulationMaxTime = 9999999999999;
Ptr<PacketSink> sink; //Used for average throughput calculations
int packetDrops;
int congestionWindowChanges;
std::string tcpVariant = "TcpCubic";             /* TCP variant type. */
std::string myDataRate = "50Mbps";
double myErrorRate = 0.0001; // Reference #23: Strong solar flares
std::string myDelay = "1.28s";

// MyApp Definition
class MyApp : public Application 
{
public:

  MyApp ();
  virtual ~MyApp();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0), 
    m_peer (), 
    m_packetSize (0), 
    m_nPackets (0), 
    m_dataRate (0), 
    m_sendEvent (), 
    m_running (false), 
    m_packetsSent (0)
{
}

MyApp::~MyApp()
{
  m_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void 
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void 
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void 
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

static void
CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
  //NS_LOG_UNCOND ("Congestion window at " << Simulator::Now ().GetSeconds () << " is now: " << newCwnd);
  congestionWindowChanges += 1;
}

static void
RxDrop (Ptr<const Packet> p)
{
  //NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
  packetDrops++;
}

static void
RxEnd (Ptr<const Packet> p)
{
  //NS_LOG_UNCOND ("RxEnd at " << Simulator::Now ().GetSeconds ());
  mostRecentSimTime = Simulator::Now ();
}

int 
main (int argc, char *argv[])
{
  //LogComponentEnable("TcpL4Protocol", LOG_LEVEL_LOGIC);

  CommandLine cmd (__FILE__);
  //cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  //cmd.AddValue ("dataRate", "Application data rate", dataRate);
  cmd.AddValue ("tcpVariant", "Transport protocol to use: TcpNewReno, "
                "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat ", tcpVariant);
  //cmd.AddValue ("phyRate", "Physical layer bitrate", phyRate);
  //cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  //cmd.AddValue ("pcap", "Enable/disable PCAP Tracing", pcapTracing);
  cmd.Parse (argc, argv);

  // Select TCP variant
  tcpVariant = std::string ("ns3::") + tcpVariant;
  if (tcpVariant.compare ("ns3::TcpWestwoodPlus") == 0)
    {
      // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
      // the default protocol type in ns3::TcpWestwood is WESTWOOD
      Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    }
  else
    {
      TypeId tcpTid;
      NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (tcpVariant, &tcpTid), "TypeId " << tcpVariant << " not found");
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName (tcpVariant)));
    }
  
  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("50000Mbps")); //Data rate asymmetry in space is on order of 1000:1. Reference #21
  pointToPoint.SetChannelAttribute ("Delay", StringValue (myDelay));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (myErrorRate));
  em->SetAttribute ("ErrorUnit", StringValue ("ERROR_UNIT_BIT"));
  devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.252");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  sink = StaticCast<PacketSink> (sinkApps.Get (0));
  sinkApps.Start (Seconds (0.));
  sinkApps.Stop (Seconds (simulationMaxTime)); //Global variable defined at top

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
  ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange)); //Runs CwndChange() whenever the Congestion Window changes in the TCP socket

  Ptr<MyApp> app = CreateObject<MyApp> ();
  app->Setup (ns3TcpSocket, sinkAddress, 1500, 1000, DataRate (myDataRate)); //Setup to send 1000 packets of size 1500 bytes with a rate of 50Mbps. Total size: 1.5MB
  // 1500000 bytes == 12,000,000 bits. This / 50,000,000 bits per second = 0.24 seconds with low delay. This is what the simulation's total time is, so it works!
  nodes.Get (0)->AddApplication (app);
  app->SetStartTime (Seconds (1.)); //Must remain at 1 second so there is time to start up application and sockets
  app->SetStopTime (Seconds (simulationMaxTime)); //Global variable defined at top

  devices.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop)); //Runs RxDrop() whenever PhyRxDrop happens on the devices
  devices.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxEnd)); //Runs RxEnd() whenever a packet has been completely received from the channel medium by the device

  Simulator::Stop (Seconds (simulationMaxTime)); //Global variable defined at top
  Time startTime = Simulator::Now() + Seconds(1); //Add 1 because app->SetStartTime begins at 1 second
  Simulator::Run ();
  Simulator::Destroy ();

  Time totalSimTime = mostRecentSimTime - startTime;
  std::cout << "\nTCP Variant: " << tcpVariant << ", Data Rate: " << myDataRate << ", Error Rate: " << myErrorRate << ", Delay: " << myDelay << "\n";
  std::cout << "Total simulation time (seconds): " << totalSimTime.GetSeconds () << "\n";
  double averageThroughput = ((sink->GetTotalRx () * 8) / (1 * totalSimTime.GetSeconds ())); //Change to 1e6 for Megabits/s
  std::cout << "Average throughput: " << averageThroughput << " bit/s" << std::endl;
  std::cout << "Packets dropped: " << packetDrops << std::endl;
  std::cout << "Times congestion window changed: " << congestionWindowChanges << std::endl;
  return 0;
}

