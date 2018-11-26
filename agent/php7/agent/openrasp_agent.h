/*
 * Copyright 2017-2018 Baidu Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _OPENRASP_BASE_AGENT_H_
#define _OPENRASP_BASE_AGENT_H_

#include "openrasp_v8.h"
#include "openrasp_log.h"
#include "openrasp_agent_manager.h"
#include "backend_request.h"
#include "backend_response.h"
#include <signal.h>
#include "utils/time.h"

namespace openrasp
{
class LogCollectItem;

class BaseAgent
{
public:
  bool is_alive;
  pid_t agent_pid;

  BaseAgent(std::string name);

  virtual void run() = 0;
  virtual void write_pid_to_shm(pid_t agent_pid) = 0;
  virtual void install_signal_handler(__sighandler_t signal_handler);

protected:
  std::string name;
  std::string default_slash;
};

class HeartBeatAgent : public BaseAgent
{
public:
  static volatile int signal_received;

public:
  HeartBeatAgent();
  virtual void run();
  virtual void write_pid_to_shm(pid_t agent_pid);

private:
  void do_heartbeat();
};

class LogAgent : public BaseAgent
{
public:
  static volatile int signal_received;
  static const int max_post_logs_account = 512;

public:
  LogAgent();
  virtual void run();
  virtual void write_pid_to_shm(pid_t agent_pid);

private:
  static const long log_push_interval = 15;
  static const long max_interval = 500;
  static const double factor;

private:
  bool post_logs_via_curl(std::string &log_arr, std::string &url_string);
};
} // namespace openrasp

#endif
