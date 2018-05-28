/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 University of Kansas
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Justin Rohrer <rohrej@ittc.ku.edu>
 *
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  http://wiki.ittc.ku.edu/resilinets
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 *
 * Work supported in part by NSF FIND (Future Internet Design) Program
 * under grant CNS-0626918 (Postmodern Internet Architecture),
 * NSF grant CNS-1050226 (Multilayer Network Resilience Analysis and Experimentation on GENI),
 * US Department of Defense (DoD), and ITTC at The University of Kansas.
 */

/*
 * This example program allows one to run ns-3 DSDV, AODV, or OLSR under
 * a typical random waypoint mobility model.
 *
 * By default, the simulation runs for 200 simulated seconds, of which
 * the first 50 are used for start-up time.  The number of nodes is 50.
 * Nodes move according to RandomWaypointMobilityModel with a speed of
 * 20 m/s and no pause time within a 300x1500 m region.  The WiFi is
 * in ad hoc mode with a 2 Mb/s rate (802.11b) and a Friis loss model.
 * The transmit power is set to 7.5 dBm.
 *
 * It is possible to change the mobility and density of the network by
 * directly modifying the speed and the number of nodes.  It is also
 * possible to change the characteristics of the network by changing
 * the transmit power (as power increases, the impact of mobility
 * decreases and the effective density increases).
 *
 * By default, OLSR is used, but specifying a value of 2 for the protocol
 * will cause AODV to be used, and specifying a value of 3 will cause
 * DSDV to be used.
 *
 * By default, there are 10 source/sink data pairs sending UDP data
 * at an application rate of 2.048 Kb/s each.    This is typically done
 * at a rate of 4 64-byte packets per second.  Application data is
 * started at a random time between 50 and 51 seconds and continues
 * to the end of the simulation.
 *
 * The program outputs a few items:
 * - packet receptions are notified to stdout such as:
 *   <timestamp> <node-id> received one packet from <src-address>
 * - each second, the data reception statistics are tabulated and output
 *   to a comma-separated value (csv) file
 * - some tracing and flow monitor configuration that used to work is
 *   left commented inline in the program
 */

#include <fstream>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"
#include "wifi-example-apps.h"
#include "ns3/gpsr-module.h"
#include "ns3/gpsr-helper.h"
#include "ns3/mygpsr-module.h"
#include "ns3/mygpsr-helper.h"
#include "ns3/seq-ts-header.h"
#include "ns3/wave-net-device.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-helper.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"


using namespace ns3;
using namespace dsr;

NS_LOG_COMPONENT_DEFINE ("manet-routing-compare");

class RoutingExperiment
{
public:
  RoutingExperiment ();
  void Run (int nSinks, double txp, std::string CSVfileName, int protocol, int nodes);
  //static void SetMACParam (ns3::NetDeviceContainer & devices,
  //                                 int slotDistance);
  void CommandSetup (int argc, char **argv);

  std::string GetTimeFile();
  std::string GetLogFile();

private:
  Ptr<Socket> SetupPacketReceive (Ipv4Address addr, Ptr<Node> node);
  void ReceivePacket (Ptr<Socket> socket);
  void CheckThroughput ();
  void Statistics (int nodes);

  uint32_t port;
  uint32_t bytesTotal;
  uint32_t packetsReceived;
  uint32_t packetsTotal;
  Time totalTime;

  std::string m_CSVfileName;
  std::string m_averageTimeFile;
  std::string m_traceName;
  int m_nSinks;
  std::string m_protocolName;
  double m_txp;
  bool m_traceMobility;
  uint32_t m_protocol;
};

RoutingExperiment::RoutingExperiment ()
  : port (9),
    bytesTotal (0),
    packetsReceived (0),
    packetsTotal (100),
    totalTime (Seconds(0)),
    m_CSVfileName ("../experiment-statistics/vanet-routing.output.csv"),
    m_averageTimeFile ("../experiment-statistics/vanet-routing.time.csv"),
    m_traceName ("../experiment-statistics/manet-routing-compare"),
    m_traceMobility (false),
    m_protocol (2) // AODV
{
}

std::string
RoutingExperiment::GetTimeFile()
{
  return m_averageTimeFile;
}

std::string
RoutingExperiment::GetLogFile()
{
  return m_CSVfileName;
}

