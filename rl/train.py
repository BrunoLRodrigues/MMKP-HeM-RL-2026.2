"""
@file train.py
@brief Training script for the MMKP RL agents using Stable-Baselines3 SAC.
"""

import re
from pathlib import Path
from typing import Any, Dict

import mmkp_sim
import numpy as np
from stable_baselines3 import SAC
from stable_baselines3.common.vec_env import DummyVecEnv, VecNormalize

from env import MMKPSimulatorEnv

CONFIG: Dict[str, Any] = {
    # Controle de Episódios e Execuções
    # Use "auto" para continuar automaticamente de onde parou (ex: 1..100 -> 101..200)
    # ou defina um número inteiro explícito caso queira forçar o episódio inicial.
    "start_episode": "auto",
    "episodes_to_train": 100,                  # Quantidade de NOVOS episódios nesta rodada
    "num_runs_per_episode": 30,                # Nº de execuções do TSA (e BGR) por episódio
    "log_interval": 10,                        # Intervalo de episódios para log no terminal (1 = todo episódio)

    # Rótulos gravados na coluna Model_Type do metaheuristics.txt
    "model_type_labels": {
        "T": "T",
        "alpha": "alpha",
        "both": "both",
    },

    # Parâmetros da Instância (Simulador MMKP)
    "devices_range": (15000, 25000),           # (mín, máx) de dispositivos IoT sorteados
    "ec_range": (499, 499),                    # (mín, máx) de servidores Edge (EC)
    "cc_range": (1, 1),                        # (mín, máx) de servidores Cloud (CC)
    "services": 100,                           # Quantidade de perfis de serviços
    "fixed_penalty": False,                    # Penalidade fixa (True=1.0) ou variável (False)

    # Limites de Parametrização do TSA (Espaço de Ação do RL)
    "temp_bounds": (1.0, 100.0, 50.0),         # Temperatura: (mín, máx, centro)
    "alpha_k_bounds": (1.0, 5.0, 3.0),         # Expoente k de alpha = 1 - 10^(-k): (mín, máx, centro)
    "default_temp": 50.0,                      # Temperatura padrão (usada quando mode="alpha")
    "default_alpha": 0.999,                    # Alpha padrão (usado quando mode="T")

    # Normalização e Hiperparâmetros do SAC (Stable-Baselines3)
    "clip_obs": 10.0,
    "learning_rate": 1e-3,
    "buffer_size": 50000,
    "learning_starts": 20,                     # Passos iniciais aleatórios (apenas no 1º treino do zero)
    "batch_size": 32,
    "gamma": 0.0,                              # Contextual Bandit (passo único)
    "tau": 0.005,
    "train_freq": (1, "step"),
    "gradient_steps": 1,
    "ent_coef": "auto",
    "verbose": 1,
}

AGENTS_CONFIG = [
    # --- 3 Agentes TSA + GCB (Opção 5 no main.cpp) ---
    ("exp1_T_GCB", "T", 5),
    ("exp2_alpha_GCB", "alpha", 5),
    ("exp3_both_GCB", "both", 5),

    # --- 3 Agentes TSA + BGR (Opção 8 no main.cpp) ---
    ("exp4_T_BGR", "T", 8),
    ("exp5_alpha_BGR", "alpha", 8),
    ("exp6_both_BGR", "both", 8),
]

class SharedEpisodeCallback:
    """
    @brief Cycles deterministically through a fixed window of episode IDs across agents.
    """

    def __init__(self, start_episode: int, num_episodes: int) -> None:
        """
        @brief Initializes the shared episode iterator.
        @param start_episode First episode ID in the window.
        @param num_episodes Total number of episodes in the window.
        """
        self.start_episode = start_episode
        self.num_episodes = num_episodes
        self.step_idx = 0

    def get_next_episode_id(self) -> int:
        """
        @brief Returns the next episode ID in the sequence.
        @return int Next 1-based episode identifier.
        """
        ep_id = self.start_episode + (self.step_idx % self.num_episodes)
        self.step_idx += 1
        return ep_id

