
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

