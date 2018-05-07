# Record

- record in folder src_cpp/elfgames/go/record.h is a c++ implementation with **json** for serialization.

## Json Utils
- JSON util functions are defined as macros in src_cpp/elf/utils/json_utils.h
```cpp
#define JSON_LOAD(target, j, field)
#define JSON_LOAD_OPTIONAL(target, j, field)
#define JSON_SAVE(j, field)
#define JSON_SAVE_OBJ(j, field)
#define JSON_LOAD_OBJ_ARGS(target, j, field, ...)
#define JSON_LOAD_VEC(target, j, field)
#define JSON_LOAD_VEC_OPTIONAL(target, j, field)
```

## Client
- Client Type: either invalid, selfplay or eval-then selfplay:
```cpp
struct ClientType {};
```
- Client Control: set json stuff like client type, resign threshold and so on.
```cpp
struct ClientCtrl {
  void setJsonFields(json& j);
  static ClientCtrl createFromJson(
      const json& j,
      bool player_swap_optional = false);
};
```
- Model Pair
```cpp
struct ModelPair {};
```

## Message Related Request
- Message request
```cpp
struct MsgRequest {
  ModelPair vers;
  ClientCtrl client_ctrl;

  void setJsonFields(json& j);
  static MsgRequest createFromJson(const json& j) {}
  std::string setJsonFields(); 
};
```
- Message Result (mcts policy and value for a game)
```cpp
struct CoordRecord {
  unsigned char prob[BOUND_COORD];
};

struct MsgResult {
  std::vector<int64_t> using_models;
  std::vector<CoordRecord> policies;
  std::string content;
  std::vector<float> values;

  // set self value to the json object fields
  void setJsonFields(json& j) const {};
  // get results from a json file (one game) save num_move, value...
  static MsgResult createFromJson(const json& j) {}

};
```
- Others
```cpp
struct MsgVersion {};
enum RestartReply {};
struct MsgRestart {};
```

## Record
- A struct with both MsgRequest and MsgResult
```cpp
struct Record {
  MsgRequest request;
  MsgResult result;

  uint64_t timestamp = 0;
  uint64_t thread_id = 0;
  int seq = 0;
  float pri = 0.0;
  bool offline = false;

  // save self members to the json object j
  void setJsonFields(json& j) const {}
  // load everything from json object j and save to self members
  static Record createFromJson(const json& j) {}
  // return a vector of record by first json::parse(string)
  // iteratively createFromJson()
  static std::vector<Record> createBatchFromJson(const std::string& json_str) {
  // same as above
  static bool loadBatchFromJsonFile(
      const std::string& f,
      std::vector<Record>* records);
  // batch setJsonFields
  static std::string dumpBatchJsonString();
};
```
- A wrapper on record on a vector of Record
```cpp
struct Records {
  std::string identity;
  std::unordered_map<int, ThreadState> states;
  std::vector<Record> records;
};
```