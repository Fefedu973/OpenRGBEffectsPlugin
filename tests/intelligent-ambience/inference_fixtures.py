"""Tiny ONNX protobuf fixtures constructed locally without ONNX/PyTorch installs.

Every constant is mathematical test data, never a downloaded/learned weight.
Field numbers follow the official ONNX ModelProto/GraphProto/TensorProto schema.
The real ORT parser and Session::Run validate these files in inference_tests.cpp.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

def var(value):
    out = bytearray()
    while value > 127:
        out.append((value & 127) | 128)
        value >>= 7
    out.append(value)
    return bytes(out)

def integer(field, value): return var(field << 3) + var(value)
def blob(field, value):
    if isinstance(value, str): value = value.encode()
    return var((field << 3) | 2) + var(len(value)) + value
def dims(shape): return b"".join(blob(1, integer(1, d)) for d in shape)
def info(name, shape): return blob(1, name) + blob(2, blob(1, integer(1, 1) + blob(2, dims(shape))))
def tensor(name, shape, data, kind=1):
    raw = struct.pack("<" + ("f" if kind == 1 else "q") * len(data), *data)
    return b"".join(integer(1, d) for d in shape) + integer(2, kind) + blob(8, name) + blob(9, raw)
def attr_int(name, value): return blob(1, name) + integer(3, value) + integer(20, 2)
def attr_ints(name, values): return blob(1, name) + b"".join(integer(8, v) for v in values) + integer(20, 7)
def node(op, inputs, outputs, attributes=b"", domain=""):
    return b"".join(blob(1, s) for s in inputs) + b"".join(blob(2, s) for s in outputs) + blob(4, op) + attributes + (blob(7, domain) if domain else b"")
def model(nodes, inputs, outputs, initializers):
    graph = b"".join(blob(1, n) for n in nodes) + blob(2, "OpenRGB synthetic technical test")
    graph += b"".join(blob(5, t) for t in initializers)
    graph += b"".join(blob(11, info(*t)) for t in inputs) + b"".join(blob(12, info(*t)) for t in outputs)
    return integer(1, 8) + blob(2, "OpenRGB synthetic fixture; no learned parameters") + blob(7, graph) + blob(8, integer(2, 13))

VIDEO = [("current_rgb", [1, 3, 2, 2]), ("previous_rgb", [1, 3, 2, 2]), ("screen_rect", [1, 4]), ("delta_seconds", [1])]
OUTPUT = [("field_rgb", [1, 3, 2, 2]), ("confidence", [1, 1, 2, 2])]
def package(root, name, data, inputs=VIDEO, outputs=OUTPUT, states=(), age=500, task="video-field-v1", **changes):
    folder = root / name
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "model.onnx").write_bytes(data)
    manifest = dict(schema_version=1, id=name.replace("-", "_"), task=task, model="model.onnx", sha256=hashlib.sha256(data).hexdigest(),
                    inputs=[dict(name=n, shape=s) for n, s in inputs], outputs=[dict(name=n, shape=s) for n, s in outputs],
                    states=list(states), field_rect=[-.5, -.3, 2, 1.2] if task.startswith("video") else [0, 0, 1, 1], max_age_ms=age)
    if task.startswith("audio"): manifest["sample_rate"] = 48000
    manifest.update(changes)
    (folder / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")

def generate(root):
    root = Path(root)
    constants = [tensor("quarter", [1], [.25]), tensor("increment", [1], [.1]), tensor("ones", [1, 1, 2, 2], [1]*4)]
    nodes = [node("Add", ["current_rgb", "previous_rgb"], ["sum"]), node("Mul", ["sum", "quarter"], ["base"]),
             node("Add", ["base", "state_in"], ["field_rgb"]), node("Mul", ["delta_seconds", "increment"], ["delta"]),
             node("Add", ["state_in", "delta"], ["state_out"]), node("Identity", ["ones"], ["confidence"])]
    state = [dict(input="state_in", output="state_out", shape=[1])]
    graph = model(nodes, VIDEO + [("state_in", [1])], OUTPUT + [("state_out", [1])], constants)
    package(root, "video", graph, states=state)
    package(root, "bad_hash", graph, states=state, sha256="0"*64)
    package(root, "bad_shape", graph, states=state, outputs=[("field_rgb", [1,3,3,2]), ("confidence", [1,1,3,2])])
    bad = model([node("Identity", ["nan"], ["field_rgb"]), node("Identity", ["ones"], ["confidence"])], VIDEO, OUTPUT,
                [tensor("nan", [1,3,2,2], [float("nan")]*12), constants[-1]])
    package(root, "nan_output", bad)
    external = tensor("external", [1], [0]) + blob(13, blob(1, "location") + blob(2, "outside.bin")) + integer(14, 1)
    package(root, "external", model(nodes, VIDEO, OUTPUT, [external]))
    package(root, "custom", model([node("Identity", ["current_rgb"], ["field_rgb"], domain="untrusted")], VIDEO, OUTPUT, []))
    package(root, "truncated", graph[:-3], states=state)
    pcm = [("audio_pcm", [1,1,480])]
    audio = model([node("Abs", ["audio_pcm"], ["positive"]), node("ReduceMean", ["positive"], ["mean"], blob(5, attr_ints("axes", [2]))),
                   node("Expand", ["mean", "field_shape"], ["field_rgb"])], pcm, OUTPUT[:1], [tensor("field_shape", [4], [1,3,2,2], 7)])
    package(root, "audio", audio, pcm, OUTPUT[:1], task="audio-field-v1")
    # A bounded, intentionally expensive CPU chain makes cancellation observable.
    # Input-dependent, so ORT cannot fold it to a constant during model loading.
    size = 768
    weight = [1/size] * (size*size)
    slow_nodes = [node("ReduceMean", ["current_rgb"], ["mean"], blob(5, attr_int("keepdims", 0))), node("Expand", ["mean", "matrix_shape"], ["m0"])]
    for i in range(24): slow_nodes.append(node("MatMul", [f"m{i}", "matrix"], [f"m{i+1}"]))
    slow_nodes += [node("ReduceMean", ["m24"], ["out_mean"], blob(5, attr_int("keepdims", 0))), node("Expand", ["out_mean", "field_shape"], ["field_rgb"])]
    slow = model(slow_nodes, VIDEO, OUTPUT[:1], [tensor("matrix_shape", [2], [size,size], 7), tensor("field_shape", [4], [1,3,2,2], 7), tensor("matrix", [size,size], weight)])
    package(root, "slow", slow, outputs=OUTPUT[:1], age=2000)
    package(root, "stale", slow, outputs=OUTPUT[:1], age=20)
    return root

if __name__ == "__main__":
    parser = argparse.ArgumentParser(); parser.add_argument("output", type=Path)
    print(generate(parser.parse_args().output))
