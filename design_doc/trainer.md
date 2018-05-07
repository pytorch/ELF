# Trainer and Evaluation in ELF

## Evaluator
- class Evaluator is a pure python class, which run neural network in eval mode and get return results and update some stat info
```python
class Evaluator(object):
  def __init__(self, option_map, name, ...):

  def setup(self, mi=None, sampler=None):
    self.mi = mi
    self.sampler = sampler

    if self.stats is not None:
      self.stats.reset()

  def episode_start(self, i):
    self.actor_count = 0

  def actor(self, batch):
    # get model form self.mi, set volatile=True
    # forward()
    # feed_batch() to update self.stats
    # return reply_msg

  def episode_summary(self, i):
    # called after each episode
```

## Trainer
- Trainer is also a pure python class wrapped on evaluator.
```python
class Trainer(object):
  def __init__(self, option_map, ...):
    self.saver = ModelSaver(option_map)
    self.counter = MultiCounter() # in utils.py
    self.evaluator = Evaluator()

  def setup(self, rl_method, mi, sampler):
    self.mi = mi
    self.sampler = sampler
    self.rl_method = rl_method

  def actor(self, batch):
    # run eval mode

  def train(self, batch, *args, **kwargs):
    mi = self.evaluator.mi
    mi.zero_grad()
    # call update() mcts_prediction.py
    res = self.rl_method.update(mi, batch, stats)
```
