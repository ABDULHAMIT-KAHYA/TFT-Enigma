import torch
from torch import nn


class LinearPolicyScorer(nn.Module):
    def __init__(self, feature_dim: int):
        super().__init__()
        self.linear = nn.Linear(feature_dim, 1)

    def forward(self, x):
        return self.linear(x).squeeze(-1)


class LinearValueModel(nn.Module):
    def __init__(self, state_dim: int):
        super().__init__()
        self.linear = nn.Linear(state_dim, 1)

    def forward(self, x):
        return self.linear(x).squeeze(-1)
