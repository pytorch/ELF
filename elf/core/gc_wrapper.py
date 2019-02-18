import sys
from .allocator import Allocator


class GameContextWrapper:
    def __init__(self,
                 game_context,
                 spec,
                 default_batchsize,
                 default_device,
                 num_recv=1):
        self.allocator = Allocator(
            game_context,
            spec,
            default_batchsize,
            default_device,
            num_recv)
        self.game_context = game_context
        self._cb = {}

    # def reg_has_callback(self, key):
    #     return key in self.allocator.target2idx

    # def reg_callback_if_exists(self, key, cb):
    #     if self.reg_has_callback(key):
    #         self.reg_callback(key, cb)
    #         return True
    #     else:
    #         return False

    def reg_callback(self, key, cb):
        assert cb is not None
        if key not in self.allocator.target2idx:
            raise ValueError('in reg_callback, "%s" is not in the specification' % key)

        for idx in self.allocator.target2idx[key]:
            # print('Register ' + str(cb) + ' at idx: %d' % idx)
            self._cb[idx] = cb
        return True

    def _call(self, smem, *args, **kwargs):
        idx = smem.getSharedMemOptions().idx()
        target = self.allocator.idx2target[idx]
        # print('smem idx: %d, label: %s' % (idx, self.allocator.idx2name[idx]))
        # print(self.allocator.target2idx)
        if idx not in self._cb:
            raise ValueError(
                'smem.idx[%d] (target: %s) has no callback function' % (idx, target))

        batchsize = smem.effective_batchsize()
        assert batchsize > 0

        input_ = self.allocator.get_input_batch(idx, batchsize)
        # Save the infos structure, if people want to have access to state
        # directly, they can use infos.s[i], which is a state pointer.
        input_.smem = smem
        input_.max_batchsize = smem.getSharedMemOptions().batchsize()

        # Get the reply array
        reply_buffer = self.allocator.get_reply_batch(idx, batchsize)
        reply = self._cb[idx](input_, *args, **kwargs)
        if reply_buffer is None:
            assert reply is None
            return target

        assert reply is not None, 'No reply is returned but reply is needed'
        extra_keys, missing_keys = reply_buffer.copy_(reply)

        if len(extra_keys) > 0:
            print('Warning: extra keys %s in reply from [%s]\'s callback!'
                  % (extra_keys, target))
        if len(missing_keys) > 0:
            raise ValueError(
                'Missing keys %s in reply from [%s]\'s callback!'
                % (missing_keys, target))

        return target

    def _check_callbacks(self):
        # Check whether all callbacks are assigned properly.
        for key, indices in self.allocator.target2idx.items():
            for idx in indices:
                if idx not in self._cb:
                    raise ValueError(
                        ('GameContextWrapper.start(): No callback function '
                         'for key = %s and idx = %d') % (key, idx))

    def run(self, *args, **kwargs):
        '''Wait group of an arbitrary collector key.

        Samples in a returned batch are always from the same group,
        but the group key of the batch may be arbitrary.
        '''

        # print('before wait')
        smem = self.game_context.wait()
        # smem = self.GC.ctx().wait()

        # print('before calling')
        label = self._call(smem, *args, **kwargs)

        # print('before_step')
        self.game_context.step()

        # Return the label for the callback being invoked
        return label

    def start(self):
        '''Start all game environments'''
        self._check_callbacks()
        self.game_context.start()

    def stop(self):
        '''Stop all game environments.

        :func:`start()` cannot be called again after :func:`stop()`
        has been called.
        '''
        self.game_context.stop()

    def reg_sig_int(self):
        import signal

        def signal_handler(s, frame):
            print('Detected Ctrl-C!')
            self.stop()
            sys.exit(0)

        signal.signal(signal.SIGINT, signal_handler)
