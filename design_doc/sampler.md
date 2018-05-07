# Sampler in ELF

Python class in src_py/rlpytorch/sampler/ folder. It is a little out-of-date and not realy used in the Go project.

## Class Sampler
- Used to sample an action from policy.
- In ELF-Go, MCTS and sampling move is integrated together in C++, so this part of codes are not actually used.
```python
class Sampler(object):
  def __init__(self, option_map):
    self.sample_nodes = []

  def sample(self, state_curr):
    # Sample an action from distribution using a certain sample method
    # method could be epsilon_greedy or multinomial
```

## Sampler Methods
- Sample with check:
```python
def sample_with_check(probs, greedy=False):

def sample_eps_with_check(probs, epsilon, greedy=False):
```
- Two sample methods provided based on numpy:
```python
import numpy

def uniform_multinomial(batchsize, num_action, use_cuda=True):

def epsilon_greedy(state_curr, args, node="pi"):
```