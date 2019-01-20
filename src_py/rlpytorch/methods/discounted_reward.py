# discounted reward for value based method


class DiscountedReward:
    def __init__(self, gamma):
        self.gamma = gamma

    def setR(self, r, terminal, V, stats):
        """Set rewards and feed to stats."""
        self.R = terminal * r + (1 - terminal) * V
        stats["reward"].feed(r.mean())
        stats["reward_init"].feed(self.R.mean())

    def feed(self, r, terminal, stats):
        """Update discounted reward and feed to stats.

        Keys in a batch:

        ``r`` (tensor): immediate reward.

        ``terminal`` (float tensor, 0 or 1): whether the current game has terminated.

        Feed to stats: immediate reward and accumulated reward
        """
        # Compute the reward.
        self.R = r + (1 - terminal) * self.R * self.gamma
        stats["reward"].feed(r.mean())
        stats["reward_acc"].feed(self.R.mean())
        return self.R
