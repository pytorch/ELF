# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

# XXX hack fix path
import os
import random
import sys

import torch.multiprocessing as _mp

import utils_elf


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'elf'))
mp = _mp.get_context('spawn')


'''
Usage:
    In process main function, run the following and then
    you get a shared model.

    if rank == 0:
        model = build_model(with_cuda)
    else:
        model = None

    model = param_server.sync_model(rank, model)

'''


class Cond:
    ''' Wrapper for `Condition` class from torch multiprocessing'''

    def __init__(self):
        self.cond = mp.Condition()

    def wait(self):
        self.cond.acquire()
        self.cond.wait()
        self.cond.release()

    def wait_noblock(self):
        self.cond.acquire()
        self.cond.wait(0)
        self.cond.release()

    def notify(self):
        self.cond.acquire()
        self.cond.notify()
        self.cond.release()


class ParameterServer(object):
    ''' ParameterServer to handle updates in the model concurrently '''

    def __init__(self, n_processes):
        ''' Initialization.

        Args:
            n_processes: number of processes.
        '''
        self.queue = mp.Queue()
        self.n_processes = n_processes
        self.barrier = mp.Barrier(n_processes)
        # For update signal.
        self.send_done = Cond()
        self.recv_done = Cond()

    def __getstate__(self):
        return (
            self.queue,
            self.barrier,
            self.n_processes,
            self.send_done,
            self.recv_done)

    def __setstate__(self, state):
        self.queue, self.barrier, self.n_processes, \
            self.send_done, self.recv_done = \
            state

    def server_send_model(self, mi):
        """Send the model to others and starts to wait.

        Finish waiting if all client receives the model.

        Args:
            mi(`ModelInterface`): model interface to send
        """
        assert mi is not None
        for i in range(self.n_processes - 1):
            self.queue.put(mi)
        self._server_shared_mi = mi

        self.barrier.wait()

    def client_receive_model(self):
        """Receive model from the queue.

        Finish waiting if all client receives the model.

        Returns:
            `ModelInterface` shared in clients.
        """
        mi = self.queue.get()
        # clone the gradients to break the sharing
        for _, model in mi.models.items():
            for param in model.parameters():
                if param.grad is not None:
                    param._grad = param.grad.clone()

        self.barrier.wait()
        self._client_shared_mi = mi
        return self._client_shared_mi

    def server_update_model(self, key, new_mi, noblock=False):
        ''' Update shared model in the server, wait until all clients receive.

        Args:
            key(str): the key in ``models`` to update
            new_mi(`ModelInterface`): new model interface to update
            noblock(bool): indicates if updating models block other threads.
                           Default is blocking.
        '''
        # if recv is not done, skip it.
        if noblock:
            try:
                self.recv_done.wait_noblock()
            except BaseException:
                # The recv is not done yet. Cannot send.
                return False
        else:
            self.recv_done.wait()

        self._server_shared_mi.update_model(key, new_mi)

        # Then wait until other people have received.
        self.send_done.notify()
        return True

    def client_refresh_model(self, gpu=None, skip=False):
        ''' Clone updated shared model from the server.

        Args:
            gpu(int): gpu index
            skip(bool): if we skip this model.
                        Will return ``None`` if set to ``True``

        Returns:
            refreshed model.
        '''

        # First wait until we are synced up.
        self.send_done.wait()
        if not skip:
            mi = self._client_shared_mi.clone(gpu=gpu)
        else:
            mi = None
        self.recv_done.notify()
        return mi


class SharedData:
    def __init__(self, total_process, mi, batch_template,
                 cb_remote_initialize=None,
                 cb_remote_batch_process=None,
                 args=None):
        ''' Initialize `SharedData` class with a few hooks

        Args:
            total_process: number of processes
            mi: ModelInterface
            batch_template:
            cb_remote_initialize: Callbacks for remote Initialization
            cb_remote_batch_process: Callbacks for remote process
            args: additional arguments
        '''
        self.server = ParameterServer(total_process)
        self.cb_remote_initialize = cb_remote_initialize
        self.cb_remote_batch_process = cb_remote_batch_process
        self.args = args

        # def get_gpu_id(i): return i + 1
        def get_gpu_id(i): return 0

        # Share only training batches.
        shared_batches = []
        cvs_send = []
        cvs_recv = []
        qs = []
        for i in range(total_process - 1):
            # gpu_id = get_gpu_id(i)
            # shared_batches.append(
            #     cpu2gpu(all_batches[train_idx][0], gpu=gpu_id))
            shared_batches.append(utils_elf.pin_clone(batch_template))
            qs.append(mp.Queue(1))
            qs[-1].put(shared_batches[i])
            cvs_send.append(Cond())
            cvs_recv.append(Cond())

        self.cvs_send = cvs_send
        self.cvs_recv = cvs_recv
        self.shared_batches = shared_batches
        self.qs = qs
        self.b = mp.Barrier(total_process)

        self.optimizers = [
            mp.process(
                target=self.process_main, args=(
                    i, get_gpu_id(i))) for i in range(
                total_process - 1)]
        for optimizer in self.optimizers:
            optimizer.start()

        # Wait until all models have received the shared memory.
        self.b.wait()

        self.server.server_send_model(mi)

    def process_main(self, i, gpu_id):
        ''' Main process. Transportation between cpu and gpu.

        Args:
            i(int): process id
            gpu_id(int): gpu id
        '''
        batch = self.qs[i].get()
        self.b.wait()

        batch_gpu = utils_elf.cpu2gpu(batch, gpu=gpu_id)

        mi = self.server.client_receive_model()
        context = self.cb_remote_initialize(mi, gpu_id, self.args)
        print(
            "[%d] Context initialization completed, gpu_id = %d.. " %
            (i, gpu_id))

        # Ready.
        self.cvs_send[i].notify()

        while True:
            self.cvs_recv[i].wait()
            utils_elf.transfer_cpu2gpu(batch, batch_gpu, non_blocking=True)
            self.cvs_send[i].notify()
            self.cb_remote_batch_process(context, batch_gpu)

    def send_batch(self, batch):
        ''' Send batch to a cpu process

        Args:
            batch(dict): batch data
        '''
        process_idx = random.randint(0, len(self.shared_batches) - 1)
        try:
            self.cvs_send[process_idx].wait_noblock()
            utils_elf.transfer_cpu2cpu(batch, self.shared_batches[process_idx])
            self.cvs_recv[process_idx].notify()
            return True
        except Exception as e:
            # print("Failed to send batch to %d" % process_idx)
            # print(type(e))
            # print(e.args)
            # print(e)
            return False
