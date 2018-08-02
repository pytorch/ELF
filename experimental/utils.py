import multiprocessing
import os
import sys
import json
import tqdm

class Wrapper:
    def __init__(self, root, callback):
        self.root = root
        self.callback = callback

    def get_logs(self, job):
        restart = 0
        job["logs"] = []
        while True:
            folder = str(job["id"])
            if restart > 0:
                folder = folder + "_%d" % restart
            curr_log = os.path.join(self.root, folder, "output%d-0.log" % job["job_idx"])
            if not os.path.exists(curr_log):
                break
            job["logs"].append(curr_log)
            restart += 1
        return job

    def __call__(self, job):
        job = self.get_logs(job)
        return self.callback(job)


class ParallelCheck:
    def __init__(self, fs, callback, collector, n=32, root = "/mnt/vol/gfsai-flash-east/ai-group/users/yuandong/rts"):
        p = multiprocessing.Pool(n)

        if isinstance(fs, str):
            if fs.endswith(".json"):
                jobs = json.load(open(fs))
                jobs = jobs["jobs"]
            else:
                jobs = []
                for l in open(fs):
                    try:
                        job_id = int(l)
                    except:
                        continue
                    job = {
                        "id" : job_id,
                        "job_idx": 0
                    }
                    jobs.append(job)
        else:
            raise ValueError("Unsupport input: %s", str(fs))

        wrapper = Wrapper(root, callback)
        iterator = tqdm.tqdm(jobs, ncols=100)
        for job, result in zip(iterator, p.imap(wrapper, jobs)):
            collector(job, result)

