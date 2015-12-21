/*
Userspace Virtual Filesystem

Copyright (C) 2015 Sebastian Herbord. All rights reserved.

This file is part of usvfs.

usvfs is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

usvfs is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with usvfs. If not, see <http://www.gnu.org/licenses/>.
*/
#pragma warning (disable:4714)
#pragma warning (disable:4503)
#pragma warning(push, 3)
#include "shmlogger.h"
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/format.hpp>
#include <limits>
#include <algorithm>
#pragma warning(pop)

#pragma warning(disable: 4996)

using namespace boost::interprocess;

SHMLogger *SHMLogger::s_Instance = nullptr;

SHMLogger::owner_t SHMLogger::owner;
SHMLogger::client_t SHMLogger::client;


SHMLogger::SHMLogger(owner_t, const std::string &queueName)
  : m_QueueName(queueName)
  , m_LogQueue(create_only, queueName.c_str(), MESSAGE_COUNT, MESSAGE_SIZE)
  , m_DroppedMessages(0)
{
  if (s_Instance != nullptr) {
    throw std::runtime_error("duplicate shm logger instantiation");
  } else {
    s_Instance = this;
  }
}


SHMLogger::SHMLogger(client_t, const std::string &queueName)
  : m_QueueName(queueName)
  , m_LogQueue(open_only, queueName.c_str())
  , m_DroppedMessages(0)
{
  if (s_Instance != nullptr) {
    throw std::runtime_error("duplicate shm logger instantiation");
  } else {
    s_Instance = this;
  }
}


SHMLogger::~SHMLogger() {
  s_Instance = nullptr;
  message_queue_interop::remove(m_QueueName.c_str());
}


SHMLogger &SHMLogger::create(const char *instanceName)
{
  if (s_Instance != nullptr) {
    throw std::runtime_error("duplicate shm logger instantiation");
  } else {
    std::string queueName = std::string("__shm_sink_") + instanceName;
    message_queue::remove(queueName.c_str());
    new SHMLogger(owner, queueName);
    atexit([] () { delete s_Instance; });
  }
  return *s_Instance;
}

SHMLogger &SHMLogger::open(const char *instanceName)
{
  if (s_Instance != nullptr) {
    throw std::runtime_error("duplicate shm logger instantiation");
  } else {
    new SHMLogger(client, std::string("__shm_sink_") + instanceName);
  }
  return *s_Instance;
}


void SHMLogger::free()
{
  if (s_Instance != nullptr) {
    SHMLogger *temp = s_Instance;
    s_Instance = nullptr;
    delete temp;
  }
}

bool SHMLogger::tryGet(char *buffer, size_t bufferSize)
{
  message_queue_interop::size_type receivedSize;
  unsigned int prio;
  bool res = m_LogQueue.try_receive(buffer, bufferSize, receivedSize, prio);
  if (res) {
    buffer[std::min(bufferSize - 1, static_cast<size_t>(receivedSize))] = '\0';
  }
  return res;
}


void SHMLogger::get(char *buffer, size_t bufferSize)
{
  message_queue_interop::size_type receivedSize;
  unsigned int prio;
  m_LogQueue.receive(buffer, bufferSize, receivedSize, prio);
  buffer[std::min(bufferSize - 1, static_cast<size_t>(receivedSize))] = '\0';
}

spdlog::sinks::shm_sink::shm_sink(const char *queueName)
  : m_LogQueue(open_only, (std::string("__shm_sink_") + queueName).c_str())
{
}

void spdlog::sinks::shm_sink::flush()
{
}

void spdlog::sinks::shm_sink::log(const details::log_msg &msg)
{
  if (m_DroppedMessages > 0) {
    int droppedMessages = m_DroppedMessages;
    std::string dropMessage = fmt::format("{} messages dropped", droppedMessages);
    if (m_LogQueue.try_send(dropMessage.c_str(), dropMessage.size(), 0)) {
      m_DroppedMessages.fetch_sub(droppedMessages);
    }
  }

  bool sent = true;

  std::string message = msg.formatted.str();
  // spdlog auto-append line breaks which we don't need. Probably would be better to not write the
  // breaks to begin with?
  size_t count = std::min(message.find_last_not_of("\r\n") + 1,
                          static_cast<size_t>(m_LogQueue.get_max_msg_size()));

  // depending on the log level, drop less important messages if the receiver can't keep up
  switch (msg.level) {
    case level::trace:
    case level::debug:
    case level::notice: {
      //m_LogQueue.send(message.c_str(), count, 0);
      sent = m_LogQueue.try_send(message.c_str(), count, 0);
    } break;
    case level::alert:
    case level::critical:
    case level::emerg:
    case level::err: {
      m_LogQueue.send(message.c_str(), count, 0);
    } break;
    default: {
      boost::posix_time::ptime time = microsec_clock::universal_time() + boost::posix_time::milliseconds(200);
      sent = m_LogQueue.timed_send(message.c_str(), count, 0, time);
    } break;
  }

  if (!sent) {
    ++m_DroppedMessages;
  }
}