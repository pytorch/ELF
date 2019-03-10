# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import zmq


class ZMQCtx:
    def __init__(self):
        pass

    def __enter__(self):
        pass

    def __exit__(self, ty, value, tb):
        if value is not None:
            # print(value)
            pass
        return True
        # print("Send failed for " + self.identity + "..")


class ZMQSender:
    def __init__(self, addr, identity, send_timeout=0, recv_timeout=0):
        self.ctx = zmq.Context()
        self.ctx.setsockopt(zmq.IPV6, 1)

        self.sender = self.ctx.socket(zmq.DEALER)
        self.sender.identity = identity.encode('ascii')
        # self.sender.set_hwm(10000)

        if send_timeout > 0:
            self.sender.SNDTIMEO = send_timeout
        if recv_timeout > 0:
            self.sender.RCVTIMEO = recv_timeout
        self.sender.connect(addr)

    def Send(self, msg, copy=False):
        with ZMQCtx():
            self.sender.send(msg, copy=copy)
            return True
        return False

    def Receive(self):
        with ZMQCtx():
            return self.sender.recv()
        return None


class ZMQReceiver:
    def __init__(self, addr, timeout=0):
        self.ctx = zmq.Context()
        self.ctx.setsockopt(zmq.IPV6, 1)
        self.receiver = self.ctx.socket(zmq.ROUTER)
        # self.receiver.set_hwm(10000)

        if timeout > 0:
            self.receiver.RCVTIMEO = timeout
        self.receiver.bind(addr)

    def Send(self, identity, msg):
        with ZMQCtx():
            self.receiver.send_multipart([identity, msg])
            return True
        return False

    def Receive(self):
        # return identity, msg
        with ZMQCtx():
            identity, msg = self.receiver.recv_multipart()
            # print(identity)
            # print(msg)
            return identity, msg
        return None, None
