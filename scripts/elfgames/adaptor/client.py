import gym
import numpy as np

from elf import EnvWrapper

def transpose(v):
    return np.transpose(v, axes=[2, 1, 0])

class Env:
    def __init__(self):
        self.env = gym.make("Breakout-v0")

    def reset(self):
        s = self.env.reset()
        return transpose(s[::2,::2,:])

    def step(self, action):
        next_s, reward, terminal, _ = self.env.step(action)
        return transpose(next_s[::2,::2,:]), reward, terminal, _ 

    def getActionSize(self):
        return [1]

    def getStateSize(self):
        sz = self.env.observation_space.shape[::-1]
        return (sz[0], sz[1] // 2, sz[2] // 2)

    def getNumAction(self):
        return self.env.action_space.n

sender = EnvWrapper()
env = Env()
sender.setEnv(env)
sender.run()

