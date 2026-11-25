"""
@file evaluate.py
@brief Validation script for executing baseline heuristics and trained SAC agents over a set of fixed scenarios.
"""

import argparse
import pickle
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import mmkp_sim
import numpy as np
from stable_baselines3 import SAC

EVAL_CONFIG: Dict[str, Any] = {
    # Parâmetros padrão do cenário (podem ser sobrescritos via linha de comando)
    "default_devices_list": [
        15000, 16000, 17000, 18000, 19000, 20000,
    ],
    "default_ec": 499,                         # Quantidade yyy de servidores Edge (EC)
    "default_cc": 1,                           # Quantidade zzz de servidores Cloud (CC)
    "services": 100,                           # Quantidade de perfis de serviços
    "fixed_penalty": False,                    # Penalidade fixa (True=1.0) ou variável (False)
    "num_runs": 30,                            # 30 execuções do TSA por abordagem

    # Checkpoint de episódios (None = extrai automaticamente do modelo carregado)
    "checkpoint_episodes": None,
    "model_filename": "sac_model.zip",         # Nome padrão do arquivo do modelo (ex: sac_model.zip ou sac_model_100.zip)

    # Hiperparâmetros fixos das Baselines (TSA Padrão) e valores default dos agentes parciais
    "baseline_temp": 50.0,
    "baseline_alpha": 0.999,

    # Limites do espaço de ação usados no treinamento (train.py)
    "temp_bounds": (1.0, 100.0, 50.0),         # (mín, máx, centro) da Temperatura
    "alpha_k_bounds": (1.0, 5.0, 3.0),         # (mín, máx, centro) do expoente k em alpha = 1 - 10^(-k)

    # Rótulos da coluna Model_Type no CSV final
    "model_type_labels": {
        "NONE": "NONE",
        "T": "T",
        "alpha": "alpha",
        "both": "both",
    },
}

@dataclass
class ApproachSpec:
    """
    @brief Specification for a single evaluation approach (baseline or trained SAC agent).
    """
    name: str
    is_rl: bool
    mode: str                                  # "NONE", "T", "alpha", ou "both"
    option: int                                # 1 = TSA + GCB | 4 = TSA + BGR
    folder_candidates: Tuple[str, ...] = ()    # Nomes possíveis da pasta em rl/models/

APPROACHES: List[ApproachSpec] = [
    ApproachSpec(name="Baseline_1_TSA_GCB", is_rl=False, mode="NONE", option=5),
    ApproachSpec(name="Baseline_2_TSA_BGR", is_rl=False, mode="NONE", option=8),
    ApproachSpec(name="Modelo_1_exp1_T_GCB", is_rl=True, mode="T", option=5, folder_candidates=("exp1_T_GCB", "exp1_temperature_GCB")),
    ApproachSpec(name="Modelo_2_exp2_alpha_GCB", is_rl=True, mode="alpha", option=5, folder_candidates=("exp2_alpha_GCB",)),
    ApproachSpec(name="Modelo_3_exp3_both_GCB", is_rl=True, mode="both", option=5, folder_candidates=("exp3_both_GCB",)),
    ApproachSpec(name="Modelo_4_exp4_T_BGR", is_rl=True, mode="T", option=8, folder_candidates=("exp4_T_BGR", "exp4_temperature_BGR")),
    ApproachSpec(name="Modelo_5_exp5_alpha_BGR", is_rl=True, mode="alpha", option=8, folder_candidates=("exp5_alpha_BGR",)),
    ApproachSpec(name="Modelo_6_exp6_both_BGR", is_rl=True, mode="both", option=8, folder_candidates=("exp6_both_BGR",)),
]

