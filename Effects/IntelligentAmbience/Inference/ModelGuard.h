// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
// Deliberately small ONNX protobuf preflight. ORT remains the authoritative
// graph/type validator. This rejects external data, custom domains/functions,
// sparse tensors and graph-valued attributes before any ORT session is created.
namespace room_ai::inference::guard
{
struct Field {unsigned number=0,wire=0;std::uint64_t integer=0;std::string_view bytes;};
inline void Check(bool ok){if(!ok)throw std::runtime_error("Unsupported or malformed self-contained ONNX graph");}
inline std::uint64_t Var(std::string_view& data)
{std::uint64_t v=0;for(unsigned i=0;i<10;++i){Check(!data.empty());const auto b=static_cast<unsigned char>(data.front());data.remove_prefix(1);Check(i<9||b<=1);v|=std::uint64_t(b&127)<<(7*i);if(!(b&128))return v;}Check(false);return 0;}
inline Field Next(std::string_view& data)
{
    Field f;const auto key=Var(data);f.number=unsigned(key>>3);f.wire=unsigned(key&7);Check(f.number>0&&key>>3<=0x1fffffff);
    if(f.wire==0)f.integer=Var(data);
    else if(f.wire==2){const auto size=Var(data);Check(size<=data.size());f.bytes=data.substr(0,std::size_t(size));data.remove_prefix(std::size_t(size));}
    else if(f.wire==1||f.wire==5){const unsigned size=f.wire==1?8:4;Check(data.size()>=size);f.bytes=data.substr(0,size);data.remove_prefix(size);}
    else Check(false);return f;
}
inline void Tensor(std::string_view bytes)
{while(!bytes.empty()){const auto f=Next(bytes);Check(f.number!=13);if(f.number==14)Check(f.wire==0&&f.integer==0);}}
inline void Attribute(std::string_view bytes)
{while(!bytes.empty()){const auto f=Next(bytes);Check(f.number!=6&&f.number!=11&&f.number!=22&&f.number!=23);if(f.number==5||f.number==10){Check(f.wire==2);Tensor(f.bytes);}}}
inline void Node(std::string_view bytes)
{
    std::string op,domain;while(!bytes.empty()){const auto f=Next(bytes);if(f.number==4)op=std::string(f.bytes);if(f.number==7)domain=std::string(f.bytes);if(f.number==5)Attribute(f.bytes);}
    static const std::set<std::string> allowed={"Abs","Add","AveragePool","Cast","Clip","Concat","Constant","Conv","Div","Einsum","Expand","Flatten","Gather","Gemm","GlobalAveragePool","Identity","LayerNormalization","MatMul","Max","MaxPool","Min","Mul","Neg","Pad","Pow","ReduceMean","ReduceSum","Relu","Reshape","Resize","Shape","Sigmoid","Slice","Softmax","Split","Sqrt","Squeeze","Sub","Tanh","Transpose","Unsqueeze","Where"};
    Check((domain.empty()||domain=="ai.onnx")&&allowed.count(op)>0);
}
inline void Validate(std::string_view model)
{
    unsigned graph_count=0,nodes=0;
    while(!model.empty())
    {
        const auto f=Next(model);Check(f.number!=25);if(f.number!=7)continue;Check(f.wire==2&&++graph_count==1);auto graph=f.bytes;
        while(!graph.empty())
        {const auto g=Next(graph);Check(g.number!=15);if(g.number==1){Check(g.wire==2&&++nodes<=4096);Node(g.bytes);}if(g.number==5){Check(g.wire==2);Tensor(g.bytes);}}
    }
    Check(graph_count==1&&nodes>0);
}
}
