/**
 * @file test_asr.cc
 * @brief ASR 模块测试
 */

#include <gtest/gtest.h>
#include "synthorbis/ai/asr_engine.h"
#include <vector>

using namespace synthorbis::ai;

class AsrEngineTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(AsrEngineTest, CreateLocalEngine) {
    auto engine = create_asr_engine(AsrEngineType::Local);
    EXPECT_NE(engine, nullptr);
    EXPECT_EQ(engine->get_type(), AsrEngineType::Local);
    EXPECT_EQ(engine->get_name(), "OnnxAsrEngine");
}

TEST_F(AsrEngineTest, AudioDataFormat) {
    std::vector<float> samples(16000, 0.0f);  // 1秒音频
    AudioData audio;
    audio.data = samples.data();
    audio.sample_rate = 16000;
    audio.samples = 16000;
    
    EXPECT_EQ(audio.sample_rate, 16000);
    EXPECT_EQ(audio.samples, 16000);
}

TEST_F(AsrEngineTest, AsrConfig) {
    AsrConfig config;
    config.type = AsrEngineType::Local;
    config.model_path = "/path/to/model.onnx";
    config.tokens_path = "/path/to/tokens.txt";
    config.num_threads = 4;
    config.language = 0;
    config.text_normalization = true;
    
    EXPECT_EQ(config.type, AsrEngineType::Local);
    EXPECT_EQ(config.num_threads, 4);
    EXPECT_TRUE(config.text_normalization);
}

TEST_F(AsrEngineTest, AsrResult) {
    AsrResult result;
    result.text = "测试文本";
    result.confidence = 0.95f;
    result.process_time = 0.5;
    
    EXPECT_EQ(result.text, "测试文本");
    EXPECT_FLOAT_EQ(result.confidence, 0.95f);
    EXPECT_DOUBLE_EQ(result.process_time, 0.5);
}

#ifdef SYNTHORBIS_AI_HAS_ONNXRUNTIME
TEST_F(AsrEngineTest, OnnxEngineInitialize) {
    auto engine = create_asr_engine(AsrEngineType::Local);
    ASSERT_NE(engine, nullptr);
    
    AsrConfig config;
    config.model_path = "/nonexistent/model.onnx";  // 不存在的路径
    config.tokens_path = "";
    config.num_threads = 2;
    
    // 预期失败（文件不存在）
    // EXPECT_EQ(engine->initialize(config), 0);
}
#endif
