# RLPyTorch: A Simple Reinforcement Learning Package in PyTorch

Overview    
==============
Here we provide a simple reinforcement learning package as a backend for ELF. In the root directory there is basic framework for Rlpytorch.

* `args_provider.py`
Provide basic argument management mechanism. In RLPyTorch, each component (methods, model loader, etc) has its own argument set.  
Note that `args_utils.py` is a symlink for backward compatibility.

* `model_base.py`
Wrapper of `nn.Module`, providing save/load/counting steps etc.

* `model_interface.py`
Wrapper of multiple models.

* `model_loader.py`
Utility for loading saved models.

In the subfolders there are multiple components in RLPyTorch:

1. `methods`
Basic RL methods, e.g., PolicyGradient, ActorCritic, etc. Some methods are constructed from other basic ones. For example, `ActorCritic = PolicyGradient + DiscountedReward + ValueMatcher`. This makes the library easier to extend. For example, you might replace `DiscountedReward` with the final reward, which becomes REINFORCE. You might also replace it with Generalized Advantage Estimation (GAE) or $\lambda$-return, etc.  

2. `trainer`
Provider example callback functions for ELF framework. A simple example looks like the following:
```
GC.start()
GC.reg_callback("train", train_callback)
GC.reg_callback("actor", actor_callback)
while True:
    GC.run()
GC.stop()
```
The `GC.run()` function waits until the next batch with a specific tag arrives, then call registered callback functions.  

3. `runner`
Customize how to run the loop of `GC`. E.g., whether to run it with progress bar, in a single process or multiple processes, etc.

4. `sampler`
Different ways to sample the actor model (to get the next action).   

5. `stats`
Supporting classes to collect various statistics.

6. `utils`
Internal utilities.  


Actor Critic model  
-------------
We implemented advantageous actor-critic models, similar to Vanilla A3C, but with off-policy corrections with importance sampling.

Specifically, we use the trajectory sampled from the previous version of the actor in the actor-critic update.
