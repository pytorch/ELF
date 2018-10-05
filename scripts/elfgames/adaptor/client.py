import gym
import numpy as np

from elf import EnvWrapper

env = gym.make("Breakout-v0")

sender = EnvWrapper()
sender.setEnv(env)

action_size = [1]
state_size = env.observation_space.shape

sender.run(state_size, action_size, transpose=True)

