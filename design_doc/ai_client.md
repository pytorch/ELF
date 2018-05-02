# AI Client

The AI-Client class is used to send input batch from C++ and run the python side CNN from pytorch.

## AIClient Class
- C++ class in folder ./src_cpp/elf/ai/ai.h
- Has a private member of type GameClient to send batch and wait for execution;
```cpp
template <typename S, typename A>
class AIClientT : public AI_T<S, A> {
 public:
  // bind state targets_ to s in funcs_s
  // bind action targets_ to a in funcs_a
  // comm::ReplyStatus status = client_->sendWait(targets_, &funcs_s);
  bool act(const S& s, A* a) override;

  // batch mode
  virtual bool act_batch(
      const std::vector<const S*>& batch_s,
      const std::vector<A*>& batch_a);

 private:
  elf::GameClient* client_;
  std::vector<std::string> targets_;
};
```

## GameClient Class
- C++ class in folder ./src_cpp/elf/ai/ai.h
- The private member client\_ is of type Client defined in src_cpp/elf/comm/comm.h
```cpp
class GameClient {
 public:
  // constructor
  GameClient(Comm* comm, const Context* ctx);
  void start();
  void End();
  bool DoStopGames();

  template <typename S>
  FuncsWithState BindStateToFunctions(
      const std::vector<std::string>& smem_names,
      S* s);
  template <typename S>
  vector<FuncsWithState> BindStateToFunctions(
      const vector<std::string>& smem_names,
      const vector<S*>& batch_s);

  comm::ReplyStatus sendWait(
      const std::vector<std::string>& targets,
      FuncsWithState* funcs) {
    return client_->sendWait(funcs, targets);
  }
  comm::ReplyStatus sendBatchWait(
      const std::vector<std::string>& targets,
      const std::vector<FuncsWithState*>& funcs) {
    return client_->sendBatchWait(funcs, targets);
  }
 private:
  const Context* context_;
  unique_ptr<Client> client_;
};
```

## Client and Server class
- client class and server class are defined in src_cpp/elf/comm/comm.h
- It uses tbb for concurrent queue execution
```cpp
class Client {
 protected:
  ReplyStatus sendWait(Id id, const std::vector<Id>& server_ids, Data data);

  // send data from client to server by message
  ReplyStatus sendBatchWait(Id id,
      const vector<Id>& server_ids,
      const vector<Data>& data);
 private:
  ClientNode* client(Id id);
  ServerNode* server(Id id);
  using ClientMap = tbb::concurrent_hash_map<Id, std::unique_ptr<ClientNode>>;
  using ServerMap = tbb::concurrent_hash_map<Id, std::unique_ptr<ServerNode>>;
  ClientMap clients_;
  ServerMap servers_;
};
```

```cpp
class Server {
 protected:
  bool waitBatch(Id id,
        const WaitOptions& opt,
        vector<ClientToServerMsg>* batch);
  bool sendClosuresWaitDone(
        const vector<ClientToServerMsg>& messages,
        const vector<ReplyFunction>& functions);
  bool ReleaseBatch(
        const std::vector<ClientToServerMsg>& messages,
        ReplyStatus task_result);
  private:
   ClientNode* client(Id id);
   ServerNode* server(Id id);
   using ClientMap = tbb::concurrent_hash_map<Id, std::unique_ptr<ClientNode>>;
   using ServerMap = tbb::concurrent_hash_map<Id, std::unique_ptr<ServerNode>>;
   ClientMap clients_;
   ServerMap servers_;  
};
```

