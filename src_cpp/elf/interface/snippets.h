#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <vector>

#include "decorator.h"
#include "game_interface.h"

namespace elf {

namespace snippet {

using SpecItem = std::unordered_map<std::string, std::vector<std::string>>;
using Spec = std::unordered_map<std::string, SpecItem>;

struct Reply {
  int game_idx;
  int64_t a;
  float V;
  float r;
  int terminal;
  int tick = -1;
  int cnt = -1;
  std::vector<float> pi;

  Reply(int num_action = 0) : pi(num_action) {
  }

  void clear() {
    r = 0;
    V = 0;
    a = -1;
    terminal = 0;
  }

  void reset() {
    tick = -1;
    cnt = -1;
  }

  void setPi(const float *ppi) { assert(pi.size() > 0); std::copy(ppi, ppi + pi.size(), pi.begin()); }
  void setValue(const float *pV) { V = *pV; }
  void setAction(const int64_t *aa) { a = *aa; }

  size_t getValue(float *pV) const { *pV = V; return 1; }
  size_t getAction(int64_t *aa) const { *aa = a; return 1; }
  size_t getPi(float *ppi) const { assert(pi.size() > 0); std::copy(pi.begin(), pi.end(), ppi); return pi.size(); }
  size_t getTick(int *t) const { *t = tick; return 1; }
  size_t getCnt(int *t) const { *t = cnt; return 1; }
  size_t getReward(float *rr) const { *rr = r; return 1; }
  size_t getTerminal(int *tterminal) const { *tterminal = r; return 1; }
};

using Replay = elf::decorator::ShortReplayBuffer<Reply>;

// Game has the following interface:
class GameInterface {
 public:
  virtual std::vector<float> feature() const = 0;
  virtual int dim() const = 0;
  virtual std::vector<int> dims() const = 0;
  virtual int numActions() const = 0;
  virtual std::unordered_map<std::string, int> getParams() const = 0;
  // return false if the game has come to an end, and change the reply.
  virtual bool step(Reply *) = 0;
  virtual void reset() = 0;
};

struct GameFactory {
  using Func = std::function<GameInterface *(int game_idx)>;
  Func f = nullptr;

  GameFactory() { }
  GameFactory(Func f) : f(f) { }
};

struct ActorSender {
  const decorator::FrameStacking &stacking;
  Reply &reply;

  ActorSender(const decorator::FrameStacking &stacking, Reply &reply) : stacking(stacking), reply(reply) { }

  void getFeature(float *f) const { stacking.getFeature(f); }
  void getCnt(int *cnt) const { reply.getCnt(cnt); }
  void getTick(int *t) const { reply.getTick(t); }

  void setValue(const float *V) { reply.setValue(V); }
  void setPi(const float *pi) { reply.setPi(pi); }
  void setAction(const int64_t *a) { reply.setAction(a); }

  static Extractor reg(const GameInterface &game, int batchsize, int frame_stack) {
     Extractor e;
     std::vector<int> dims = game.dims();

     dims.insert(dims.begin(), batchsize);
     dims[1] *= frame_stack;

     e.addField<float>("s")
       .addExtents(batchsize, dims)
       .addFunction<ActorSender>(&ActorSender::getFeature);

     e.addField<int>("game_cnt")
       .addExtents(batchsize, {batchsize})
       .addFunction<ActorSender>(&ActorSender::getCnt);

     e.addField<int>("game_step")
       .addExtents(batchsize, {batchsize})
       .addFunction<ActorSender>(&ActorSender::getTick);

     e.addField<float>("V")
       .addExtents(batchsize, {batchsize})
       .addFunction<ActorSender>(&ActorSender::setValue);

     e.addField<float>("pi")
       .addExtents(batchsize, {batchsize, game.numActions()})
       .addFunction<ActorSender>(&ActorSender::setPi);

     e.addField<int64_t>("a")
       .addExtents(batchsize, {batchsize})
       .addFunction<ActorSender>(&ActorSender::setAction);

     return e;
  }
};

struct TrainSender {
  const Replay &replay;

  TrainSender(const Replay &replay) : replay(replay) { }

