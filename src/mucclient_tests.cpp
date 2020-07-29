/*
    Democrit - atomic trades for XAYA games
    Copyright (C) 2020  Autonomous Worlds Ltd

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "private/mucclient.hpp"

#include "testutils.hpp"

#include <glog/logging.h>
#include <gtest/gtest.h>

namespace democrit
{

/* ************************************************************************** */

class MucClientTests : public testing::Test
{

protected:

  /**
   * Gives direct access to the MUCRoom instance insice a MucClient (which
   * is normally private).
   */
  static gloox::MUCRoom&
  AccessRoom (MucClient& c)
  {
    CHECK (c.room != nullptr);
    return *c.room;
  }

  /**
   * Expects that the given nickname has no known full JID for the client.
   */
  static void
  ExpectUnknownNick (const MucClient& c, const std::string& nick)
  {
    gloox::JID jid;
    ASSERT_FALSE (c.ResolveNickname (nick, jid));
  }

  /**
   * Expects that the given nickname has a known full JID and that it matches
   * the given expected one.
   */
  static void
  ExpectNickJid (const MucClient& c, const std::string& nick,
                 const gloox::JID& expected)
  {
    gloox::JID jid;
    ASSERT_TRUE (c.ResolveNickname (nick, jid));
    ASSERT_EQ (jid.full (), expected.full ());
  }

};

namespace
{

/* ************************************************************************** */

using MucConnectionTests = MucClientTests;

TEST_F (MucConnectionTests, Works)
{
  MucClient client(GetTestJid (0), GetPassword (0), GetRoom ("foo"));
  EXPECT_TRUE (client.Connect ());
}

TEST_F (MucConnectionTests, Reconnecting)
{
  MucClient client(GetTestJid (0), GetPassword (0), GetRoom ("foo"));

  ASSERT_TRUE (client.Connect ());
  EXPECT_TRUE (client.IsConnected ());

  client.Disconnect ();
  EXPECT_FALSE (client.IsConnected ());

  ASSERT_TRUE (client.Connect ());
  EXPECT_TRUE (client.IsConnected ());
}

TEST_F (MucConnectionTests, InvalidConnection)
{
  MucClient client(GetTestJid (0), "wrong password", GetRoom ("foo"));
  EXPECT_FALSE (client.Connect ());
}

TEST_F (MucConnectionTests, InvalidRoom)
{
  MucClient client(GetTestJid (0), GetPassword (0), GetRoom ("invalid room"));
  EXPECT_FALSE (client.Connect ());
}

TEST_F (MucConnectionTests, MultipleParticipants)
{
  const gloox::JID room = GetRoom ("foo");

  MucClient client1(GetTestJid (0), GetPassword (0), room);
  ASSERT_TRUE (client1.Connect ());

  MucClient client2(GetTestJid (1), GetPassword (1), room);
  ASSERT_TRUE (client2.Connect ());

  MucClient client3(GetTestJid (0), GetPassword (0), room);
  ASSERT_TRUE (client3.Connect ());
}

TEST_F (MucConnectionTests, KickedFromRoom)
{
  const gloox::JID room = GetRoom ("foo");

  MucClient first(GetTestJid (0), GetPassword (0), room);
  ASSERT_TRUE (first.Connect ());

  MucClient second(GetTestJid (1), GetPassword (1), room);
  ASSERT_TRUE (second.Connect ());

  SleepSome ();
  ASSERT_TRUE (first.IsConnected ());
  ASSERT_TRUE (second.IsConnected ());

  AccessRoom (first).kick (AccessRoom (second).nick ());
  SleepSome ();
  ASSERT_TRUE (first.IsConnected ());
  ASSERT_FALSE (second.IsConnected ());
}

/* ************************************************************************** */

using MucClientNickMapTests = MucClientTests;

TEST_F (MucClientNickMapTests, Works)
{
  const gloox::JID room = GetRoom ("foo");

  const auto firstJid = GetTestJid (0, "first");
  MucClient first(firstJid, GetPassword (0), room);
  ASSERT_TRUE (first.Connect ());

  const auto secondJid = GetTestJid (1, "second");
  MucClient second(secondJid, GetPassword (1), room);
  ASSERT_TRUE (second.Connect ());

  ExpectNickJid (first, AccessRoom (second).nick (), secondJid);
  ExpectNickJid (second, AccessRoom (first).nick (), firstJid);
}

TEST_F (MucClientNickMapTests, UnknownNick)
{
  MucClient client(GetTestJid (0), GetPassword (0), GetRoom ("foo"));
  ASSERT_TRUE (client.Connect ());

  ExpectUnknownNick (client, "invalid");
  ExpectUnknownNick (client, AccessRoom (client).nick ());
}

TEST_F (MucClientNickMapTests, OtherRoom)
{
  const gloox::JID room = GetRoom ("foo");

  MucClient first(GetTestJid (0), GetPassword (0), GetRoom ("foo"));
  ASSERT_TRUE (first.Connect ());

  MucClient second(GetTestJid (1), GetPassword (1), GetRoom ("bar"));
  ASSERT_TRUE (second.Connect ());

  ExpectUnknownNick (first, AccessRoom (second).nick ());
  ExpectUnknownNick (second, AccessRoom (first).nick ());
}

TEST_F (MucClientNickMapTests, SelfDisconnect)
{
  const gloox::JID room = GetRoom ("foo");

  MucClient first(GetTestJid (0), GetPassword (0), room);
  ASSERT_TRUE (first.Connect ());

  MucClient second(GetTestJid (1), GetPassword (1), room);
  ASSERT_TRUE (second.Connect ());
  const std::string secondNick = AccessRoom (second).nick ();

  first.Disconnect ();
  second.Disconnect ();
  ASSERT_TRUE (first.Connect ());

  ExpectUnknownNick (first, secondNick);
}

TEST_F (MucClientNickMapTests, PeerDisconnect)
{
  const gloox::JID room = GetRoom ("foo");

  MucClient first(GetTestJid (0), GetPassword (0), room);
  ASSERT_TRUE (first.Connect ());

  MucClient second(GetTestJid (1), GetPassword (1), room);
  ASSERT_TRUE (second.Connect ());
  const std::string secondNick = AccessRoom (second).nick ();
  second.Disconnect ();

  ExpectUnknownNick (first, secondNick);
}

TEST_F (MucClientNickMapTests, NickChange)
{
  const gloox::JID room = GetRoom ("foo");

  MucClient first(GetTestJid (0), GetPassword (0), room);
  ASSERT_TRUE (first.Connect ());

  const auto secondJid = GetTestJid (1, "second");
  MucClient second(secondJid, GetPassword (1), room);
  ASSERT_TRUE (second.Connect ());
  const std::string secondNick = AccessRoom (second).nick ();

  ExpectNickJid (first, secondNick, secondJid);

  LOG (INFO) << "Changing nick in the room...";
  AccessRoom (second).setNick ("my new nick");
  SleepSome ();

  ExpectUnknownNick (first, secondNick);
  ExpectNickJid (first, "my new nick", secondJid);
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace democrit