def resolve_model_dir(
    models_root: Path,
    candidates: Tuple[str, ...],
    model_filename: str = "sac_model.zip",
) -> Path:
    """
    @brief Resolves the model directory matching one of the candidate folder names.
    @param models_root Root directory containing trained RL models.
    @param candidates Tuple of candidate subdirectory names.
    @param model_filename Target model archive filename.
    @return Path Resolved model directory path.
    """
    for folder_name in candidates:
        candidate_path = models_root / folder_name
        if (candidate_path / model_filename).exists() or (candidate_path / "sac_model.zip").exists():
            return candidate_path
    raise FileNotFoundError(
        f"Modelo SAC não encontrado em {models_root}. Pastas verificadas: {candidates}"
    )


def resolve_model_file(model_dir: Path, model_filename: str = "sac_model.zip") -> Path:
    """
    @brief Resolves the `.zip` model file inside a model directory.
    @param model_dir Model directory path.
    @param model_filename Preferred model filename.
    @return Path Resolved model archive path.
    """
    target = model_dir / model_filename
    if target.exists():
        return target
    fallback = model_dir / "sac_model.zip"
    if fallback.exists():
        return fallback
    raise FileNotFoundError(f"Arquivo de modelo não encontrado em: {model_dir}")


def detect_checkpoint_episodes(
    models_root: Path,
    cfg: Dict[str, Any],
    explicit_episodes: Optional[int] = None,
) -> int:
    """
    @brief Detects the episode checkpoint count from CLI arguments, filenames, or loaded SAC weights.
    @param models_root Root directory containing trained RL models.
    @param cfg Evaluation configuration dictionary.
    @param explicit_episodes Optional explicit episode count override.
    @return int Detected episode checkpoint count.
    """
    if explicit_episodes is not None and explicit_episodes > 0:
        return int(explicit_episodes)

    cfg_ep = cfg.get("checkpoint_episodes")
    if isinstance(cfg_ep, int) and cfg_ep > 0:
        return cfg_ep

    model_filename = cfg.get("model_filename", "sac_model.zip")
    match = re.search(r"(\d+)", Path(model_filename).stem)
    if match:
        return int(match.group(1))

    for approach in APPROACHES:
        if approach.is_rl:
            model_dir = resolve_model_dir(models_root, approach.folder_candidates, model_filename)
            model_path = resolve_model_file(model_dir, model_filename)
            model = SAC.load(str(model_path))
            return int(model.num_timesteps)

    return 0


def save_to_checkpoint_file(raw_meta_file: Path, target_meta_file: Path) -> None:
    """
    @brief Appends raw C++ metaheuristic output rows into the checkpoint-specific result file.
    @param raw_meta_file Temporary `metaheuristics.txt` written by the C++ runner.
    @param target_meta_file Destination checkpoint file (e.g., `tsa_<episodes>.txt`).
    """
    if not raw_meta_file.exists():
        return

    with raw_meta_file.open("r", encoding="utf-8") as src_f:
        lines = [line.strip() for line in src_f if line.strip()]

    raw_meta_file.unlink()

    if len(lines) <= 1:
        return

    header = lines[0]
    data_rows = lines[1:]

    target_meta_file.parent.mkdir(parents=True, exist_ok=True)
    write_header = not target_meta_file.exists() or target_meta_file.stat().st_size == 0

    with target_meta_file.open("a", encoding="utf-8") as dst_f:
        if write_header:
            dst_f.write(header + "\n")
        for row in data_rows:
            dst_f.write(row + "\n")


