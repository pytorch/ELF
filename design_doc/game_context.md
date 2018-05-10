# Game Context Interface

## Game Context
- class **GameContext**, with a python binding class in elfgames.go:
- **constructor**: 
 ```cpp
  set new context;
  if mode is "selfplay" or "online", set ``eval control`` and ``writer``; else ("train mode"), initialize ``reader``, reset the ``Data Online Loader`` or ``Offline Loader``. 
  Push into the vector _games of size num_game Train or Selfplay;
  set start call back as games[i]->mainLoop()
  set callback after game start as load_offline_selfplay_data()
 ```
- Other public members:
```cpp
 getParams() // with python binding;
 getGame() // with python binding;
 getGameStats()
 notifyNewVersion(); setInitialVersion(); setEvalMode(); // all used in training side;
 setRequest() // for the client side; 
 Context* ctx() // get _context, with python binding
```
- private members:
```cpp
 unique_ptr<Context> _context;
 vector<unique_ptr<GoGameBase>> _games;
 _contex_options;
 unique_ptr<TrainCtrl> _train_ctrl;
 unique_ptr<EvalCtrl> _eval_ctrl;
 unique_ptr<Writer> _writer
 unique_ptr<ReaderQueuesT<Record>> _reader
 GoFeature _go_feature;
 get_net_options() // get connection options like address, server, ipv6, port
 load_offline_selfplay_data() // load batches from JSON files in a multi-threading way into _reader;
 init_reader()
```

## Context
- class GameStateCollector
```cpp
 smem()
 start() 
 prepareToStop()
 stop()
 Server* server_
 BatchClient* batchClient_
 unique_ptr<SharedMem> smem_
 unique_ptr<thread> th_
 Switch completedSwitch_
 ConcurrentQueue<_Msg> msgQueue_

 // important function
 collectAndSendBatch()
```
- class **Context**
- public members of **Context**
```cpp
 friend class *GameStateCollector*
 constructor
 Extractor& getExtractor()
 GameClient* getClient()
 setStartCallback(int num_games, GameCallback cb)
 void setCBAfterGameStart(function<void()> cb)
 SharedMem& ``allocateSharedMem``()
 
 start()
```
- private members of **Context**
```cpp
 Extractor extractor_
 vector<unique_ptr<GameStateCollector>> collectors_
 Comm comm_
 unique_ptr<Server> server_
 BatchComm batch_comm
 unique_ptr<BatchServer> batch_server_
 BatchComm batch_comm_;
 unique_ptr<BatchServer> batch_server_;
 unique_ptr<BatchClient> batchClient_;
 vector<BatchMessage> smem_batch_;
 unordered_map<string, vector<string>> smem2keys_
 int num_games_ = 0;
 GameCallback game_cb_ = nullptr
 function<void()> cb_after_game_start_ = nullptr
 vector<thread> game_threads_
```
- class ``GameClient``

## Game Context Python End
- GameContext exposed to python via pybind;
- python class GCWrapper on class GameContext
```python
class GCWrapper:
  def __init__(self, gpu, GC):
    self.batches
    self.gpu = gpu
    self.params = params
    self.GC = GC
    self._cb = {}

  # register the pytorch forward() function here
  def reg_callback(self, key, cb):

  def _makebatch(self, key_array):

  # wait for the batch (smem) ready
  # run pytorch callback forward()
  # put the result in reply
  def _call(self, smem, *args, **kwargs):

  def run(self):
    smem = self.GC.ctx().wait()
    self._call(smem, *args, **kwargs)
    self.GC.ctx().step()

  def start(self):

  def stop(self):

  # import signal and send signal to handler
  # not used in Go?
  def reg_sig_int(self):
```