def detect_next_start_episode(project_root: Path, cfg: Dict[str, Any]) -> int:
    """
    @brief Detects the starting episode ID for incremental training.
    @param project_root Root directory path of the repository.
    @param cfg Training configuration dictionary.
    @return int Next episode identifier to train.
    """
    configured = cfg.get("start_episode", "auto")
    if isinstance(configured, int) and configured > 0:
        return configured

    training_data_dir = project_root / "data" / "training"
    if not training_data_dir.exists():
        return 1

    max_ep = 0
    for entry in training_data_dir.iterdir():
        if entry.is_dir():
            match = re.fullmatch(r"episode_(\d+)", entry.name)
            if match:
                ep_num = int(match.group(1))
                if ep_num > max_ep:
                    max_ep = ep_num

    return max_ep + 1

def initialize_all_episodes(start_ep: int, cfg: Dict[str, Any]) -> None:
    """
    @brief Initializes episode datasets and initial heuristic snapshots once per episode.
    @param start_ep First episode index to prepare.
    @param cfg Training configuration dictionary.
    """
    runner = mmkp_sim.PyMMKPRunner()
    rng = np.random.default_rng()

    total_eps = cfg["episodes_to_train"]
    end_ep = start_ep + total_eps - 1

    print("\n==================================================")
    print(f" Inicializando Episódios {start_ep} até {end_ep} (GCB 1x | BGR {cfg['num_runs_per_episode']}x)")
    print("==================================================")

    for ep_id in range(start_ep, end_ep + 1):
        ec_count = int(rng.integers(cfg["ec_range"][0], cfg["ec_range"][1] + 1))
        cc_count = int(rng.integers(cfg["cc_range"][0], cfg["cc_range"][1] + 1))
        runner.prepare_episode(
            episode_id=ep_id,
            devices_min=cfg["devices_range"][0],
            devices_max=cfg["devices_range"][1],
            services=cfg["services"],
            ec=ec_count,
            cc=cc_count,
            fixed_penalty=cfg["fixed_penalty"],
            num_runs=cfg["num_runs_per_episode"],
        )
    print(f"[OK] Episódios {start_ep} a {end_ep} e suas heurísticas iniciais prontos em cache e salvos em disco.")

