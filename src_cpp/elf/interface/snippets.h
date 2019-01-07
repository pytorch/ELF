#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <vector>

#include "decorator.h"
#include "game_interface.h"

// #define SNIPPET_DEBUG

#ifdef SNIPPET_DEBUG
#define PRINT(...) \
    if (reply.game_idx == 0) \
    std::cout << elf_utils::msec_since_epoch_from_now() << " [" << std::this_thread::get_id() << "][idx=" << reply.game_idx << "][" << reply.tick << "][" << reply.cnt << "] " << __VA_ARGS__ << std::endl;
#else
#define PRINT(...)
#endif

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
    clear();
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
  size_t getTerminal(int *tterminal) const { *tterminal = terminal; return 1; }
};

using Replay = elf::decorator::ShortReplayBuffer<Reply>;

// Game has the following interface:
class Game {
 public:
  virtual std::vector<float> feature() const = 0;

  // return false if the game has come to an end, and change the reply.
  virtual bool step(Reply *) = 0;
  virtual void reset() = 0;

  virtual ~Game() = default;
};

class Interface {
 public:
  virtual int dim() const = 0;
  virtual std::vector<int> dims() const = 0;
  virtual int numActions() const = 0;
  virtual std::unordered_map<std::string, int> getParams() const = 0;

  virtual Game *createGame(int game_idx, bool) const = 0;
};

struct ActorSender {
  const decorator::FrameStacking &stacking;
  Reply &reply;

  ActorSender(const decorator::FrameStacking &stacking, Reply &reply) : stacking(stacking), reply(reply) { }

  void getFeature(float *f) const { auto ff = stacking.feature(); std::copy(ff.begin(), ff.end(), f); }
  void getCnt(int *cnt) const { reply.getCnt(cnt); }
  void getTick(int *t) const { reply.getTick(t); }

  void setValue(const float *V) { reply.setValue(V); }
  void setPi(const float *pi) { reply.setPi(pi); }
  void setAction(const int64_t *a) { reply.setAction(a); }

