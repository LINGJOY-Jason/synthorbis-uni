/**
 * @file ctc_decoder.cc
 * @brief CTC 解码器实现（骨架，后续实现 Beam Search）
 *
 * 当前版本：贪婪解码（Greedy Decode）由 OnnxAsrEngine 内联实现。
 * 本文件预留给独立的 CTC 解码器模块（含 Beam Search + LM 融合）。
 */

#include "synthorbis/ai/asr_engine.h"

namespace synthorbis {
namespace ai {

// 预留：独立 CTC Beam Search 解码器将在此实现
// TODO: class CtcBeamDecoder

} // namespace ai
} // namespace synthorbis
