"""Mathematical ONNX red/green output for the real DLL/GPU smoke test.

No learned weights. Reuses the standalone official-schema protobuf test writer.
Inputs are present and consumed, then a zero-scaled scalar is added to a field.
"""
# SPDX-License-Identifier: GPL-2.0-or-later
from pathlib import Path
from inference_fixtures import VIDEO, OUTPUT, tensor, model, node, package, attr_int, blob


def generate_ui_fixtures(root):
    root = Path(root)
    for name, task, inputs, channel in (
        ("ui_video_red", "video-field-v1", VIDEO, 0),
        ("ui_audio_green", "audio-field-v1", [("audio_pcm", [1, 1, 480])], 1),
    ):
        nodes = []
        for i, (key, _) in enumerate(inputs):
            nodes.append(node("ReduceMean", [key], [f"mean{i}"], blob(5, attr_int("keepdims", 0))))
        total = "mean0"
        for i in range(1, len(inputs)):
            nodes.append(node("Add", [total, f"mean{i}"], [f"sum{i}"]))
            total = f"sum{i}"
        nodes += [node("Mul", [total, "zero"], ["offset"]),
                  node("Add", ["color", "offset"], ["field_rgb"]),
                  node("Identity", ["ones"], ["confidence"])]
        rgb = [1.0 if i // 4 == channel else 0.0 for i in range(12)]
        graph = model(nodes, inputs, OUTPUT, [tensor("color", [1, 3, 2, 2], rgb),
                      tensor("zero", [], [0]), tensor("ones", [1, 1, 2, 2], [1] * 4)])
        package(root, name, graph, inputs, OUTPUT, task=task, field_rect=[-.5, -.5, 2, 1.5], age=500)