static inline std::string
PrintReceivedPacket (Ptr<Socket> socket, Ptr<Packet> packet, Address senderAddress)
{
  std::ostringstream oss;

  oss << Simulator::Now ().GetSeconds () << " " << socket->GetNode ()->GetId ();

  if (InetSocketAddress::IsMatchingType (senderAddress))
    {
      InetSocketAddress addr = InetSocketAddress::ConvertFrom (senderAddress);
      oss << " received one packet from " << addr.GetIpv4 ();
    }
  else
    {
      oss << " received one packet!";
    }
  return oss.str ();
}

void
RoutingExperiment::ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address senderAddress;
  while ((packet = socket->RecvFrom (senderAddress)))
    {
      bytesTotal += packet->GetSize ();
      packetsReceived += 1;
      TimestampTag timestamp;
      if (packet->FindFirstMatchingByteTag (timestamp))
      {
          Time tx = timestamp.GetTimestamp ();
          totalTime += (Simulator::Now() - tx);
          // NS_LOG_UNCOND("Now the time is " << Simulator::Now() << " and the \"tx\"time is "<< tx.GetSeconds());
      }
      // NS_LOG_UNCOND ("Now the total time is " << totalTime.GetSeconds() << " seconds!");
      NS_LOG_UNCOND ("Now the total packets received is " << packetsReceived);
      NS_LOG_UNCOND (PrintReceivedPacket (socket, packet, senderAddress));
    }
}

void
RoutingExperiment::CheckThroughput ()
{
  double kbs = (bytesTotal * 8.0) / 1000;
  bytesTotal = 0;

  std::ofstream out (m_CSVfileName.c_str (), std::ios::app);

  out << (Simulator::Now ()).GetSeconds () << ","
      << kbs << ","
      << packetsReceived << ","
      << m_nSinks << ","
      << m_protocolName << ","
      << m_txp << ""
      << std::endl;

  out.close ();
  packetsReceived = 0;
  // every second doing statics once
  Simulator::Schedule (Seconds (1.0), &RoutingExperiment::CheckThroughput, this);
}

void RoutingExperiment::Statistics (int nodes)
{
  double averageTime = totalTime.GetSeconds()/packetsReceived;
  double PDR = double(packetsReceived)/(packetsTotal*10);
  std::ofstream out (m_averageTimeFile.c_str (), std::ios::app);

  out << nodes << ","
      << packetsTotal*10 << ","
      << packetsReceived << ","
      << totalTime.GetSeconds() << ","
      << averageTime << ","
      << PDR << ""
      << std::endl;

  out.close ();
  packetsReceived = 0;
  totalTime = Time(Seconds(0));
}

Ptr<Socket>
RoutingExperiment::SetupPacketReceive (Ipv4Address addr, Ptr<Node> node)
{
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket (node, tid);
  InetSocketAddress local = InetSocketAddress (addr, port);
  sink->Bind (local);
  sink->SetRecvCallback (MakeCallback (&RoutingExperiment::ReceivePacket, this));

  return sink;
}

void
RoutingExperiment::CommandSetup (int argc, char **argv)
{
  CommandLine cmd;
  cmd.AddValue ("CSVfileName", "The name of the CSV output file name", m_CSVfileName);
  cmd.AddValue ("traceMobility", "Enable mobility tracing", m_traceMobility);
  cmd.AddValue ("protocol", "1=OLSR;2=AODV;3=DSDV;4=DSR", m_protocol);
  cmd.AddValue ("AverageTimeFile", "The statistics of routing experiment", m_averageTimeFile);
  cmd.AddValue ("TraceFile", "The mobility trace file", m_traceName);
  // cmd.AddValue ("AverageTimeFile", "The name of the time file", m_averageTimeFile);
  cmd.Parse (argc, argv);
}

