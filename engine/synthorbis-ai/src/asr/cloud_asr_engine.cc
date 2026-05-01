/**
 * @file cloud_asr_engine.cc
 * @brief 云端 ASR 引擎实现
 *
 * 实现三个云端 ASR 服务的 REST 封装：
 *   - 智谱 GLM-ASR：multipart/form-data + WAV 上传
 *   - 火山引擎豆包 ASR：JSON + PCM base64
 *   - 阿里云 NLS 一句话识别：application/octet-stream + WAV 二进制 + URL 参数
 *
 * HTTP 实现：
 *   - 若编译时 SYNTHORBIS_AI_HAS_CURL=1，使用 libcurl
 *   - 否则退化为 stub（返回错误）
 */

#include "synthorbis/ai/cloud_asr_engine.h"

#include <chrono>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cmath>

// libcurl 可选依赖
#if defined(SYNTHORBIS_AI_HAS_CURL) && SYNTHORBIS_AI_HAS_CURL
#  include <curl/curl.h>
#  define HAS_CURL 1
#else
#  define HAS_CURL 0
#endif

namespace synthorbis {
namespace ai {

// ============================================================
// 工具：WAV 封装
// ============================================================

/**
 * WAV header 结构（44 字节 PCM WAV）
 * 参考: http://soundfile.sapp.org/doc/WaveFormat/
 */
static void write_le16(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}
static void write_le32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

std::vector<uint8_t> CloudAsrEngine::pcm_to_wav(const AudioData& audio) {
    // PCM float [-1, 1] → int16
    std::vector<int16_t> pcm16(audio.samples);
    for (int i = 0; i < audio.samples; ++i) {
        float s = audio.data[i];
        // clamp
        if (s > 1.0f)  s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        pcm16[i] = static_cast<int16_t>(s * 32767.0f);
    }

    const uint32_t num_channels  = 1;
    const uint32_t sample_rate   = static_cast<uint32_t>(audio.sample_rate);
    const uint32_t bits_per_sample = 16;
    const uint32_t byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    const uint16_t block_align = static_cast<uint16_t>(num_channels * bits_per_sample / 8);
    const uint32_t data_size   = static_cast<uint32_t>(audio.samples * 2); // int16 = 2 bytes
    const uint32_t riff_size   = 36 + data_size;

    std::vector<uint8_t> wav;
    wav.reserve(44 + data_size);

    // RIFF chunk
    wav.insert(wav.end(), {'R','I','F','F'});
    write_le32(wav, riff_size);
    wav.insert(wav.end(), {'W','A','V','E'});

    // fmt chunk
    wav.insert(wav.end(), {'f','m','t',' '});
    write_le32(wav, 16);                     // chunk size
    write_le16(wav, 1);                      // PCM format
    write_le16(wav, static_cast<uint16_t>(num_channels));
    write_le32(wav, sample_rate);
    write_le32(wav, byte_rate);
    write_le16(wav, block_align);
    write_le16(wav, static_cast<uint16_t>(bits_per_sample));

    // data chunk
    wav.insert(wav.end(), {'d','a','t','a'});
    write_le32(wav, data_size);

    // PCM data
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(pcm16.data());
    wav.insert(wav.end(), raw, raw + data_size);

    return wav;
}

std::vector<uint8_t> CloudAsrEngine::pcm_to_raw_int16(const AudioData& audio) {
    std::vector<uint8_t> out(audio.samples * 2);
    for (int i = 0; i < audio.samples; ++i) {
        float s = audio.data[i];
        if (s > 1.0f)  s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        int16_t v = static_cast<int16_t>(s * 32767.0f);
        out[i * 2]     = static_cast<uint8_t>(v & 0xFF);
        out[i * 2 + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    }
    return out;
}

// ============================================================
// 工具：Base64 编码
// ============================================================

static const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string CloudAsrEngine::base64_encode(const std::vector<uint8_t>& data) {
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    size_t i = 0;
    for (; i + 2 < data.size(); i += 3) {
        uint32_t triple = (static_cast<uint32_t>(data[i]) << 16) |
                          (static_cast<uint32_t>(data[i+1]) << 8)  |
                          (static_cast<uint32_t>(data[i+2]));
        result += kBase64Chars[(triple >> 18) & 0x3F];
        result += kBase64Chars[(triple >> 12) & 0x3F];
        result += kBase64Chars[(triple >>  6) & 0x3F];
        result += kBase64Chars[(triple      ) & 0x3F];
    }
    if (i + 1 == data.size()) {
        uint32_t d = static_cast<uint32_t>(data[i]) << 16;
        result += kBase64Chars[(d >> 18) & 0x3F];
        result += kBase64Chars[(d >> 12) & 0x3F];
        result += '=';
        result += '=';
    } else if (i + 2 == data.size()) {
        uint32_t d = (static_cast<uint32_t>(data[i]) << 16) |
                     (static_cast<uint32_t>(data[i+1]) << 8);
        result += kBase64Chars[(d >> 18) & 0x3F];
        result += kBase64Chars[(d >> 12) & 0x3F];
        result += kBase64Chars[(d >>  6) & 0x3F];
        result += '=';
    }
    return result;
}

// ============================================================
// 工具：极简 JSON 字符串字段提取
// ============================================================

std::string CloudAsrEngine::extract_json_string(const std::string& json,
                                                  const std::string& key) {
    // 查找 "key": "value"
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return "";

    // 跳过空白
    while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) ++pos;

    if (pos >= json.size()) return "";

    if (json[pos] == '"') {
        // 字符串值
        ++pos;
        std::string val;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                char esc = json[pos + 1];
                switch (esc) {
                    case '"':  val += '"';  break;
                    case '\\': val += '\\'; break;
                    case 'n':  val += '\n'; break;
                    case 'r':  val += '\r'; break;
                    case 't':  val += '\t'; break;
                    default:   val += esc;  break;
                }
                pos += 2;
            } else {
                val += json[pos++];
            }
        }
        return val;
    } else {
        // 数值 / 布尔
        auto end_pos = json.find_first_of(",}\n", pos);
        if (end_pos == std::string::npos) end_pos = json.size();
        std::string val = json.substr(pos, end_pos - pos);
        // trim
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
        return val;
    }
}

