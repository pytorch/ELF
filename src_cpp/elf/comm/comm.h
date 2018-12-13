/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cassert>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <tbb/concurrent_hash_map.h>

#include "elf/concurrency/TBBHashers.h"

#include "base.h"
#include "broadcast.h"

namespace comm {

///
///  Communcation class that handles the communications between Servers and
///  Clients.
///
///  Workflow (Client side):
///     1. Calls `sendWait, to sends a request to a group of Servers and gets
///        blocked. The request is an object typed Data.
///     2. Each Server responds by sending a closure of type
///        `function<ReplyStatus ()>` to the Client.
///     3. When all Servers that the Client is waiting for respond, the Client
///         gets unblocked and return from `sendWait`.
///
///  Workflow (Server side):
///     1. The Server waits on a batchsize of Clients by calling `waitBatch`.
///         The batchsize is set in `WaitOptions`.
///     2. `waitBatch` returns once a batchsize of Clients is collected,
///     3. The Server now processes the messages it has received. The Server can
///        also send data back to client to process. There are two reasons for
///        this
///        a) there are usually more clients than servers, so work can be
///        divided
///        b) server has additional state that client does not have when server
///           first receives the message and server sends back additional state
///           for client to process
///     4. The server releases the client by calling `ReleaseBatch`.
///        Note that the Client might not be unblocked until all its servers
///        have released it.
///
///  Note: All functions are thread-safe and using threads is encouraged for
///        parallelism
///
///  // TODO - check this (ssengupta@fb)
///  For 3', CommInternalT achieves that by having Client invoking another
///  sessions with all the Servers. The Client thus waits for Servers' commands
///  until no Servers have the command.
///
///  Template arguments:
///     Id: identity of server/client.
///     Data: message type sent from client to server, typically a lambda
///     function
///     kExpectRepl:. bool whether client expects a reply from server for
///                   additional work
///     ClientQueue: client side queue
///     ServerQueue: server side queue
///
///   Note on queues:
///   1. They have to concurrent, i.e allow multiple producers and consumers and
///      be thread safe
///   2. We use two different queues becuase our loads are usually different.
///
///      The sender (client) sends a lot of messages to the receiver (sever),
///      since there usually are many senders (say 4096 games). Thus the
///      receivers's queue is usually a queue that uses busy-wait which is
///      `tbb::concurrent_queue`
///
///      The receiver (server) sends relatively fewer messages to the sender.A
///      Usually these messages are control messages
///      Thus the sender's queue is usually a queue that uses signal-wait which
///      is `moodycamel::BlockingConcurrentQueue`

template <
    typename Id,
    typename Data,
    bool kExpectReply,
    template <typename> class ClientQueue,
    template <typename> class ServerQueue>
class CommInternalT {
 public:
  using ReplyFunction = std::function<ReplyStatus()>;
  using ClientNode = NodeT<Data, int, ReplyFunction, ClientQueue, ServerQueue>;
  using ServerNode = NodeT<ReplyFunction, int, Data, ServerQueue, ClientQueue>;
  using ClientToServerMsg = MsgT<Data, int, ReplyFunction, ClientQueue, ServerQueue>;
  using ServerToClientMsg = MsgT<ReplyFunction, int, Data, ServerQueue, ClientQueue>;
  using CommInternal =
      CommInternalT<Id, Data, kExpectReply, ClientQueue, ServerQueue>;

 protected:
  class Client {
   public:
    explicit Client(CommInternal* p) : p_(p) {}

   protected:
    struct _DataPair {
      int source_idx = -1;
      std::vector<Id> server_ids;
      std::vector<Data> data;
      SuccessCallback success_cb = nullptr;

      void send(CommInternal* p, ClientNode *node, std::vector<ClientToServerMsg> &messages) const {
        assert(!data.empty());
        for (Id server_id : server_ids) {
          ServerNode* server = p->server(server_id);
          // LOG(INFO) <<  "Send to server " << hex
          //           << server << dec << std::endl;
          messages.push_back(ClientToServerMsg(node, server, data, source_idx));
        }
      }

      void onSuccess() const {
        if (success_cb != nullptr) success_cb();
      }
    };

    // If Comm does not see this thread id before, it will create a new record
    // for it.
    // data and reply can point to an identical object, since the previous reply
    // can be resent
    // (e.g., the action returned from the reply will be sent for training).
    ReplyStatus sendWait(Id id, const std::vector<Id>& server_ids, Data data) {
      _DataPair d;
      d.source_idx = 0;
      d.server_ids = server_ids;
      d.data.push_back(data);

      return sendBatchesWait(id, std::vector<_DataPair>{d});
    }

