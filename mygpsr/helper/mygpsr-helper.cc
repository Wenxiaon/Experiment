/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 IITP RAS
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
 * Authors: António Fonseca <afonseca@tagus.inesc-id.pt>, written after OlsrHelper by Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "mygpsr-helper.h"
#include "ns3/mygpsr.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/node-container.h"
#include "ns3/callback.h"
#include "ns3/udp-l4-protocol.h"
#include "ns3/net-device-container.h"


namespace ns3 {

MyGpsrHelper::MyGpsrHelper (double power)
  : Ipv4RoutingHelper ()
{
  m_agentFactory.SetTypeId ("ns3::mygpsr::RoutingProtocol");
  m_agentFactory.Set ("TxPower", DoubleValue (power));
}

MyGpsrHelper*
MyGpsrHelper::Copy (void) const
{
  return new MyGpsrHelper (*this);
}

Ptr<Ipv4RoutingProtocol>
MyGpsrHelper::Create (Ptr<Node> node) const
{
  //Ptr<Ipv4L4Protocol> ipv4l4 = node->GetObject<Ipv4L4Protocol> ();
  Ptr<mygpsr::RoutingProtocol> mygpsr = m_agentFactory.Create<mygpsr::RoutingProtocol> ();

  //mygpsr->SetDownTarget (ipv4l4->GetDownTarget ());
  //ipv4l4->SetDownTarget (MakeCallback (&mygpsr::RoutingProtocol::AddHeaders, mygpsr));
  node->AggregateObject (mygpsr);
  return mygpsr;
}

void
MyGpsrHelper::Set (std::string name, const AttributeValue &value)
{
  m_agentFactory.Set (name, value);
}


void
MyGpsrHelper::Install (NetDeviceContainer device) const
{
  
  NodeContainer c = NodeContainer::GetGlobal ();
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = (*i);
      Ptr<UdpL4Protocol> udp = node->GetObject<UdpL4Protocol> ();
      Ptr<mygpsr::RoutingProtocol> mygpsr = node->GetObject<mygpsr::RoutingProtocol> ();
      mygpsr->SetDownTarget (udp->GetDownTarget ());
      udp->SetDownTarget (MakeCallback(&mygpsr::RoutingProtocol::AddHeaders, mygpsr));

    }

  for (NetDeviceContainer::Iterator i = device.Begin(); i!=device.End(); ++i)
    {
      Ptr<WifiNetDevice> device = (*i)->GetObject<WifiNetDevice>();
      Ptr<YansWifiPhy> phy = device->GetPhy()->GetObject<YansWifiPhy>();
      Ptr<Node> node = device->GetNode();
      Callback<void, Ptr<Packet>, double> callback = MakeCallback(&mygpsr::RoutingProtocol::UpdatePowerAndRange, node->GetObject<mygpsr::RoutingProtocol>());

      phy->SetCrossLayer (callback);
    }


}

}