// ============================================================
// libcurl HTTP 客户端实现
// ============================================================

#if HAS_CURL

namespace {

// libcurl 写回调
static size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* str = reinterpret_cast<std::string*>(userdata);
    str->append(ptr, size * nmemb);
    return size * nmemb;
}

/**
 * @brief libcurl HTTP 客户端实现
 */
class CurlHttpClient : public IHttpClient {
public:
    CurlHttpClient() {
        curl_global_init(CURL_GLOBAL_ALL);
    }
    ~CurlHttpClient() override {
        curl_global_cleanup();
    }

    HttpResponse post_multipart(
        const std::string& url,
        const std::vector<std::string>& headers,
        const std::vector<std::pair<std::string, std::string>>& fields,
        const std::string& file_field,
        const std::vector<uint8_t>& file_data,
        const std::string& filename,
        int timeout_ms) override
    {
        HttpResponse resp;
        CURL* curl = curl_easy_init();
        if (!curl) {
            resp.error_msg = "curl_easy_init() failed";
            return resp;
        }

        auto t0 = std::chrono::steady_clock::now();

        // Headers
        struct curl_slist* hdr = nullptr;
        for (const auto& h : headers) {
            hdr = curl_slist_append(hdr, h.c_str());
        }

        // Multipart form
        curl_mime* mime = curl_mime_init(curl);

        // 普通字段
        for (const auto& kv : fields) {
            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, kv.first.c_str());
            curl_mime_data(part, kv.second.c_str(), CURL_ZERO_TERMINATED);
        }