int
main (int argc, char *argv[])
{
  RoutingExperiment experiment;
  experiment.CommandSetup (argc, argv);
  std::string CSVfileName = experiment.GetLogFile();

  std::string AverageTimeFile = experiment.GetTimeFile();

  //blank out the last output file and write the column headers
  std::ofstream out (CSVfileName.c_str ());
  out << "SimulationSecond," <<
  "ReceiveRate," <<
  "PacketsReceived," <<
  "NumberOfSinks," <<
  "RoutingProtocol," <<
  "TransmissionPower" <<
  std::endl;
  out.close ();
  NS_LOG_UNCOND("Create the first csv file!");

  std::ofstream out1 (AverageTimeFile.c_str ());
  out1 << "NodeCounts," <<
  "TotalPackets,"
  "TotalReceivedPackets," <<
  "TotalTime," <<
  "AverageTime," <<
  "PDR" <<
  std::endl;
  out1.close ();
  NS_LOG_UNCOND ("Create the second csv file!");

  int nSinks = 10;
  double txp = 20;

  for (int protocol = 5; protocol<=6; protocol++)
  {
      for (int counts = 20; counts <= 100; counts+=5)
    {
      experiment.Run (nSinks, txp, CSVfileName, protocol, counts);
    }

  }

}

void
RoutingExperiment::Run (int nSinks, double txp, std::string CSVfileName, int protocol, int nodes)
{


  Packet::EnablePrinting ();
  m_nSinks = nSinks;
  m_txp = txp;
  m_CSVfileName = CSVfileName;

  m_protocol = protocol;

  int nWifis = nodes;

  NS_LOG_UNCOND("Running on the nodes of " << nWifis);

  double TotalTime = 53.0;
  std::string rate ("2048bps");
  std::string phyMode ("OfdmRate6MbpsBW10MHz");
  // std::string tr_name ("../experiment-statistics/manet-routing-compare");
  int nodeSpeed = 23; //in m/s
  int nodePause = 0; //in s
  m_protocolName = "protocol";
  bool verbose = false;

  Config::SetDefault  ("ns3::OnOffApplication::PacketSize",StringValue ("64"));
  Config::SetDefault ("ns3::OnOffApplication::DataRate",  StringValue (rate));

  //Set Non-unicastMode rate to unicast mode
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",StringValue (phyMode));

  NodeContainer adhocNodes;
  adhocNodes.Create (nWifis);

  // setting up wifi phy and channel using helpers
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart",DoubleValue (txp));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txp));
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
  wifiChannel.AddPropagationLoss ("ns3::NakagamiPropagationLossModel");

  Ptr<YansWifiChannel> channel = wifiChannel.Create ();
  wifiPhy.SetChannel (channel);

  // ns-3 supports generate a pcap trace and Add a wifiNetDevice
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11);
  NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
  if (verbose)
    {
      wifi80211p.EnableLogComponents ();      // Turn on all Wifi 802.11p logging
    }

  wifi80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode",StringValue (phyMode),
                                      "ControlMode",StringValue (phyMode));

  //No setting the Tx power or MAC type                                      
  NetDeviceContainer adhocDevices = wifi80211p.Install (wifiPhy, wifi80211pMac, adhocNodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::GaussMarkovMobilityModel",
    "Bounds", BoxValue (Box (0, 3000, 0, 3500, 0, 0)),
    "TimeStep", TimeValue (Seconds (3)),
    "Alpha", DoubleValue (0.85),
    "MeanVelocity", StringValue ("ns3::UniformRandomVariable[Min=14|Max=23]"),
    "MeanDirection", StringValue ("ns3::UniformRandomVariable[Min=0|Max=6.283185307]"),
    "NormalVelocity", StringValue ("ns3::NormalRandomVariable[Mean=0.0|Variance=0.0|Bound=0.0]"),
    "NormalDirection", StringValue ("ns3::NormalRandomVariable[Mean=0.0|Variance=0.2|Bound=0.4]"));
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
    "X", StringValue ("ns3::UniformRandomVariable[Min=0|Max=3500]"),
    "Y", StringValue ("ns3::UniformRandomVariable[Min=0|Max=3500]"),
    "Z", StringValue ("ns3::UniformRandomVariable[Min=0|Max=0]"));
  mobility.Install (adhocNodes);


  AodvHelper aodv;
  OlsrHelper olsr;
  DsdvHelper dsdv;
  DsrHelper dsr;
  DsrMainHelper dsrMain;
  GpsrHelper gpsr;
  MyGpsrHelper mygpsr(txp);
  Ipv4ListRoutingHelper list;
  InternetStackHelper internet;

  switch (m_protocol)
    {
    case 1:
      list.Add (olsr, 100);
      m_protocolName = "OLSR";
      break;
    case 2:
      list.Add (aodv, 100);
      m_protocolName = "AODV";
      break;
    case 3:
      list.Add (dsdv, 100);
      m_protocolName = "DSDV";
      break;
    case 4:
      m_protocolName = "DSR";
      break;
    case 5:
      m_protocolName = "GPSR";
      break;
    case 6:
      m_protocolName = "MYGPSR";
      break;
    default:
      NS_FATAL_ERROR ("No such protocol:" << m_protocol);
    }

  if (m_protocol < 4)
    {
      internet.SetRoutingHelper (list);
      internet.Install (adhocNodes);
    }
  else if (m_protocol == 4)
    {
      internet.Install (adhocNodes);
      dsrMain.Install (dsr, adhocNodes);
    }
  else if (m_protocol==5)
    {
      {
        internet.SetRoutingHelper (gpsr);
        internet.Install (adhocNodes);
      }
    
      gpsr.Install();
    }
  else if (m_protocol==6)
  {
    {
        internet.SetRoutingHelper (mygpsr);
        internet.Install (adhocNodes);
    }
    
      mygpsr.Install(adhocDevices);
  }

  NS_LOG_INFO ("assigning ip address");

  Ipv4AddressHelper addressAdhoc;
  addressAdhoc.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer adhocInterfaces;
  adhocInterfaces = addressAdhoc.Assign (adhocDevices);

  /*OnOffHelper onoff1 ("ns3::UdpSocketFactory",Address ());
  onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));*/

  for (int i = 0; i < nSinks; i++)
    {
      Ptr<Socket> sink = SetupPacketReceive (adhocInterfaces.GetAddress (i), adhocNodes.Get (i));

      ObjectFactory factory;
      factory.SetTypeId ("Sender");
      factory.Set("Destination", Ipv4AddressValue(adhocInterfaces.GetAddress (i)));
      factory.Set("Port", UintegerValue(9));
      factory.Set("NumPackets", UintegerValue(100));
      Ptr<Sender> sender = factory.Create<Sender>();

      Ptr<Node> appSource = NodeList::GetNode (i+nSinks);
      appSource->AddApplication (sender);
      Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
      sender->SetStartTime (Seconds(var->GetValue (1.0, 2.0)));


      /*AddressValue remoteAddress (InetSocketAddress (adhocInterfaces.GetAddress (i), port));
      // set the destination address
      onoff1.SetAttribute ("Remote", remoteAddress);

      Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
      ApplicationContainer temp = onoff1.Install (adhocNodes.Get (i + nSinks));
      temp.Start (Seconds (var->GetValue (100.0,101.0)));
      temp.Stop (Seconds (TotalTime));*/
    }

  std::stringstream ss;
  ss << nWifis;
  std::string nodes = ss.str ();

  std::stringstream ss2;
  ss2 << nodeSpeed;
  std::string sNodeSpeed = ss2.str ();

  std::stringstream ss3;
  ss3 << nodePause;
  std::string sNodePause = ss3.str ();

  std::stringstream ss4;
  ss4 << rate;
  std::string sRate = ss4.str ();

  //NS_LOG_INFO ("Configure Tracing.");
  //tr_name = tr_name + "_" + m_protocolName +"_" + nodes + "nodes_" + sNodeSpeed + "speed_" + sNodePause + "pause_" + sRate + "rate";

  //AsciiTraceHelper ascii;
  //Ptr<OutputStreamWrapper> osw = ascii.CreateFileStream ( (tr_name + ".tr").c_str());
  //wifiPhy.EnableAsciiAll (osw);
  AsciiTraceHelper ascii;
  MobilityHelper::EnableAsciiAll (ascii.CreateFileStream (m_traceName + ".mob"));

  //Ptr<FlowMonitor> flowmon;
  //FlowMonitorHelper flowmonHelper;
  //flowmon = flowmonHelper.InstallAll ();


  NS_LOG_INFO ("Run Simulation.");

  // CheckThroughput ();

  NS_LOG_UNCOND("The routing is " << m_protocolName);
  

  Simulator::Stop (Seconds (TotalTime));
  Simulator::Run ();

  //flowmon->SerializeToXmlFile ((tr_name + ".flowmon").c_str(), false, false);

  Simulator::Destroy ();

  Statistics (nWifis);
}
