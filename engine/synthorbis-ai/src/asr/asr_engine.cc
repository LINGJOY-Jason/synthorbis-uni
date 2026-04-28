/**
 * @file asr_engine.cc
 * @brief ASR 引擎工厂实现
 */

#include "synthorbis/ai/asr_engine.h"
#include "synthorbis/ai/onnx_engine.h"

namespace synthorbis {
namespace ai {

std::unique_ptr<IAsrEngine> create_asr_engine(AsrEngineType type) {
    switch (type) {
        case AsrEngineType::Local:
            return std::make_unique<OnnxAsrEngine>();
        case AsrEngineType::Cloud:
            // TODO: 实现云端引擎
            return nullptr;
        default:
            return nullptr;
    }
}

} // namespace ai
} // namespace synthorbis