    ReplyStatus sendBatchWait(Id id, const std::vector<Id>& server_ids, const std::vector<Data> &data) {
      _DataPair d;
      d.source_idx = 0;
      d.server_ids = server_ids;
      d.data = data;

      return sendBatchesWait(id, std::vector<_DataPair>{d});
    }

    ReplyStatus sendBatchesWait(
        Id id,
        const std::vector<_DataPair>& chunk) {
      // Find server that could accept this task.
      (void)id;
      assert(!chunk.empty());
      std::vector<ClientToServerMsg> messages;
      ClientNode node;

      for (const auto &c : chunk) {
        c.send(p_, &node, messages);
      }

      node.startSession(messages);
      int n = (int)messages.size();
      // std::cout << "sendWait, n = " << n << ", expectReply: " << kExpectReply << std::endl;

      // Wait batchsize 1, indefinitely.
      ReplyStatus final_status = SUCCESS;
      if (kExpectReply) {
        WaitOptions opt(1);
        std::vector<ServerToClientMsg> server_to_client_msgs;

        while (n > 0 && node.waitSessionInvite(opt, &server_to_client_msgs)) {
          assert(server_to_client_msgs.size() == 1);
          assert(server_to_client_msgs[0].data.size() == 1);
          ReplyStatus res = server_to_client_msgs[0].data[0]();
          int source_idx = server_to_client_msgs[0].info;
          assert(source_idx >= 0 && source_idx < (int)chunk.size());

          switch (res) {
            case DONE_ONE_JOB:
              break;
            case UNKNOWN:
            case FAILED:
              n--;
              final_status = res;
              break;
            case SUCCESS:
              n--;
              chunk[source_idx].onSuccess();
              break;
          }
          server_to_client_msgs[0].from->notifySessionInvite();
        }
      }

      node.waitSessionEnd();
      return final_status;
    }

   private:
    CommInternal* p_;
  };

  class Server {
   protected:
    // We define a C++ interface, and a Python wrapper.
    // In the future, we could use C++ interface for C++-only
    // training/evaluation.

    // We could have multiple servers reading data
    bool waitBatch(
        Id id,
        const WaitOptions& opt,
        std::vector<ClientToServerMsg>* batch) {
      ServerNode* node = p_->server(id);
      return node->waitSessionInvite(opt, batch);
    }

   public:
    explicit Server(CommInternal* p) : p_(p) {}

    bool sendClosuresWaitDone(
        const std::vector<ClientToServerMsg>& messages,
        const std::vector<ReplyFunction>& functions) {
      if (messages.empty()) {
        return true;
      }

      ServerNode* node = messages[0].to;
      // assert(node != nullptr);

      std::vector<ServerToClientMsg> server_to_client_msgs;
      for (size_t i = 0; i < messages.size(); ++i) {
        server_to_client_msgs.push_back(
            ServerToClientMsg(node, messages[i].from, functions[i], messages[i].info));
      }
      node->startSession(server_to_client_msgs);
      node->waitSessionEnd();
      return true;
    }

    // Once we have filled the reply, we thus call ReleaseBatch.
    // ReleaseBatch will resume all blocked threads, if they are blocked.
    bool ReleaseBatch(
        const std::vector<ClientToServerMsg>& messages,
        ReplyStatus task_result) {
      if (kExpectReply) {
        std::vector<ReplyFunction> functions;
        functions.resize(
            messages.size(), [task_result]() { return task_result; });
        sendClosuresWaitDone(messages, functions);
      }

      for (const ClientToServerMsg& message : messages) {
        message.from->notifySessionInvite();
      }
      return true;
    }

   private:
    CommInternal* p_;
  };

 private:
  ServerNode* server(Id id) {
    typename ServerMap::accessor elem;
    bool uninitialized = servers_.insert(elem, id);
    if (uninitialized) {
      storages_.emplace_back(new ServerNode());
      elem->second = storages_.back().get();
    }
    return elem->second;
  }

  using ServerMap = tbb::concurrent_hash_map<Id, ServerNode*>;
  ServerMap servers_;

  std::vector<std::unique_ptr<ServerNode>> storages_;
};

struct SendOptions {
  // Specify labels of this msg.
  // For each label, the message will be sent to a receiver with this label.
  // For example, if a message carrys the labels ["actor", "train"]
  // , and there are four receivers, each with label:
  //    1. "actor"
  //    2. "train"
  //    3. "actor"
  //    4. "train"
  // Then the message will be sent to (1, 2), (1, 4), (2, 3), (3, 4)
  // with equal probability.
  std::vector<std::string> labels;
};

struct RecvOptions {
  // A receiver will only honor messags that matches its label
  std::string label;
  WaitOptions wait_opt;

  RecvOptions(
      const std::string& label,
      int batchsize,
      int timeout_usec = 0,
      int min_batchsize = 0)
      : label(label), wait_opt(batchsize, timeout_usec, min_batchsize) {}