        // 文件字段
        if (!file_field.empty() && !file_data.empty()) {
            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, file_field.c_str());
            curl_mime_data(part, reinterpret_cast<const char*>(file_data.data()),
                           file_data.size());
            curl_mime_filename(part, filename.c_str());
            curl_mime_type(part, "audio/wav");
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeout_ms);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            resp.error_msg = curl_easy_strerror(res);
        } else {
            long code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
            resp.status_code = static_cast<int>(code);
        }

        auto t1 = std::chrono::steady_clock::now();
        resp.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        curl_mime_free(mime);
        curl_slist_free_all(hdr);
        curl_easy_cleanup(curl);

        return resp;
    }

    HttpResponse post_json(
        const std::string& url,
        const std::vector<std::string>& headers,
        const std::string& json_body,
        int timeout_ms) override
    {
        HttpResponse resp;
        CURL* curl = curl_easy_init();
        if (!curl) {
            resp.error_msg = "curl_easy_init() failed";
            return resp;
        }

        auto t0 = std::chrono::steady_clock::now();

        struct curl_slist* hdr = nullptr;
        hdr = curl_slist_append(hdr, "Content-Type: application/json");
        for (const auto& h : headers) {
            hdr = curl_slist_append(hdr, h.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)json_body.size());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeout_ms);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            resp.error_msg = curl_easy_strerror(res);
        } else {
            long code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
            resp.status_code = static_cast<int>(code);
        }

        auto t1 = std::chrono::steady_clock::now();
        resp.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        curl_slist_free_all(hdr);
        curl_easy_cleanup(curl);

        return resp;
    }

    HttpResponse post_binary(
        const std::string& url,
        const std::vector<std::string>& headers,
        const std::vector<uint8_t>& data,
        int timeout_ms) override
    {
        HttpResponse resp;
        CURL* curl = curl_easy_init();
        if (!curl) {
            resp.error_msg = "curl_easy_init() failed";
            return resp;
        }

        auto t0 = std::chrono::steady_clock::now();

        struct curl_slist* hdr = nullptr;
        hdr = curl_slist_append(hdr, "Content-Type: application/octet-stream");
        for (const auto& h : headers) {
            hdr = curl_slist_append(hdr, h.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, reinterpret_cast<const char*>(data.data()));
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)data.size());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeout_ms);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            resp.error_msg = curl_easy_strerror(res);
        } else {
            long code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
            resp.status_code = static_cast<int>(code);
        }

        auto t1b = std::chrono::steady_clock::now();
        resp.elapsed_ms = std::chrono::duration<double, std::milli>(t1b - t0).count();

        curl_slist_free_all(hdr);
        curl_easy_cleanup(curl);

        return resp;
    }
};

} // anonymous namespace

#endif // HAS_CURL

// ============================================================
// Stub HTTP 客户端（无 libcurl 时用于编译通过）
// ============================================================

namespace {

class StubHttpClient : public IHttpClient {
public:
    HttpResponse post_multipart(
        const std::string& /*url*/,
        const std::vector<std::string>& /*headers*/,
        const std::vector<std::pair<std::string, std::string>>& /*fields*/,
        const std::string& /*file_field*/,
        const std::vector<uint8_t>& /*file_data*/,
        const std::string& /*filename*/,
        int /*timeout_ms*/) override
    {
        HttpResponse r;
        r.status_code = 0;
        r.error_msg = "libcurl not available (SYNTHORBIS_AI_HAS_CURL=0)";
        return r;
    }

    HttpResponse post_json(
        const std::string& /*url*/,
        const std::vector<std::string>& /*headers*/,
        const std::string& /*json_body*/,
        int /*timeout_ms*/) override
    {
        HttpResponse r;
        r.status_code = 0;
        r.error_msg = "libcurl not available (SYNTHORBIS_AI_HAS_CURL=0)";
        return r;
    }

    HttpResponse post_binary(
        const std::string& /*url*/,
        const std::vector<std::string>& /*headers*/,
        const std::vector<uint8_t>& /*data*/,
        int /*timeout_ms*/) override
    {
        HttpResponse r;
        r.status_code = 0;
        r.error_msg = "libcurl not available (SYNTHORBIS_AI_HAS_CURL=0)";
        return r;
    }
};

} // anonymous namespace

