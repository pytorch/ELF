import gym
import numpy as np

import utils_elf

env = gym.make("Breakout-v0")

sender = utils_elf.EnvWrapper()
sender.setEnv(env)

action_size = [1]
state_size = env.observation_space.shape[::-1]

sender.run(action_size, state_size)