def map_rl_action_to_hyperparams(
    action: np.ndarray,
    mode: str,
    cfg: Dict[str, Any],
) -> Tuple[float, float]:
    """
    @brief Maps normalized SAC policy actions in [-1.0, 1.0] to `(temperature, alpha)`.
    @param action Continuous policy action vector.
    @param mode Parameterization mode (`"T"`, `"alpha"`, or `"both"`).
    @param cfg Evaluation configuration dictionary.
    @return Tuple[float, float] Mapped `(temperature, alpha)` pair.
    """
    t_min, t_max, t_mid = cfg["temp_bounds"]
    k_min, k_max, k_mid = cfg["alpha_k_bounds"]
    default_temp = cfg["baseline_temp"]
    default_alpha = cfg["baseline_alpha"]

    if mode == "T":
        u_t = float(np.clip(action[0], -1.0, 1.0))
        temp = t_mid + u_t * (t_mid - t_min) if u_t <= 0 else t_mid + u_t * (t_max - t_mid)
        temp = float(np.clip(temp, t_min, t_max))
        alpha = float(default_alpha)
    elif mode == "alpha":
        u_a = float(np.clip(action[0], -1.0, 1.0))
        k = k_mid + u_a * (k_mid - k_min) if u_a <= 0 else k_mid + u_a * (k_max - k_mid)
        k = float(np.clip(k, k_min, k_max))
        temp = float(default_temp)
        alpha = float(1.0 - (10.0 ** (-k)))
    else:
        u_t = float(np.clip(action[0], -1.0, 1.0))
        u_a = float(np.clip(action[1], -1.0, 1.0))

        temp = t_mid + u_t * (t_mid - t_min) if u_t <= 0 else t_mid + u_t * (t_max - t_mid)
        temp = float(np.clip(temp, t_min, t_max))

        k = k_mid + u_a * (k_mid - k_min) if u_a <= 0 else k_mid + u_a * (k_max - k_mid)
        k = float(np.clip(k, k_min, k_max))
        alpha = float(1.0 - (10.0 ** (-k)))

    return temp, alpha


def infer_sac_hyperparams(
    approach: ApproachSpec,
    raw_features: np.ndarray,
    models_root: Path,
    cfg: Dict[str, Any],
) -> Tuple[float, float]:
    """
    @brief Predicts `(temperature, alpha)` deterministically using a trained SAC model and `VecNormalize` stats.
    @param approach Evaluation approach specification.
    @param raw_features Raw 16-feature state vector for the target instance.
    @param models_root Root directory containing trained RL models.
    @param cfg Evaluation configuration dictionary.
    @return Tuple[float, float] Predicted `(temperature, alpha)` values.
    """
    model_filename = cfg.get("model_filename", "sac_model.zip")
    model_dir = resolve_model_dir(models_root, approach.folder_candidates, model_filename)
    model_path = resolve_model_file(model_dir, model_filename)
    stats_path = model_dir / "vec_normalize.pkl"

    if not stats_path.exists():
        raise FileNotFoundError(f"Arquivo de normalização não encontrado: {stats_path}")

    with stats_path.open("rb") as f:
        vec_norm = pickle.load(f)

    vec_norm.training = False
    obs_normalized = vec_norm.normalize_obs(raw_features.reshape(1, -1))

    model = SAC.load(str(model_path))
    action, _ = model.predict(obs_normalized, deterministic=True)
    return map_rl_action_to_hyperparams(action[0], approach.mode, cfg)


