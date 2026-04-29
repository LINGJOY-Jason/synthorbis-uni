/**
 * @file asr_engine.cc
 * @brief ASR 引擎工厂实现
 */

#include "synthorbis/ai/asr_engine.h"
#include "synthorbis/ai/onnx_engine.h"
#include "synthorbis/ai/cloud_asr_engine.h"

namespace synthorbis {
namespace ai {

std::unique_ptr<IAsrEngine> create_asr_engine(AsrEngineType type) {
    switch (type) {
        case AsrEngineType::Local:
            return std::make_unique<OnnxAsrEngine>();
        case AsrEngineType::Cloud:
            return std::make_unique<CloudAsrEngine>();
        default:
            return nullptr;
    }
}

} // namespace ai
} // namespace synthorbis
