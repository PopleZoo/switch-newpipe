#include "newpipe/image_loader.hpp"

#include <chrono>

#include "newpipe/http_client.hpp"
#include "newpipe/log.hpp"

#ifdef NEWPIPE_HAVE_LIBWEBP
#include <webp/decode.h>
#endif

// The stb_image implementation is already compiled into libborealis.a
// (nanovg.o); only the declarations are needed here to avoid duplicate
// symbols at link time.
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace newpipe {

namespace {

// nanovg/stb_image can't decode WebP, so YouTube's vi_webp/*.webp thumbnails come
// back as a 0 texture. Rewrite to the equivalent JPG that stb_image can decode.
std::string rewrite_unsupported_image_url(const std::string& url) {
    std::string rewritten = url;
    const std::string webp_path = "/vi_webp/";
    const std::string jpg_path = "/vi/";
    auto pos = rewritten.find(webp_path);
    if (pos != std::string::npos) {
        rewritten.replace(pos, webp_path.size(), jpg_path);
    }
    const std::string webp_ext = ".webp";
    if (rewritten.size() >= webp_ext.size()) {
        const auto ext_pos = rewritten.rfind(webp_ext);
        if (ext_pos != std::string::npos) {
            const auto query_pos = rewritten.find('?', ext_pos);
            if (query_pos == std::string::npos
                && ext_pos + webp_ext.size() == rewritten.size()) {
                rewritten.replace(ext_pos, webp_ext.size(), ".jpg");
            }
        }
    }
    return rewritten;
}

bool is_webp_data(const std::string& data) {
    return data.size() > 12
        && data[0] == 'R'
        && data[1] == 'I'
        && data[2] == 'F'
        && data[3] == 'F'
        && data.compare(8, 4, "WEBP") == 0;
}

std::string image_format_label(const std::string& data) {
    if (is_webp_data(data)) {
        return "webp";
    }
    if (data.size() > 3
        && static_cast<unsigned char>(data[0]) == 0xFF
        && static_cast<unsigned char>(data[1]) == 0xD8) {
        return "jpeg";
    }
    if (data.size() > 8 && data.compare(1, 3, "PNG") == 0) {
        return "png";
    }
    if (data.size() > 6 && data.compare(0, 4, "GIF8") == 0) {
        return "gif";
    }
    if (data.size() > 2 && data[0] == 'B' && data[1] == 'M') {
        return "bmp";
    }
    return "unknown";
}

void png_write_callback(void* context, void* data, int size) {
    auto* out = static_cast<std::string*>(context);
    out->append(static_cast<const char*>(data), static_cast<size_t>(size));
}

std::string rgba_to_png(const unsigned char* rgba, int width, int height) {
    std::string png;
    stbi_write_png_to_func(&png_write_callback, &png, width, height, 4, rgba, width * 4);
    return png;
}

std::string convert_webp_to_png(const std::string& webp) {
    int width = 0;
    int height = 0;
    uint8_t* rgba = WebPDecodeRGBA(
        reinterpret_cast<const uint8_t*>(webp.data()), webp.size(), &width, &height);
    if (!rgba) {
        return std::string();
    }

    std::string png = rgba_to_png(rgba, width, height);
    WebPFree(rgba);
    return png;
}

std::string convert_image_to_png(const std::string& data) {
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load_from_memory(
        reinterpret_cast<const unsigned char*>(data.data()),
        static_cast<int>(data.size()),
        &width,
        &height,
        &channels,
        4);
    if (!pixels) {
        return std::string();
    }

    std::string png = rgba_to_png(pixels, width, height);
    stbi_image_free(pixels);
    return png;
}

}  // namespace

ImageLoader& ImageLoader::instance() {
    static ImageLoader loader;
    return loader;
}

ImageLoader::~ImageLoader() {
    stop();
}

void ImageLoader::start() {
    if (running_) {
        return;
    }

    running_ = true;
    log_line("image: start worker");
    thread_ = std::thread([this]() { worker(); });
}

void ImageLoader::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    log_line("image: stop worker");
}

void ImageLoader::load(const std::string& url, brls::Image* target) {
    if (!target) {
        return;
    }

    const std::string fetch_url = rewrite_unsupported_image_url(url);
    target->setImageAsync([fetch_url, this](std::function<void(const std::string&, size_t)> cb) {
        std::string cached;
        if (tryGetCached(fetch_url, &cached)) {
            logf("image: cache hit %s bytes=%zu", fetch_url.c_str(), cached.size());
            cb(cached, cached.size());
            return;
        }
        logf("image: queue %s", fetch_url.c_str());
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push({fetch_url, cb});
    });
}

bool ImageLoader::tryGetCached(const std::string& url, std::string* out) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = cache_.find(url);
    if (it == cache_.end()) {
        return false;
    }
    *out = it->second;
    return true;
}

void ImageLoader::putCache(const std::string& url, const std::string& data) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_[url] = data;
}

void ImageLoader::worker() {
    HttpsHttpClient client;
    log_line("image: worker entered");

    while (running_) {
        AsyncRequest request;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!queue_.empty()) {
                request = std::move(queue_.front());
                queue_.pop();
            }
        }

        if (request.url.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        std::string cached;
        if (tryGetCached(request.url, &cached)) {
            logf("image: cache hit (worker) %s bytes=%zu", request.url.c_str(), cached.size());
            request.callback(cached, cached.size());
            continue;
        }

        auto data = client.get(request.url);
        if (data.has_value() && !data->empty()) {
            logf("image: fetched %s bytes=%zu", request.url.c_str(), data->size());
            const std::string format = image_format_label(*data);
            std::string final_data = *data;
            std::string converted;
            if (format == "webp") {
#ifdef NEWPIPE_HAVE_LIBWEBP
                converted = convert_webp_to_png(final_data);
#else
                logf("image: webp decode skipped (libwebp unavailable) %s", request.url.c_str());
#endif
            } else {
                converted = convert_image_to_png(final_data);
            }
            if (!converted.empty()) {
                logf("image: converted %s to png %s bytes=%zu",
                     format.c_str(),
                     request.url.c_str(),
                     converted.size());
                final_data = std::move(converted);
            } else {
                logf("image: decode failed format=%s %s", format.c_str(), request.url.c_str());
            }
            putCache(request.url, final_data);
            request.callback(final_data, final_data.size());
        } else {
            logf("image: fetch failed %s", request.url.c_str());
            request.callback("", 0);
        }
    }

    log_line("image: worker exit");
}

}  // namespace newpipe