// ============================================================
// CloudAsrEngine 构造 / 析构
// ============================================================

CloudAsrEngine::CloudAsrEngine() {
#if HAS_CURL
    http_client_ = std::make_shared<CurlHttpClient>();
#else
    http_client_ = std::make_shared<StubHttpClient>();
#endif
}

CloudAsrEngine::CloudAsrEngine(std::shared_ptr<IHttpClient> http_client)
    : http_client_(std::move(http_client))
{}

CloudAsrEngine::~CloudAsrEngine() = default;

// ============================================================
// IAsrEngine 接口实现
// ============================================================

int CloudAsrEngine::initialize(const AsrConfig& config) {
    if (config.type != AsrEngineType::Cloud) {
        return -1;
    }
    base_cfg_ = config;

    // 从 AsrConfig 推断云端配置（兼容现有接口）
    if (!config.api_key.empty()) {
        cloud_cfg_.api_key = config.api_key;
    }
    if (!config.api_endpoint.empty()) {
        cloud_cfg_.endpoint = config.api_endpoint;
        // 根据 endpoint 推断 provider
        if (cloud_cfg_.endpoint.find("bigmodel.cn") != std::string::npos ||
            cloud_cfg_.endpoint.find("zhipuai.cn") != std::string::npos) {
            cloud_cfg_.provider = CloudProvider::ZhipuGLM;
        } else if (cloud_cfg_.endpoint.find("bytedance.com") != std::string::npos ||
                   cloud_cfg_.endpoint.find("volcengine.com") != std::string::npos) {
            cloud_cfg_.provider = CloudProvider::VolcengineDouBao;
        } else if (cloud_cfg_.endpoint.find("aliyuncs.com") != std::string::npos ||
                   cloud_cfg_.endpoint.find("nls-gateway") != std::string::npos) {
            cloud_cfg_.provider = CloudProvider::AliyunNLS;
        }
    }
    // 默认模型
    if (cloud_cfg_.model.empty()) {
        if (cloud_cfg_.provider == CloudProvider::ZhipuGLM) {
            cloud_cfg_.model = "glm-4-voice-flash";
        } else if (cloud_cfg_.provider == CloudProvider::VolcengineDouBao) {
            cloud_cfg_.model = "bigmodel";
        }
        // AliyunNLS 无模型字段
    }

    initialized_ = true;
    return 0;
}

void CloudAsrEngine::set_cloud_config(const CloudAsrConfig& cfg) {
    cloud_cfg_ = cfg;
}

void CloudAsrEngine::set_http_client(std::shared_ptr<IHttpClient> client) {
    http_client_ = std::move(client);
}

void CloudAsrEngine::reset_stats() {
    request_count_.store(0);
    failure_count_.store(0);
}

std::string CloudAsrEngine::get_name() const {
    switch (cloud_cfg_.provider) {
        case CloudProvider::ZhipuGLM:         return "CloudAsrEngine(ZhipuGLM)";
        case CloudProvider::VolcengineDouBao:  return "CloudAsrEngine(DouBao)";
        case CloudProvider::AliyunNLS:         return "CloudAsrEngine(AliyunNLS)";
        case CloudProvider::Custom:            return "CloudAsrEngine(Custom)";
        default:                               return "CloudAsrEngine";
    }
}

AsrResult CloudAsrEngine::recognize(const AudioData& audio) {
    if (!initialized_) {
        AsrResult r;
        r.text = "";
        r.confidence = 0.0f;
        r.process_time = 0.0;
        return r;
    }
    return recognize_with_retry(audio);
}

std::vector<AsrResult> CloudAsrEngine::recognize_batch(
    const std::vector<AudioData>& audios)
{
    std::vector<AsrResult> results;
    results.reserve(audios.size());
    for (const auto& a : audios) {
        results.push_back(recognize(a));
    }
    return results;
}

