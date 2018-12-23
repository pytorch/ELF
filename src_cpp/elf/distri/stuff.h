
    feature_.registerExtractor(ctx->options().batchsize, ctx->getExtractor());


    return std::unordered_map<std::string, int>{
      { "input_dim", options_.input_dim },
      { "num_action", options_.num_action },
    };

          _state_ext[i]->fromRecord(*r);

          // Random pick one ply.
          if (_state_ext[i]->switchRandomMove(&base->rng()))
            break;

        // std::cout << "[" << _game_idx << "] Generating D4Code.." << endl;
        _state_ext[i]->generateD4Code(&base->rng());

// Client Manager...
  size_t getNumEval() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return num_eval_then_selfplay_;
  }

  size_t getExpectedNumEval() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (num_expected_clients_ > 0) {
      return num_expected_clients_ * (1.0 - selfplay_only_ratio_);
    } else {
      return num_eval_then_selfplay_;
    }
  }

