"""Loads mechanical/params.json and derives the values the generator needs."""
import json
from pathlib import Path

PARAMS_PATH = Path(__file__).resolve().parent.parent / "params.json"


def load(path=PARAMS_PATH):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def thickness(params, override=None):
    t = override if override is not None else params["body"]["thickness_default"]
    if t not in params["body"]["thickness_options"]:
        raise ValueError(f"thickness {t} is not one of {params['body']['thickness_options']}")
    return t


def seam_z(params, t):
    """Depth of the seam between the front and the rear shell (a prototype assumption)."""
    if params["prototype_assumptions"]["seam_at"] != "mid-thickness":
        raise ValueError("only a mid-thickness seam is implemented")
    return t / 2.0