// ============================================================
// 带重试的请求分发
// ============================================================

AsrResult CloudAsrEngine::recognize_with_retry(const AudioData& audio) {
    int max_attempts = cloud_cfg_.max_retries + 1;
    AsrResult last_result;
    last_result.confidence = 0.0f;

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        auto t0 = std::chrono::high_resolution_clock::now();

        request_count_.fetch_add(1);
        HttpResponse resp;

        switch (cloud_cfg_.provider) {
            case CloudProvider::ZhipuGLM:
                resp = call_zhipu(audio);
                break;
            case CloudProvider::VolcengineDouBao:
                resp = call_doubao(audio);
                break;
            case CloudProvider::AliyunNLS:
                resp = call_aliyun(audio);
                break;
            default:
                resp = call_zhipu(audio);
                break;
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(t1 - t0).count();

        if (!resp.error_msg.empty() || resp.status_code != 200) {
            failure_count_.fetch_add(1);
            last_result.process_time = elapsed;
            // 非超时错误不重试（如鉴权失败 401、格式错误 400）
            if (resp.status_code == 401 || resp.status_code == 400) break;
            continue;
        }

        // 解析响应
        std::string text;
        if (cloud_cfg_.provider == CloudProvider::ZhipuGLM) {
            text = parse_zhipu_response(resp.body);
        } else if (cloud_cfg_.provider == CloudProvider::AliyunNLS) {
            text = parse_aliyun_response(resp.body);
        } else {
            text = parse_doubao_response(resp.body);
        }

        last_result.text = text;
        last_result.process_time = elapsed;
        last_result.confidence = text.empty() ? 0.0f : 0.95f;  // 云端默认高置信度

        if (!text.empty()) break;  // 成功，不重试
    }

    return last_result;
}

// ============================================================
// 智谱 GLM-ASR 实现
// ============================================================

HttpResponse CloudAsrEngine::call_zhipu(const AudioData& audio) {
    // 构建 WAV 文件
    auto wav_data = pcm_to_wav(audio);

    // 确定 endpoint
    std::string url = cloud_cfg_.endpoint.empty()
        ? "https://open.bigmodel.cn/api/paas/v4/audio/transcriptions"
        : cloud_cfg_.endpoint;

    // Headers（仅 Authorization，Content-Type 由 curl multipart 自动设置）
    std::vector<std::string> headers;
    headers.push_back("Authorization: Bearer " + cloud_cfg_.api_key);

    // Form 字段
    std::vector<std::pair<std::string, std::string>> fields;
    std::string model = cloud_cfg_.model.empty() ? "glm-4-voice-flash" : cloud_cfg_.model;
    fields.push_back({"model", model});
    if (!cloud_cfg_.language.empty()) {
        fields.push_back({"language", cloud_cfg_.language});
    }

    return http_client_->post_multipart(
        url, headers, fields,
        "file", wav_data, "audio.wav",
        cloud_cfg_.timeout_ms);
}

std::string CloudAsrEngine::parse_zhipu_response(const std::string& json_body) {
    // 智谱响应格式：
    // {"text":"识别结果","task_id":"...","duration":1.23}
    // 错误：{"error":{"code":"...","message":"..."}}
    if (json_body.empty()) return "";

    // 检查是否有错误字段
    auto err = extract_json_string(json_body, "message");
    if (!err.empty() && json_body.find("\"error\"") != std::string::npos) {
        return "";  // 返回空，触发重试或降级
    }

    return extract_json_string(json_body, "text");
}

// ============================================================
// 豆包 ASR 实现
// ============================================================

