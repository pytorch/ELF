# Model Interface in ELF

## Model Interface
- class ModelInterface is a python class saving network models.
- Its member `models` is a k-v store to call a CNN model by name.
```python
class ModelInterface(object):
  def __init__(self, option_map):
    self.option_map = option_map
    self.models = {}
    self.old_models = deque()
    self.optimizers = {}

  def clone(self, gpu=None):
    # return a deep copy of self

  def __contains__(self, key):
    # overload the "in" operator

  def add_model(self, key, model, ...):
    # add a model to self.models[key]

  def update_model(self, key, model):
    # reload model parameter from a state_dict

  def remove_model(self, key):
    # delete a model by key

  def average_model(self, key, model):
    # average self.models[key] and model

  def copy(self, dst_key, src_key):
    # copy self.model[src_key] to self.model[dst_key]

  def zero_grad(self):
    # zero_grad items in self.optimizers

  def __get__item(self, key):
    # Get an item associated with ``key`` from self.models
```