def evaluate_instance(
    runner: mmkp_sim.PyMMKPRunner,
    devices_count: int,
    ec_count: int,
    cc_count: int,
    cfg: Dict[str, Any],
    episodes: Optional[int] = None,
) -> None:
    """
    @brief Evaluates all baseline and SAC approaches on a single instance and writes checkpoint results.
    @param runner Initialized `PyMMKPRunner` instance.
    @param devices_count Number of IoT devices in the instance.
    @param ec_count Number of Edge Computing servers.
    @param cc_count Number of Cloud Computing servers.
    @param cfg Evaluation configuration dictionary.
    @param episodes Optional checkpoint episode count override.
    """
    project_root = Path(__file__).resolve().parent.parent
    models_root = project_root / "rl" / "models"
    num_runs = cfg["num_runs"]

    # Resolve a quantidade de episódios do checkpoint para nomear o arquivo de saída
    checkpoint_ep = detect_checkpoint_episodes(models_root, cfg, explicit_episodes=episodes)
    raw_meta_file = project_root / "results" / "METAHEURISTIC" / "metaheuristics.txt"
    target_meta_file = project_root / "results" / "METAHEURISTIC" / f"tsa_{checkpoint_ep}.txt"

    dev_path = project_root / "data" / "devices" / f"Devices_{devices_count}.txt"
    srv_path = project_root / "data" / "servers" / f"Servers_{ec_count}_{cc_count}.txt"
    snap_cache_path = (
        project_root
        / "results"
        / "HEURISTIC"
        / "cache"
        / f"Snapshots_D{devices_count}_EC{ec_count}_CC{cc_count}_R{num_runs}.bin"
    )
    heur_file_path = project_root / "results" / "HEURISTIC" / "heuristics.txt"

    dev_status = "Existente" if dev_path.exists() else "Será criado automaticamente"
    srv_status = "Existente" if srv_path.exists() else "Será criado automaticamente"
    heur_cached = snap_cache_path.exists() and heur_file_path.exists()
    heur_status = (
        "Carregadas do cache (soluções originais fixas, sem reexecutar)"
        if heur_cached
        else "Geradas agora (1ª execução desta instância) e salvas no cache"
    )

    print("\n" + "=" * 85)
    print(f" CENÁRIO: Devices_{devices_count}.txt ({dev_status}) | Servers_{ec_count}_{cc_count}.txt ({srv_status})")
    print(f" HEURÍSTICAS INICIAIS (1x GCB | {num_runs}x BGR): {heur_status}")
    print(f" CHECKPOINT AVALIADO: {checkpoint_ep} episódios -> Saída: {target_meta_file.relative_to(project_root)}")
    print("=" * 85)

    # 1. Carrega (ou cria) a instância em data/devices/ e data/servers/,
    #    executa 1x GCB + 30x BGR (salvando em results/HEURISTIC/heuristics.txt)
    #    e retorna as 16 features do estado RL.
    raw_features_list = runner.prepare_evaluation_scenario(
        devices_count=devices_count,
        services=cfg["services"],
        ec=ec_count,
        cc=cc_count,
        fixed_penalty=cfg["fixed_penalty"],
        num_runs=num_runs,
    )
    raw_features = np.array(raw_features_list, dtype=np.float32)

    # Garante que o arquivo temporário padrão do C++ esteja limpo antes de rodar as abordagens
    if raw_meta_file.exists():
        raw_meta_file.unlink()

    summary_table: List[Tuple[str, str, str, float, float, float, float]] = []

    # 2. Executa as 8 abordagens (30 runs do TSA cada) reutilizando as soluções iniciais em cache
    for approach in APPROACHES:
        if not approach.is_rl:
            temp = float(cfg["baseline_temp"])
            alpha = float(cfg["baseline_alpha"])
            model_type_str = cfg["model_type_labels"]["NONE"]
        else:
            temp, alpha = infer_sac_hyperparams(
                approach=approach,
                raw_features=raw_features,
                models_root=models_root,
                cfg=cfg,
            )
            model_type_str = cfg["model_type_labels"][approach.mode]

        heur_name = "GCB" if approach.option == 5 else "BGR"
        print(
            f"-> [{approach.name:<24}] ({num_runs} runs) | Heur={heur_name} "
            f"| T={temp:8.4f} | Alpha={alpha:.6f} | Model_Type={model_type_str}"
        )

        # Executa as 30 runs do TSA no C++ e redireciona a saída para results/METAHEURISTIC/tsa_<episodes>.txt
        avg_cost, max_cost, avg_time = runner.step_tsa(
            option=approach.option,
            temperature=temp,
            alpha=alpha,
            num_runs=num_runs,
            model_type=model_type_str,
        )
        save_to_checkpoint_file(raw_meta_file, target_meta_file)

        summary_table.append(
            (approach.name, heur_name, model_type_str, temp, alpha, avg_cost, avg_time)
        )

    # 3. Exibe o resumo comparativo da instância no terminal
    print("\n" + "-" * 95)
    print(
        f"{'Abordagem':<26} | {'Heur':<4} | {'Model_Type':<10} | "
        f"{'Temp (T)':>10} | {'Alpha':>10} | {'Custo Médio':>14} | {'Tempo (s)':>9}"
    )
    print("-" * 95)
    for name, heur, m_type, t_val, a_val, c_val, time_val in summary_table:
        print(
            f"{name:<26} | {heur:<4} | {m_type:<10} | "
            f"{t_val:>10.4f} | {a_val:>10.6f} | {c_val:>14.4f} | {time_val:>9.4f}"
        )
    print("-" * 95)
    print(f"Heurísticas salvas em:     {project_root / 'results/HEURISTIC/heuristics.txt'}")
    print(f"Meta-heurísticas salvas em: {target_meta_file}")