  void getFeature(float *s) const { replay.getFeatureForward(s); }
  void getPi(float *pi) const { replay.histReply().extractForward(pi, &Reply::getPi); }
  void getValue(float *V) const { replay.histReply().extractForward(V, &Reply::getValue); }
  void getAction(int64_t *a) const { replay.histReply().extractForward(a, &Reply::getAction); }
  void getTick(int *tick) const { replay.histReply().extractForward(tick, &Reply::getTick); }
  void getReward(float *reward) const { replay.histReply().extractForward(reward, &Reply::getReward); }
  void getTerminal(int *terminal) const { replay.histReply().extractForward(terminal, &Reply::getTerminal); }

  static Extractor reg(const GameInterface &game, int batchsize, int T, int frame_stack) {
     Extractor e;

     std::vector<int> dims = game.dims();
     dims.insert(dims.begin(), T);
     dims.insert(dims.begin(), batchsize);
     dims[2] *= frame_stack;

     e.addField<float>("s_")
       .addExtents(batchsize, dims)
       .addFunction<TrainSender>(&TrainSender::getFeature);

     e.addField<float>("pi_")
       .addExtents(batchsize, {batchsize, T, game.numActions()})
       .addFunction<TrainSender>(&TrainSender::getPi);

     e.addField<float>("V_")
       .addExtents(batchsize, {batchsize, T})
       .addFunction<TrainSender>(&TrainSender::getValue);

     e.addField<int64_t>("a_")
       .addExtents(batchsize, {batchsize, T})
       .addFunction<TrainSender>(&TrainSender::getAction);

     e.addField<float>("r_")
       .addExtents(batchsize, {batchsize, T})
       .addFunction<TrainSender>(&TrainSender::getReward);

     e.addField<int>("terminal_")
       .addExtents(batchsize, {batchsize, T})
       .addFunction<TrainSender>(&TrainSender::getTerminal);

     e.addField<int>("t_")
       .addExtents(batchsize, {batchsize, T})
       .addFunction<TrainSender>(&TrainSender::getTick);

     return e;
  }
};

DEF_STRUCT(Options)
  DEF_FIELD(int, T, 6, "len of history")
  DEF_FIELD(int, frame_stack, 4, "Framestack")
  DEF_FIELD(float, reward_clip, 1.0f, "Reward clip")
  // DEF_FIELD_NODEFAULT(elf::Options, base, "ELF options")
DEF_END

class Summary {
public:
    Summary()
      : _accu_reward(0), _accu_reward_all_game(0),
        _accu_reward_last_game(0), _n_complete_game(0), _n_merged(0) { }

    void feed(float curr_reward) {
      _accu_reward = _accu_reward + curr_reward;
    }

    void feed(const Summary &other) {
      _accu_reward_all_game = _accu_reward_all_game + other._accu_reward_all_game;
      _n_complete_game = _n_complete_game + other._n_complete_game;

      _accu_reward_last_game = _accu_reward_last_game + other._accu_reward_last_game;
      _n_merged ++;
    }

    void reset() {
      _accu_reward_all_game = _accu_reward_all_game + _accu_reward;
      _accu_reward_last_game = _accu_reward + 0;
      _accu_reward = 0;
      _n_complete_game ++;
    }

    std::string print() const {
      std::stringstream ss;
      if (_n_complete_game > 0) {
        ss << "Accumulated: " << (float)_accu_reward_all_game / _n_complete_game << "[" << _n_complete_game << "]";
      } else {
        ss << "0[0]";
      }
      if (_n_merged > 0) {
        ss << ", Avg last episode:" << (float)_accu_reward_last_game / _n_merged << "[" << _n_merged << "]";
      } else {
        ss << "0[0]";
      }

      ss << " current accumulated reward: " << _accu_reward;
      return ss.str();
    }

private:
    std::atomic<float> _accu_reward;
    std::atomic<float> _accu_reward_all_game;
    std::atomic<float> _accu_reward_last_game;
    std::atomic<int> _n_complete_game;
    std::atomic<int> _n_merged;
};

class MyContext {
 public:
   MyContext(const Options &opt, const std::string& eval_name, const std::string& train_name)
     : options_(opt), eval_name_(eval_name), train_name_(train_name) {
     }

   void setGameFactory(GameFactory factory) {
     factory_ = factory;
   }

