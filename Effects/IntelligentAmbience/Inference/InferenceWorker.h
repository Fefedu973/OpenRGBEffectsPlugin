// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace room_ai::inference
{
using Json = nlohmann::json;
struct TensorSpec { std::string name; std::vector<std::int64_t> shape; };
struct StateSpec { std::string input, output; std::vector<std::int64_t> shape; };
struct Tensor : TensorSpec { std::vector<float> values; };
struct ModelConfig
{
    Json manifest;
    std::string id, task;
    std::vector<TensorSpec> inputs, outputs;
    std::vector<StateSpec> states;
    std::array<double,4> field_rect{0,0,1,1};
    int max_age_ms=250;
};
struct Request
{
    std::uint64_t generation=0, epoch=0, sequence=0;
    double source_time=0; // Same steady_clock seconds as Now(), never wall time.
    bool reset=false;
    std::vector<Tensor> inputs; // State tensors belong exclusively to the worker.
};
struct Result
{
    std::uint64_t generation=0, epoch=0, sequence=0;
    double source_time=0, completed_time=0, expires=0;
    std::vector<Tensor> outputs; // field_rgb and optional confidence; no hidden state.
    bool Usable(double now) const;
};
struct StatusSnapshot
{
    std::string state="stopped", detail, runtime_version;
    std::uint64_t generation=0, completed=0, rejected=0, replaced=0, stale=0;
    double last_run_ms=0;
};
double Now();
// Reads only the bounded local manifest and validates version-1 tensor semantics.
// Does not load a runtime, model, capture device or session. Throws on invalid input.
std::shared_ptr<const ModelConfig> ReadManifest(const std::string& manifest_path);
class Worker
{
public:
    Worker();
    ~Worker(); // Requests cooperative cancellation, then joins before unloading ORT.
    Worker(const Worker&)=delete;
    Worker& operator=(const Worker&)=delete;
    std::uint64_t Configure(const std::string& runtime_dll,const std::string& manifest_path);
    bool Submit(Request request);
    // Bounded, non-Qt callback runs on the inference worker, never the render/UI
    // thread. Own every captured input; returning nullopt means not ready yet.
    bool SubmitPrepared(std::uint64_t generation,std::function<std::optional<Request>(const ModelConfig&)> prepare);
    std::shared_ptr<const ModelConfig> Config() const;
    std::shared_ptr<const Result> Latest() const;
    StatusSnapshot Status() const;
    void RequestStop(); // Nonblocking: invalidate results and request ORT cancellation.
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