def parse_cli_args() -> argparse.Namespace:
    """
    @brief Parses command-line arguments for `evaluate.py`.
    @return argparse.Namespace Parsed CLI arguments.
    """
    parser = argparse.ArgumentParser(
        description="Avalia as 2 Baselines e os 6 Modelos SAC salvando em METAHEURISTIC/tsa_<episodes>.txt."
    )
    parser.add_argument(
        "--episodes",
        type=int,
        default=EVAL_CONFIG["checkpoint_episodes"],
        help="Quantidade de episódios de treino do checkpoint (ex: 100 gera tsa_100.txt). "
             "Se omitido, extrai automaticamente do modelo carregado.",
    )
    parser.add_argument(
        "--model-file",
        type=str,
        default=EVAL_CONFIG["model_filename"],
        help="Nome do arquivo .zip do modelo dentro da pasta do agente (padrão: sac_model.zip)",
    )
    parser.add_argument(
        "--devices",
        type=int,
        nargs="+",
        default=EVAL_CONFIG["default_devices_list"],
        help="Quantidade(s) xxxxx de dispositivos para carregar/criar em data/devices/Devices_xxxxx.txt",
    )
    parser.add_argument(
        "--ec",
        type=int,
        default=EVAL_CONFIG["default_ec"],
        help="Quantidade yyy de servidores Edge (EC) para carregar/criar em data/servers/Servers_yyy_zzz.txt",
    )
    parser.add_argument(
        "--cc",
        type=int,
        default=EVAL_CONFIG["default_cc"],
        help="Quantidade zzz de servidores Cloud (CC) para carregar/criar em data/servers/Servers_yyy_zzz.txt",
    )
    parser.add_argument(
        "--devices-file",
        type=str,
        default=None,
        help="Opcional: caminho para Devices_xxxxx.txt (extrai xxxxx automaticamente do nome)",
    )
    parser.add_argument(
        "--servers-file",
        type=str,
        default=None,
        help="Opcional: caminho para Servers_yyy_zzz.txt (extrai yyy e zzz automaticamente do nome)",
    )
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_cli_args()
    runner = mmkp_sim.PyMMKPRunner()

    EVAL_CONFIG["model_filename"] = args.model_file

    ec_val = args.ec
    cc_val = args.cc
    devices_list = list(args.devices)

    if args.servers_file:
        m_srv = re.search(r"Servers_(\d+)_(\d+)", args.servers_file)
        if not m_srv:
            raise ValueError("O nome do arquivo --servers-file deve seguir o padrão Servers_yyy_zzz.txt")
        ec_val, cc_val = int(m_srv.group(1)), int(m_srv.group(2))

    if args.devices_file:
        m_dev = re.search(r"Devices_(\d+)", args.devices_file)
        if not m_dev:
            raise ValueError("O nome do arquivo --devices-file deve seguir o padrão Devices_xxxxx.txt")
        devices_list = [int(m_dev.group(1))]

    for dev_count in devices_list:
        evaluate_instance(
            runner=runner,
            devices_count=dev_count,
            ec_count=ec_val,
            cc_count=cc_val,
            cfg=EVAL_CONFIG,
            episodes=args.episodes,
        )