   void setGameContext(elf::GCInterface* ctx) {
     int num_games = ctx->options().num_game_thread;

     using std::placeholders::_1;

     for (int i = 0; i < num_games; ++i) {
       auto* g = ctx->getGame(i);
       if (g != nullptr) {
         games_.emplace_back(new _Bundle(i, options_, ctx->getClient(), factory_, eval_name_, train_name_));
         g->setCallbacks(std::bind(&_Bundle::OnAct, games_[i].get(), _1));
       }
     }

     regFunc(ctx);
   }

   std::unordered_map<std::string, int> getParams() const {
     assert(! games_.empty());
     const GameInterface &game = *games_[0]->game;

     auto params = game.getParams();

     params["num_action"] = game.numActions();
     params["frame_stack"] = options_.frame_stack;
     params["T"] = options_.T;
     return params;
   }

   std::string getSummary() {
     Summary summary;
     for (const auto &g : games_) {
       summary.feed(g->summary);
     }
     return summary.print();
   }

   Spec getBatchSpec() const { return spec_; }

 private:
  Options options_;
  GameFactory factory_;
  std::string eval_name_;
  std::string train_name_;
  Spec spec_;

  struct _Bundle {
    std::unique_ptr<GameInterface> game;
    Reply reply;

    decorator::FrameStacking stacking;
    Replay replay;

    Summary summary;

    GameClientInterface *client;
    std::string eval_name;
    std::string train_name;
    const Options opt;

    _Bundle(int idx, const Options &opt, GameClientInterface *client, GameFactory factory,
            const std::string &eval_name, const std::string &train_name)
      : game(factory.f(idx)), reply(game->numActions()),
        stacking(opt.frame_stack, game->dim()),
        replay(opt.T, opt.frame_stack * game->dim()),
        client(client),
        eval_name(eval_name),
        train_name(train_name),
        opt(opt) {

       assert(game.get() != nullptr);
       assert(game->numActions() > 0);
       reset();

       reply.game_idx = idx;
       reply.cnt = 0;
       reply.tick = 0;
    }

    void reset() {
      game->reset();
      stacking.reset();
      replay.reset([](Reply &r) { r.reset(); });
      summary.reset();
      reply.tick = 0;
      reply.cnt ++;
    }

    void OnAct(elf::game::Base *base) {
      // std::cout << "Get reply: a1: " << reply_.a1 << ", a2: " << reply_.a2 << std::endl;
      (void)base;

      reply.clear();
      stacking.feedObs(game->feature());

      bool game_end = false;
      // std::cout << "reply.size() = " << reply.pi.size() << std::endl;
      ActorSender as(stacking, reply);
      // std::cout << "Send wait" << std::endl;
      if (client->sendWait(eval_name, as)) {
        game_end = ! game->step(&reply);
        summary.feed(reply.r);
        if (opt.reward_clip > 0) {
          reply.r = std::min(std::max(reply.r, -opt.reward_clip), opt.reward_clip);
        }
        reply.terminal = game_end ? 1 : 0;
        replay.feedReplay(stacking.feature(), Reply(reply));
      }

      if (replay.needSendReplay() || game_end) {
        // std::cout << "Sending training to " << train_name << std::endl;
        TrainSender ts(replay);
        client->sendWait(train_name, ts);
      }

      if (game_end) {
        reset();
      } else {
        reply.tick ++;
      }
    }
  };

  std::vector<std::unique_ptr<_Bundle>> games_;

  static SpecItem getSpec(const Extractor &e) {
    return SpecItem{
      { "input", e.getState2MemNames() },
      { "reply", e.getMem2StateNames() },
    };
  }

  void regFunc(elf::GCInterface *ctx) {
    assert(! games_.empty());
    const GameInterface &game = *games_[0]->game;

    Extractor& e = ctx->getExtractor();
    int batchsize = ctx->options().batchsize;
    
    Extractor e_actor = ActorSender::reg(game, batchsize, options_.frame_stack);
    spec_[eval_name_] = getSpec(e_actor);
    e.merge(std::move(e_actor));

    Extractor e_train = TrainSender::reg(game, batchsize, options_.T, options_.frame_stack);
    spec_[train_name_] = getSpec(e_train); 
    e.merge(std::move(e_train));
  }
};

} // namespace snippet

} // namespace elf
