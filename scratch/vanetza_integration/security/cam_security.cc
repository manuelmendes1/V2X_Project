#include "cam_security.h"

#include "vanetza_integration/core/v2x_log.h"

#include <memory>
#include <string>
#include <vector>

namespace {

    //=============================================================================
    // OpenSSL helpers
    //=============================================================================

    struct OpenSSLDeleter {
        void operator()(EVP_MD_CTX* ctx) const {
            if (ctx){

                EVP_MD_CTX_free(ctx);
            }
        }

        void operator()(EVP_PKEY* key) const {
            if (key) {

                EVP_PKEY_free(key);
            }
        }
    };

    using MdContext = std::unique_ptr<EVP_MD_CTX, OpenSSLDeleter>;
    using PublicKey = std::unique_ptr<EVP_PKEY, OpenSSLDeleter>;

    //=============================================================================
    // Logging helpers
    //=============================================================================

    inline void LogBegin(const ns3::PacketContext& ctx, const std::string& event) {
        V2XWriteLog(ctx, V2XLogLevel::DEBUG, "SECURITY", event);
    }

    inline void LogSuccess(ns3::PacketContext& ctx, const std::string& event, const std::string& message = "") {

        ctx.MarkSuccess();

        V2XWriteLog(ctx, V2XLogLevel::DEBUG, "SECURITY", event, message);
    }

    inline void LogFailure(ns3::PacketContext& ctx, V2XLogLevel level, const std::string& event, const std::string& reason, const std::string& message) {

        ctx.MarkFailure();
        ctx.reason = reason;

        V2XWriteLog(ctx, level, "SECURITY", event, message);
    }

} // anonymous namespace

//=============================================================================
// SIGN
//=============================================================================

std::vector<uint8_t> CamSecurity::Sign(const ns3::PacketContext& ctx, const std::vector<uint8_t>& payload, EVP_PKEY* privateKey) {

    ns3::PacketContext logCtx = ctx;

    logCtx.payload.type = ns3::PayloadType::CAM;
    logCtx.payload.bytes = payload;
    logCtx.payload.signatureSize = 0;

    LogBegin(logCtx, "SIGN_BEGIN");

    const auto start = V2XTimeNow();

    if (!privateKey) {

        logCtx.durationUs = V2XElapsedMicros(start);

        LogFailure(logCtx, V2XLogLevel::ERROR, "SIGN_FAIL", "missing_private_key", "private key is null");

        return {};
    }

    if (payload.empty()) {

        logCtx.durationUs = V2XElapsedMicros(start);

        LogFailure(logCtx, V2XLogLevel::WARN, "SIGN_FAIL", "empty_payload", "payload is empty");

        return {};
    }

    MdContext mdCtx(EVP_MD_CTX_new());

    if (!mdCtx) {

        logCtx.durationUs = V2XElapsedMicros(start);

        LogFailure(logCtx, V2XLogLevel::ERROR, "SIGN_FAIL", "md_context_creation_failed", "EVP_MD_CTX_new failed");

        return {};
    }

    std::vector<uint8_t> signature(EVP_PKEY_size(privateKey));
    size_t signatureSize = signature.size();

    const bool ok =
        EVP_DigestSignInit(mdCtx.get(), nullptr, EVP_sha256(), nullptr, privateKey) == 1 &&
        EVP_DigestSignUpdate(mdCtx.get(), payload.data(), payload.size()) == 1 &&
        EVP_DigestSignFinal(mdCtx.get(), signature.data(), &signatureSize) == 1;

    logCtx.durationUs = V2XElapsedMicros(start);

    if (!ok) {

        LogFailure(logCtx, V2XLogLevel::ERROR, "SIGN_FAIL", "digest_sign_failed", "DigestSign failed");

        return {};
    }

    signature.resize(signatureSize);

    logCtx.payload.signatureSize = static_cast<uint32_t>(signatureSize);

    LogSuccess(logCtx, "SIGN_OK", "signature_size=" + std::to_string(signatureSize));

    return signature;
}

//=============================================================================
// VERIFY
//=============================================================================

bool CamSecurity::Verify(const ns3::PacketContext& ctx, const std::vector<uint8_t>& payload, const std::vector<uint8_t>& signature, X509* certificate) {

    ns3::PacketContext logCtx = ctx;

    logCtx.payload.type = ns3::PayloadType::CAM;
    logCtx.payload.bytes = payload;
    logCtx.payload.signatureSize = static_cast<uint32_t>(signature.size());

    const auto start = V2XTimeNow();

    if (!certificate) {

        logCtx.durationUs = V2XElapsedMicros(start);

        LogFailure(logCtx, V2XLogLevel::ERROR, "VERIFY_FAIL", "missing_certificate", "certificate is null");

        return false;
    }

    if (payload.empty()) {

        logCtx.durationUs = V2XElapsedMicros(start);

        LogFailure(logCtx, V2XLogLevel::ERROR, "VERIFY_FAIL", "empty_payload", "payload is empty");

        return false;
    }

    if (signature.empty()) {

        logCtx.durationUs = V2XElapsedMicros(start);

        LogFailure(logCtx, V2XLogLevel::ERROR, "VERIFY_FAIL", "empty_signature", "signature is empty");

        return false;
    }

    PublicKey publicKey(X509_get_pubkey(certificate));

    if (!publicKey) {

        logCtx.durationUs = V2XElapsedMicros(start);

        LogFailure(logCtx, V2XLogLevel::ERROR, "VERIFY_FAIL", "public_key_not_found", "public key unavailable");

        return false;
    }

    MdContext mdCtx(EVP_MD_CTX_new());

    if (!mdCtx) {

        logCtx.durationUs = V2XElapsedMicros(start);

        LogFailure(logCtx, V2XLogLevel::ERROR, "VERIFY_FAIL", "digest_context_creation_failed", "EVP_MD_CTX_new failed");

        return false;
    }

    const bool ok =
        EVP_DigestVerifyInit(mdCtx.get(), nullptr, EVP_sha256(), nullptr, publicKey.get()) == 1 &&
        EVP_DigestVerifyUpdate(mdCtx.get(), payload.data(), payload.size()) == 1 &&
        EVP_DigestVerifyFinal(mdCtx.get(), signature.data(), signature.size()) == 1;

    logCtx.durationUs = V2XElapsedMicros(start);

    if (!ok) {
        LogFailure(logCtx, V2XLogLevel::ERROR, "VERIFY_FAIL", "signature_verification_failed", "signature verification failed");
        return false;
    }

    return true;   // <-- mantém só isto, sem o LogSuccess antes

}