def train_incremental_agent(
    agent_tag: str,
    mode: str,
    option: int,
    start_ep: int,
    cfg: Dict[str, Any],
) -> None:
    """
    @brief Trains or resumes incremental training for a single SAC agent.
    @param agent_tag Identifier directory name under `rl/models/`.
    @param mode Parameterization mode (`"T"`, `"alpha"`, or `"both"`).
    @param option Constructive heuristic selector (`1` for GCB, `4` for BGR).
    @param start_ep Starting episode ID for this training batch.
    @param cfg Training configuration dictionary.
    """
    project_root = Path(__file__).resolve().parent.parent
    models_dir = project_root / "rl" / "models" / agent_tag
    models_dir.mkdir(parents=True, exist_ok=True)

    model_path = models_dir / "sac_model.zip"
    replay_buffer_path = models_dir / "replay_buffer.pkl"
    stats_path = models_dir / "vec_normalize.pkl"

    target_episodes = cfg["episodes_to_train"]
    end_ep = start_ep + target_episodes - 1
    model_label = cfg["model_type_labels"][mode]

    print(f"\n==================================================")
    print(f" Agente: {agent_tag} (mode={mode}, Model_Type={model_label}, option={option})")
    print(f"==================================================")

    raw_env = DummyVecEnv([
        lambda: MMKPSimulatorEnv(
            mode=mode,
            model_label=model_label,
            option=option,
            num_runs=cfg["num_runs_per_episode"],
            devices_range=cfg["devices_range"],
            ec_range=cfg["ec_range"],
            cc_range=cfg["cc_range"],
            temp_bounds=cfg["temp_bounds"],
            alpha_k_bounds=cfg["alpha_k_bounds"],
            default_temp=cfg["default_temp"],
            default_alpha=cfg["default_alpha"],
            services=cfg["services"],
            fixed_penalty=cfg["fixed_penalty"],
        )
    ])

    # 1. Carrega ou cria o normalizador de estado (VecNormalize)
    if stats_path.exists():
        print(f"[{agent_tag}] Carregando normalização anterior de {stats_path}...")
        env = VecNormalize.load(str(stats_path), raw_env)
        env.training = True
        env.norm_reward = False
    else:
        print(f"[{agent_tag}] Criando nova normalização de estado...")
        env = VecNormalize(
            raw_env, norm_obs=True, norm_reward=False, clip_obs=cfg["clip_obs"]
        )

    # 2. Carrega modelo e replay buffer existentes (treino contínuo) ou cria do zero
    is_resuming = model_path.exists()
    if is_resuming:
        print(f"[{agent_tag}] Retomando treinamento do modelo salvo em {model_path}...")
        model = SAC.load(str(model_path), env=env, verbose=cfg["verbose"])
        if replay_buffer_path.exists():
            model.load_replay_buffer(str(replay_buffer_path))
            print(
                f"[{agent_tag}] Replay Buffer carregado ({model.replay_buffer.size()} transições acumuladas)."
            )
    else:
        print(f"[{agent_tag}] Treinando novo modelo do zero...")
        model = SAC(
            policy="MlpPolicy",
            env=env,
            learning_rate=cfg["learning_rate"],
            buffer_size=cfg["buffer_size"],
            learning_starts=cfg["learning_starts"],
            batch_size=cfg["batch_size"],
            gamma=cfg["gamma"],
            tau=cfg["tau"],
            train_freq=cfg["train_freq"],
            gradient_steps=cfg["gradient_steps"],
            ent_coef=cfg["ent_coef"],
            verbose=cfg["verbose"],
        )

    callback = SharedEpisodeCallback(
        start_episode=start_ep,
        num_episodes=target_episodes,
    )

    print(f"[{agent_tag}] Executando {target_episodes} novos episódios (de {start_ep} até {end_ep})...")

    env_instance = env.envs[0]
    original_reset = env_instance.reset

    def custom_reset(*args, **kwargs):
        ep_id = callback.get_next_episode_id()
        if kwargs.get("options") is None:
            kwargs["options"] = {}
        kwargs["options"]["episode_id"] = ep_id
        return original_reset(*args, **kwargs)

    env_instance.reset = custom_reset

    # reset_num_timesteps=False mantém o contador total de episódios e não repete o learning_starts
    model.learn(
        total_timesteps=target_episodes,
        log_interval=cfg["log_interval"],
        reset_num_timesteps=not is_resuming,
    )

    # 3. Salva modelo, replay buffer e estatísticas de normalização
    model.save(str(model_path))
    model.save_replay_buffer(str(replay_buffer_path))
    env.save(str(stats_path))

    if not model_path.exists() or not stats_path.exists() or not replay_buffer_path.exists():
        raise RuntimeError(f"Erro ao salvar artefatos do agente {agent_tag} em {models_dir}")

    print(
        f"[{agent_tag}] Salvo com sucesso (Total acumulado: {model.num_timesteps} episódios | "
        f"Buffer: {model.replay_buffer.size()} transições)."
    )

if __name__ == "__main__":
    project_root = Path(__file__).resolve().parent.parent

    next_start_episode = detect_next_start_episode(project_root, CONFIG)

    initialize_all_episodes(next_start_episode, CONFIG)

    for agent_tag, mode, option in AGENTS_CONFIG:
        train_incremental_agent(
            agent_tag=agent_tag,
            mode=mode,
            option=option,
            start_ep=next_start_episode,
            cfg=CONFIG,
        )