  static Extractor reg(const Interface &interface, int batchsize, int frame_stack) {
     Extractor e;
     std::vector<int> dims = interface.dims();

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
       .addExtents(batchsize, {batchsize, interface.numActions()})
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
  void getPi(float *pi) const { replay.histReply().extractForward(elf::FULL_ONLY, pi, &Reply::getPi); }
  void getValue(float *V) const { replay.histReply().extractForward(elf::FULL_ONLY, V, &Reply::getValue); }
  void getAction(int64_t *a) const { replay.histReply().extractForward(elf::FULL_ONLY, a, &Reply::getAction); }
  void getTick(int *tick) const { replay.histReply().extractForward(elf::FULL_ONLY, tick, &Reply::getTick); }
  void getReward(float *reward) const { replay.histReply().extractForward(elf::FULL_ONLY, reward, &Reply::getReward); }
  void getTerminal(int *terminal) const { replay.histReply().extractForward(elf::FULL_ONLY, terminal, &Reply::getTerminal); }

  static Extractor reg(const Interface &interface, int batchsize, int T, int frame_stack) {
     Extractor e;

     std::vector<int> dims = interface.dims();
     dims.insert(dims.begin(), T);
     dims.insert(dims.begin(), batchsize);
     dims[2] *= frame_stack;

     e.addField<float>("s_")
       .addExtents(batchsize, dims)
       .addFunction<TrainSender>(&TrainSender::getFeature);

     e.addField<float>("pi_")
       .addExtents(batchsize, {batchsize, T, interface.numActions()})
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
  DEF_FIELD(int, num_eval_games, 0, "number of evaluation games")
  // DEF_FIELD_NODEFAULT(elf::Options, base, "ELF options")
DEF_END

class Stats;

class Summary {
 public:
    std::string print() const {
      std::stringstream ss;
      ss << "Total step: " << static_cast<float>(_total_step + _total_ongoing_step) / 1e6 << "M, ";
      ss << "#step (completed episode): " << static_cast<float>(_total_step) / 1e6 << "M " << std::endl;
      if (_total_episode > 0) {
        ss << "Accumulated: " << (float)_total_reward / _total_episode << "[" << _total_episode << "]";
      } else {
        ss << "0[0]";
      }
      if (_n_merged > 0) {
        ss << ", Last episode[" << _n_merged << "] Avg: " << (float)_total_reward_last_game / _n_merged
           << ", Min: " << _min_reward_last_game << ", Max: " << _max_reward_last_game << std::endl;
      } else {
        ss << "N/A";
      }
      return ss.str();
    }

    friend class Stats;

 private:
    float _total_reward = 0;
    int _total_episode = 0;
    int _total_step = 0;
    int _total_ongoing_step = 0;

    float _total_reward_last_game = 0.0;
    float _max_reward_last_game = -std::numeric_limits<float>::max();
    float _min_reward_last_game = std::numeric_limits<float>::max();

    int _n_merged = 0;
};

class Stats {
public:
    Stats() {}

    void feed(float curr_reward) {
      std::lock_guard<std::mutex> lock(mutex_);
      _started = true;
      _accu_reward += curr_reward;
      _accu_step ++;
    }

    void export2summary(Summary &s) const {
      std::lock_guard<std::mutex> lock(mutex_);

      if (! _started) return;

      s._total_reward += _accu_reward_all_game;
      s._total_episode += _n_episode;
      s._total_step += _n_step;
      s._total_ongoing_step += _accu_step;

      if (_n_episode > 0) {
        // We already have a previous game.
        s._total_reward_last_game += _accu_reward_last_game;
        s._max_reward_last_game = std::max(s._max_reward_last_game, _accu_reward_last_game);
        s._min_reward_last_game = std::min(s._min_reward_last_game, _accu_reward_last_game);
        s._n_merged ++;
      }
    }

    void reset() {
      if (! _started) return;

      _accu_reward_all_game += _accu_reward;
      _accu_reward_last_game = _accu_reward;
      _n_step += _accu_step;
      _n_episode ++;

      _accu_step = 0;
      _accu_reward = 0;
    }

private:
    mutable std::mutex mutex_;

    bool _started = false;

    float _accu_reward = 0.0;
    int _accu_step = 0;
    float _accu_reward_all_game = 0.0;
    float _accu_reward_last_game = 0.0;
    int _n_episode = 0;
    int _n_step = 0;
};

class MyContext {
 public:
   MyContext(const Options &opt, const std::string& eval_name, const std::string& train_name)
     : options_(opt), eval_name_(eval_name), train_name_(train_name) {
     }

   void setInterface(Interface *factory) {
     factory_ = factory;
   }

   void setGameContext(elf::GCInterface* ctx) {
     int num_games = ctx->options().num_game_thread;

     using std::placeholders::_1;

     for (int i = 0; i < num_games; ++i) {
       auto* g = ctx->getGame(i);
       if (g != nullptr) {
         games_.emplace_back(
             new _Bundle(i, i >= num_games - options_.num_eval_games,
               options_, ctx->getClient(), factory_, eval_name_, train_name_));
         g->setCallbacks(std::bind(&_Bundle::OnAct, games_[i].get(), _1));
       }
     }

     regFunc(ctx);
   }

   std::unordered_map<std::string, int> getParams() const {
     auto params = factory_->getParams();

     params["num_action"] = factory_->numActions();
     params["frame_stack"] = options_.frame_stack;
     params["T"] = options_.T;
     return params;
   }

   std::string getSummary() {
     Summary summary;
     Summary summary_eval;

     int n_eval = 0;
     for (const auto &g : games_) {
       if (g->eval_mode) {
         g->stats.export2summary(summary_eval);
         n_eval ++;
       } else {
         g->stats.export2summary(summary);
       }
     }

     if (n_eval == 0) return summary.print();
     else {
       return "Train: \n" + summary.print() + "\nEval:\n" + summary_eval.print();
     }
   }

   Spec getBatchSpec() const { return spec_; }

 private:
  Options options_;
  Interface *factory_ = nullptr;
  std::string eval_name_;
  std::string train_name_;
  Spec spec_;

  struct _Bundle {
    std::unique_ptr<Game> game;
    Reply reply;

    decorator::FrameStacking stacking;
    Replay replay;

    bool eval_mode;
    Stats stats;

    GameClientInterface *client;
    std::string eval_name;
    std::string train_name;
    const Options opt;

    _Bundle(int idx, bool eval_mode, const Options &opt, GameClientInterface *client, Interface* factory,
            const std::string &eval_name, const std::string &train_name)
      : game(factory->createGame(idx, eval_mode)), reply(factory->numActions()),
        stacking(opt.frame_stack, factory->dim()),
        replay(opt.T, opt.frame_stack * factory->dim()),
        eval_mode(eval_mode),
        client(client),
        eval_name(eval_name),
        train_name(train_name),
        opt(opt) {

       assert(game.get() != nullptr);
       assert(factory->numActions() > 0);
       reset();

       reply.game_idx = idx;
       reply.cnt = 0;
       reply.tick = 0;
    }

    void reset() {
      game->reset();
      stacking.reset();
      replay.reset([](Reply &r) { r.reset(); });
      stats.reset();
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

      PRINT("Send wait");
      if (client->sendWait(eval_name, as)) {
        PRINT("Get reply..");
        if (eval_mode) {
          // Find max from reply.pi.
          int a = 0;
          float best_pi = -1;
          for (size_t i = 0; i < reply.pi.size(); ++i) {
            if (best_pi < reply.pi[i]) {
              best_pi = reply.pi[i];
              a = i;
            }
          }
          reply.a = a;
        }

        game_end = ! game->step(&reply);
        PRINT("Game stepped..");
        stats.feed(reply.r);

        if (! eval_mode) {
          if (opt.reward_clip > 0) {
            reply.r = std::min(std::max(reply.r, -opt.reward_clip), opt.reward_clip);
          }
          reply.terminal = game_end ? 1 : 0;
          replay.feedReplay(stacking.feature(), Reply(reply));
          PRINT("Replay fed to buffer..");
        }
      }

      if (! train_name.empty() && (replay.needSendReplay() || (game_end && replay.isFull())) ) {
        // std::cout << "Sending training to " << train_name << std::endl;
        TrainSender ts(replay);
        PRINT("Send replay..");
        client->sendWait(train_name, ts);
        PRINT("Send replay done..");
      }

      if (game_end) {
        PRINT("Resetting..");
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
    Extractor& e = ctx->getExtractor();
    int batchsize = ctx->options().batchsize;

    Extractor e_actor = ActorSender::reg(*factory_, batchsize, options_.frame_stack);
    spec_[eval_name_] = getSpec(e_actor);
    e.merge(std::move(e_actor));

    if (! train_name_.empty()) {
      Extractor e_train = TrainSender::reg(*factory_, batchsize, options_.T, options_.frame_stack);
      spec_[train_name_] = getSpec(e_train);
      e.merge(std::move(e_train));
    }
  }
};

} // namespace snippet

} // namespace elf