HttpResponse CloudAsrEngine::call_doubao(const AudioData& audio) {
    // 豆包 ASR v1 接口格式（OpenAI-compatible）：
    // POST https://openspeech.bytedance.com/api/v1/asr
    // Body:
    // {
    //   "app": {"appid": "xxx", "token": "ACCESS_TOKEN"},
    //   "user": {"uid": "synthorbis"},
    //   "request": {
    //     "reqid": "unique-id",
    //     "sequence": -1
    //   },
    //   "audio": {
    //     "format": "pcm",
    //     "sample_rate": 16000,
    //     "bits": 16,
    //     "channel": 1,
    //     "codec": "raw",
    //     "data": "<base64>"
    //   }
    // }

    auto raw_pcm = pcm_to_raw_int16(audio);
    std::string b64 = base64_encode(raw_pcm);

    // 构建简单唯一请求 ID
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    std::string reqid = "synthorbis_" + std::to_string(now_us);

    std::string url = cloud_cfg_.endpoint.empty()
        ? "https://openspeech.bytedance.com/api/v1/asr"
        : cloud_cfg_.endpoint;

    // JSON body（手动拼接，避免引入 json 库）
    std::ostringstream oss;
    oss << "{"
        << "\"app\":{\"appid\":\"" << cloud_cfg_.app_id << "\","
        <<          "\"token\":\"" << cloud_cfg_.api_key << "\"},"
        << "\"user\":{\"uid\":\"synthorbis\"},"
        << "\"request\":{\"reqid\":\"" << reqid << "\","
        <<              "\"sequence\":-1},"
        << "\"audio\":{"
        <<     "\"format\":\"pcm\","
        <<     "\"sample_rate\":" << cloud_cfg_.sample_rate << ","
        <<     "\"bits\":" << cloud_cfg_.bits << ","
        <<     "\"channel\":" << cloud_cfg_.channels << ","
        <<     "\"codec\":\"raw\","
        <<     "\"data\":\"" << b64 << "\""
        << "}"
        << "}";

    std::vector<std::string> headers;

    return http_client_->post_json(url, headers, oss.str(), cloud_cfg_.timeout_ms);
}

std::string CloudAsrEngine::parse_doubao_response(const std::string& json_body) {
    // 豆包响应格式：
    // {"code":0,"message":"","id":"...","utterances":[{"text":"...","words":[...]}],"result":{"text":"..."},"duration":5550}
    // 或者 HTTP 错误：{"code":1000X,"message":"..."}
    if (json_body.empty()) return "";

    // 优先取 result.text（有的版本直接在 result 里）
    // 先找 result 对象内的 text
    auto result_pos = json_body.find("\"result\"");
    if (result_pos != std::string::npos) {
        auto sub = json_body.substr(result_pos);
        auto text = extract_json_string(sub, "text");
        if (!text.empty()) return text;
    }

    // 其次取第一个 utterance.text
    auto uttr_pos = json_body.find("\"utterances\"");
    if (uttr_pos != std::string::npos) {
        auto sub = json_body.substr(uttr_pos);
        auto text = extract_json_string(sub, "text");
        if (!text.empty()) return text;
    }

    // 最后直接取顶层 text
    return extract_json_string(json_body, "text");
}

// ============================================================
// 阿里云 NLS 一句话识别实现
// ============================================================

HttpResponse CloudAsrEngine::call_aliyun(const AudioData& audio) {
    // 阿里云 NLS REST API:
    //   POST https://nls-gateway-cn-{region}.aliyuncs.com/stream/v1/asr
    //       ?appkey={appkey}&format=pcm&sample_rate=16000
    //       &enable_punctuation_prediction=true
    //       &enable_inverse_text_normalization=true
    //   Header:
    //     X-NLS-Token: {nls_token}
    //     Content-Type: application/octet-stream
    //   Body: 原始 WAV 二进制（不是 base64）

    // 确定 region 和 endpoint
    std::string region = cloud_cfg_.nls_region.empty() ? "cn-shanghai" : cloud_cfg_.nls_region;

    std::string base_url;
    if (!cloud_cfg_.endpoint.empty()) {
        base_url = cloud_cfg_.endpoint;
    } else {
        base_url = "https://nls-gateway-" + region + ".aliyuncs.com/stream/v1/asr";
    }

    // 构建 URL query 参数
    std::string appkey = cloud_cfg_.nls_appkey.empty() ? cloud_cfg_.app_id : cloud_cfg_.nls_appkey;
    std::string url = base_url
        + "?appkey=" + appkey
        + "&format=pcm"
        + "&sample_rate=" + std::to_string(cloud_cfg_.sample_rate)
        + "&enable_punctuation_prediction=true"
        + "&enable_inverse_text_normalization=true";

    // 鉴权 Header：X-NLS-Token
    std::vector<std::string> headers;
    std::string token = cloud_cfg_.nls_token.empty() ? cloud_cfg_.api_key : cloud_cfg_.nls_token;
    headers.push_back("X-NLS-Token: " + token);

    // 上传 PCM 原始数据（int16，直接 body，不加 WAV header）
    // 注：阿里云 NLS format=pcm 期望的是裸 PCM，但支持 WAV，为保持一致统一传 WAV
    auto wav_data = pcm_to_wav(audio);

    return http_client_->post_binary(url, headers, wav_data, cloud_cfg_.timeout_ms);
}

