"""
@file env.py
@brief Gymnasium environment wrapper for the in-memory MMKP simulator via pybind11.
"""

from typing import Any, Tuple

import gymnasium as gym
import mmkp_sim
import numpy as np
from gymnasium import spaces

ZERO: float = 1e-13
TIME_LIMIT: float = 1.0 - ZERO


class MMKPSimulatorEnv(gym.Env):
    """
    @brief Gymnasium environment wrapping the in-memory C++ MMKP simulator via pybind11.
    @details Exposes a 16-feature scale-invariant observation space and a continuous action
    space in [-1.0, 1.0] mapped to Simulated Annealing initial temperature and cooling rate.
    """

    def __init__(
        self,
        mode: str = "both",
        model_label: str = "both",
        option: int = 1,
        num_runs: int = 30,
        devices_range: Tuple[int, int] = (15000, 25000),
        ec_range: Tuple[int, int] = (499, 499),
        cc_range: Tuple[int, int] = (1, 1),
        temp_bounds: Tuple[float, float, float] = (1.0, 100.0, 50.0),
        alpha_k_bounds: Tuple[float, float, float] = (1.0, 5.0, 3.0),
        default_temp: float = 50.0,
        default_alpha: float = 0.999,
        services: int = 100,
        fixed_penalty: bool = False,
        max_time: float = TIME_LIMIT,
    ) -> None:
        """
        @brief Initializes the MMKPSimulatorEnv environment instance.
        @param mode Parameterization mode (`"T"`, `"alpha"`, or `"both"`).
        @param model_label Model identifier recorded in simulation outputs.
        @param option Constructive heuristic selector (`1` for GCB, `4` for BGR).
        @param num_runs Number of independent TSA runs executed per episode step.
        @param devices_range Inclusive `(min, max)` device count bounds for training episodes.
        @param ec_range Inclusive `(min, max)` Edge server count bounds.
        @param cc_range Inclusive `(min, max)` Cloud server count bounds.
        @param temp_bounds Temperature bounds tuple `(min, max, mid)`.
        @param alpha_k_bounds Cooling exponent bounds tuple `(min, max, mid)` where `alpha = 1 - 10^(-k)`.
        @param default_temp Default temperature used when `mode == "alpha"`.
        @param default_alpha Default cooling rate used when `mode == "T"`.
        @param services Number of distinct service profiles.
        @param fixed_penalty Whether device penalties are constant (`1.0`) or task-proportional.
        @param max_time Normalization reference time limit in seconds.
        """
        super().__init__()

        self.runner = mmkp_sim.PyMMKPRunner()
        self.mode = mode
        self.model_label = model_label
        self.option = option
        self.num_runs = num_runs
        self.devices_range = devices_range
        self.ec_range = ec_range
        self.cc_range = cc_range
        self.temp_bounds = temp_bounds
        self.alpha_k_bounds = alpha_k_bounds
        self.default_temp = default_temp
        self.default_alpha = default_alpha
        self.services = services
        self.fixed_penalty = fixed_penalty
        self.max_time = max_time

        self.observation_space = spaces.Box(
            low=0.0, high=np.inf, shape=(16,), dtype=np.float32
        )

        action_dim = 2 if self.mode == "both" else 1
        self.action_space = spaces.Box(
            low=-1.0, high=1.0, shape=(action_dim,), dtype=np.float32
        )

    def _map_action(self, action: np.ndarray) -> Tuple[float, float]:
        """
        @brief Maps normalized SAC actions in [-1.0, 1.0] to `(temperature, alpha)`.
        @param action Continuous action array produced by the RL policy.
        @return Tuple[float, float] Mapped `(temperature, alpha)` values.
        """
        t_min, t_max, t_mid = self.temp_bounds
        k_min, k_max, k_mid = self.alpha_k_bounds

        if self.mode == "T":
            u_t = float(np.clip(action[0], -1.0, 1.0))
            temp = t_mid + u_t * (t_mid - t_min) if u_t <= 0 else t_mid + u_t * (t_max - t_mid)
            temp = np.clip(temp, t_min, t_max)
            alpha = self.default_alpha
        elif self.mode == "alpha":
            u_a = float(np.clip(action[0], -1.0, 1.0))
            k = k_mid + u_a * (k_mid - k_min) if u_a <= 0 else k_mid + u_a * (k_max - k_mid)
            k = np.clip(k, k_min, k_max)
            temp = self.default_temp
            alpha = 1.0 - (10.0 ** (-k))
        else:
            u_t = float(np.clip(action[0], -1.0, 1.0))
            u_a = float(np.clip(action[1], -1.0, 1.0))

            temp = t_mid + u_t * (t_mid - t_min) if u_t <= 0 else t_mid + u_t * (t_max - t_mid)
            temp = np.clip(temp, t_min, t_max)

            k = k_mid + u_a * (k_mid - k_min) if u_a <= 0 else k_mid + u_a * (k_max - k_mid)
            k = np.clip(k, k_min, k_max)
            alpha = 1.0 - (10.0 ** (-k))

        return float(temp), float(alpha)

    def reset(
        self, *, seed: int | None = None, options: dict[str, Any] | None = None
    ) -> Tuple[np.ndarray, dict[str, Any]]:
        """
        @brief Resets the environment for a new training episode and returns the state vector.
        @param seed Optional random seed forwarded to `gym.Env.reset`.
        @param options Optional dictionary supporting `"episode_id"`.
        @return Tuple[np.ndarray, dict[str, Any]] Initial 16-element state observation and info dictionary.
        """
        super().reset(seed=seed)

        ec_count = int(self.np_random.integers(self.ec_range[0], self.ec_range[1] + 1))
        cc_count = int(self.np_random.integers(self.cc_range[0], self.cc_range[1] + 1))

        req_ep = int(options.get("episode_id", 1)) if options else 1

        features = self.runner.reset_instance(
            devices_min=self.devices_range[0],
            devices_max=self.devices_range[1],
            services=self.services,
            ec=ec_count,
            cc=cc_count,
            fixed_penalty=self.fixed_penalty,
            requested_episode=req_ep,
            option=self.option,
            num_runs=self.num_runs,
        )

        current_ep = self.runner.get_current_episode()
        return np.array(features, dtype=np.float32), {"episode_id": current_ep}

    def step(
        self, action: np.ndarray
    ) -> Tuple[np.ndarray, float, bool, bool, dict[str, Any]]:
        """
        @brief Executes a single episodic TSA evaluation step for the chosen action.
        @param action Normalized action array in [-1.0, 1.0].
        @return Tuple[np.ndarray, float, bool, bool, dict[str, Any]] `(next_obs, reward, terminated, truncated, info)`.
        """
        temp, alpha = self._map_action(action)
        total_cost, max_cost, exec_time = self.runner.step_tsa(
            self.option, temp, alpha, self.num_runs, self.model_label
        )

        # Acoplamento Beta-Quadrático:
        # \hat{C} = C_med / C_max  |  \hat{T} = T_med / T_max (onde T_max = 1.0 - ZERO)
        # R = -\hat{C} * [1 + \hat{C} * (1 - \hat{C}) * (\hat{T}^2)]
        if max_cost > 0.0:
            c_hat = float(np.clip(total_cost / max_cost, 0.0, 1.0))
            t_hat = float(np.clip(exec_time / self.max_time, 0.0, 1.0))
            reward = -c_hat * (1.0 + c_hat * (1.0 - c_hat) * (t_hat ** 2))
        else:
            c_hat = 0.0
            t_hat = 0.0
            reward = 0.0

        info = {
            "episode_id": self.runner.get_current_episode(),
            "temperature": temp,
            "alpha": alpha,
            "total_cost": total_cost,
            "max_cost": max_cost,
            "execution_time": exec_time,
            "c_hat": c_hat,
            "t_hat": t_hat,
        }

        dummy_obs = np.zeros(self.observation_space.shape, dtype=np.float32)
        return dummy_obs, float(reward), True, False, info