  friend bool operator==(const RecvOptions &op1, const RecvOptions &op2) {
    return op1.label == op2.label && op1.wait_opt == op2.wait_opt;
  }
};

///
/// Adds capability of grouping server by their levels and some simple routing
///
///  1. Each server or client id is the current thread id. If a new thread
///     calls `Client::sendwait`, it's thread id will be registered
///     automatically.
///  2. A server has a label associated with it that is be registered via
///     `RegServer`
///  3. When the Client call `sendWait`. it also needs to specify a set of
///     server labels. If there are multiple servers with the same label,
///     a server is chosen by uniform random sampling.
template <
    typename Data,
    bool kExpectReply,
    template <typename> class ClientQueue,
    template <typename> class ServerQueue>
class CommT : public CommInternalT<
                  std::thread::id,
                  Data,
                  kExpectReply,
                  ClientQueue,
                  ServerQueue> {
 public:
  using Id = std::thread::id;
  using Comm = CommT<Data, kExpectReply, ClientQueue, ServerQueue>;
  using CommInternal = CommInternalT<
      std::thread::id,
      Data,
      kExpectReply,
      ClientQueue,
      ServerQueue>;
  using Message = typename CommInternal::ClientToServerMsg;
  using Function = typename CommInternal::ReplyFunction;

  class Client : public CommInternal::Client {
   public:
    explicit Client(Comm* pp)
        : CommInternal::Client(pp), pp_(pp), rng_(time(NULL)) {}

    ReplyStatus sendWait(Data data, const std::vector<std::string>& labels) {
      return CommInternal::Client::sendWait(
          std::this_thread::get_id(), label2server(labels), data);
    }

    ReplyStatus sendBatchWait(
        const std::vector<Data>& data,
        const std::vector<std::string>& labels) {
      return CommInternal::Client::sendBatchWait(
          std::this_thread::get_id(), label2server(labels), data);
    }

    ReplyStatus sendBatchesWait(
        const std::vector<std::vector<Data>>& data,
        const std::vector<std::string>& labels,
        const std::vector<SuccessCallback>& callbacks) {
      std::vector<typename CommInternal::Client::_DataPair> ds(data.size());
      for (size_t i = 0; i < data.size(); ++i) {
        ds[i].source_idx = (int)i;
        ds[i].data = data[i];
        ds[i].server_ids = label2server(labels);
        ds[i].success_cb = callbacks[i];
      }

      return CommInternal::Client::sendBatchesWait(std::this_thread::get_id(), ds);
    }

   private:
    Comm* pp_;
    std::mt19937 rng_;

    std::vector<Id> label2server(const std::vector<std::string>& labels) {
      assert(!labels.empty());
      std::vector<Id> server_ids;

      for (const auto& label : labels) {
        // [TODO] Will this one work in multithreading case?
        // std::cout << "Request label: " << label << std::endl;
        typename ServerLabelMap::const_accessor elem;
        bool found = pp_->serverLabels_.find(elem, label);
        if (!found) {
          // std::cout << "WARNING! no servers has the label: " << label
          //          << std::endl;
        } else {
          const std::vector<Id>& ids = elem->second;
          // Randomly pick one of the label.
          // Note that there is no lock needed since only
          // read access is requested.
          const int idx = rng_() % ids.size();
          server_ids.push_back(ids.at(idx));
        }
      }

      return server_ids;
    }
  };

  class Server : public CommInternal::Server {
   public:
    explicit Server(Comm* pp) : CommInternal::Server(pp), pp_(pp) {}

    // TODO: Put these logic to a separate place.
    void RegServer(const std::string& label) {
      std::lock_guard<std::mutex> lock(pp_->register_mutex_);
      ServerLabelMap::accessor elem;
      pp_->serverLabels_.insert(elem, label);
      elem->second.push_back(std::this_thread::get_id());
      counter_.increment();
    }

    void waitForRegs(int n) {
      counter_.waitUntilCount(n);
      counter_.reset();
    }

    bool waitBatch(const RecvOptions& options, std::vector<Message>* batch) {
      return CommInternal::Server::waitBatch(
          std::this_thread::get_id(), options.wait_opt, batch);
    }

   private:
    Comm* pp_;
    elf::concurrency::Counter<int> counter_;
  };

  // Create and return a client object
  std::unique_ptr<Client> getClient() {
    return std::unique_ptr<Client>(new Client(this));
  }

  // Create and return a server object
  std::unique_ptr<Server> getServer() {
    return std::unique_ptr<Server>(new Server(this));
  }

 private:
  using ServerLabelMap =
      tbb::concurrent_hash_map<std::string, std::vector<Id>>;

  ServerLabelMap serverLabels_;
  std::mutex register_mutex_;
};

} // namespace comm
