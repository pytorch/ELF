import gym
import numpy as np
import time
import psutil

from elf import EnvWrapper

def transpose(v):
    return np.transpose(v, axes=[2, 1, 0])

def convert(s):
    return transpose(s[::2,::2,:]) / 255.0

env = gym.make("Breakout-v0")
sz = env.observation_space.shape[::-1]
num_action = env.action_space.n

spec = {}
spec["actor"] = dict(
    input=dict(s=("float", (sz[0], sz[1] // 2, sz[2] // 2))),
    reply=dict(a=("int32_t", 1), pi=("float", num_action), V=("float", 1))
)

wrapper = EnvWrapper()
mem = wrapper.alloc(spec)
start = time.perf_counter()
n = 0
total_n = 0

while True:
    mem["s"][:] = convert(env.reset())
    terminal = False
    while not terminal:
        wrapper.wrapper.sendAndWaitReply()
        n += 1
        if n == 20:
            elapsed = time.perf_counter() - start
            print("[%d] Time per action: %.3f msec" % (total_n, elapsed / n * 1000))
            start = time.perf_counter()
            total_n += n
            n = 0
            # gives a single float value
            #print(psutil.cpu_percent())
            # gives an object with many fields
            #print(psutil.virtual_memory())

        # print(mem["a"])
        next_s, r, terminal, _ = env.step(mem["a"])
        mem["s"][:] = convert(next_s)