std::string CloudAsrEngine::parse_aliyun_response(const std::string& json_body) {
    // 阿里云 NLS 成功响应格式：
    //   {"task_id":"cf7b...","result":"北京的天气。","status":20000000,"message":"SUCCESS"}
    //
    // 失败响应格式：
    //   {"task_id":"8bae...","result":"","status":40000001,"message":"Gateway:ACCESS_DENIED:..."}

    if (json_body.empty()) return "";

    // 检查状态码（20000000 = 成功）
    auto status_str = extract_json_string(json_body, "status");
    if (!status_str.empty() && status_str != "20000000") {
        // 记录错误消息但返回空，让调用方触发重试
        return "";
    }

    // 取 result 字段
    return extract_json_string(json_body, "result");
}

// ============================================================
// HybridAsrEngine 实现
// ============================================================

HybridAsrEngine::HybridAsrEngine(std::unique_ptr<IAsrEngine> local,
                                   std::unique_ptr<IAsrEngine> cloud)
    : local_engine_(std::move(local))
    , cloud_engine_(std::move(cloud))
{}

void HybridAsrEngine::set_local_engine(std::unique_ptr<IAsrEngine> engine) {
    local_engine_ = std::move(engine);
}

void HybridAsrEngine::set_cloud_engine(std::unique_ptr<IAsrEngine> engine) {
    cloud_engine_ = std::move(engine);
}

int HybridAsrEngine::initialize(const AsrConfig& config) {
    // HybridAsrEngine 一般在外部分别 initialize 两个子引擎
    // 这里提供一个方便接口：传入 Local 配置，只初始化端侧
    if (local_engine_) {
        return local_engine_->initialize(config);
    }
    return 0;
}

bool HybridAsrEngine::should_fallback(const AsrResult& local_result) const {
    // 1. 文本为空 → 必须降级
    if (local_result.text.empty()) return true;
    // 2. 置信度过低
    if (local_result.confidence < fallback_threshold_) return true;
    // 3. 耗时异常（超过限制）
    if (local_result.process_time > local_timeout_) return true;
    return false;
}

AsrResult HybridAsrEngine::recognize(const AudioData& audio) {
    AsrResult result;

    // 优先端侧
    if (local_engine_) {
        result = local_engine_->recognize(audio);
        if (!should_fallback(result)) {
            return result;
        }
    }

    // 降级到云端
    if (cloud_engine_) {
        auto cloud_result = cloud_engine_->recognize(audio);
        // 云端结果只有在比端侧好的情况下才采用
        if (!cloud_result.text.empty()) {
            return cloud_result;
        }
    }

    // 两者均失败，返回端侧结果（可能为空）
    return result;
}

std::vector<AsrResult> HybridAsrEngine::recognize_batch(
    const std::vector<AudioData>& audios)
{
    std::vector<AsrResult> results;
    results.reserve(audios.size());
    for (const auto& a : audios) {
        results.push_back(recognize(a));
    }
    return results;
}

} // namespace ai
} // namespace synthorbis
