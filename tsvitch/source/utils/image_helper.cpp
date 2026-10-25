#include <borealis/core/singleton.hpp>
#include <borealis/core/application.hpp>
#include <borealis/core/cache_helper.hpp>
#include <borealis/core/thread.hpp>
#include <stb_image.h>
#include <lunasvg.h>
#include <algorithm>
#include <cmath>

#include "utils/image_helper.hpp"
#include "api/tsvitch/util/http.hpp"

#ifdef USE_WEBP
#include <webp/decode.h>
#endif

class ImageThreadPool : public cpr::ThreadPool, public brls::Singleton<ImageThreadPool> {
  public:
    ImageThreadPool() : cpr::ThreadPool(1, ImageHelper::REQUEST_THREADS, std::chrono::milliseconds(5000)) {
        brls::Logger::info("max_thread_num: {}", this->max_thread_num);
        this->Start();
    }

    ~ImageThreadPool() override { this->Stop(); }
};

ImageHelper::ImageHelper(brls::Image *view) : imageView(view) {}

ImageHelper::~ImageHelper() { brls::Logger::verbose("delete ImageHelper {}", (size_t)this); }

std::shared_ptr<ImageHelper> ImageHelper::with(brls::Image *view) {
    std::lock_guard<std::mutex> lock(requestMutex);
    std::shared_ptr<ImageHelper> item;

    if (!requestPool.empty() && (*requestPool.begin())->getImageView() == nullptr) {
        item = *requestPool.begin();
        item->setImageView(view);
        requestPool.splice(requestPool.end(), requestPool, requestPool.begin());
    } else {
        item = std::make_shared<ImageHelper>(view);
        requestPool.emplace_back(item);
    }

    auto iter = --requestPool.end();
    requestMap[view] = iter;
    item->currentIter = iter;

    item->isCancel = false;

    item->imageView->ptrLock();

    item->imageView->setFreeTexture(false);

    brls::Logger::verbose("with view: {} {} {}", (size_t)view, (size_t)item.get(), (size_t)(*iter).get());

    return item;
}

void ImageHelper::load(std::string url) {
    this->imageUrl = url;
    if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0) {
        clean();
        return;
    }

    brls::Logger::verbose("load view: {} {}", (size_t)this->imageView, (size_t)this);

    int tex = brls::TextureCache::instance().getCache(this->imageUrl);
    if (tex > 0) {
        brls::Logger::verbose("cache hit: {}", this->imageUrl);
        this->imageView->innerSetImage(tex);
        this->clean();
        return;
    }

    brls::Logger::verbose("request Image 1: {} {}", this->imageUrl, this->isCancel.load());
    ImageThreadPool::instance().Submit([this]() {
        brls::Logger::verbose("Submit view: {} {} {} {}", (size_t)this->imageView, (size_t)this,
                              this->imageUrl, this->isCancel.load());
        if (this->isCancel) {
            this->clean();
            return;
        }
        try {
            this->requestImage();
        } catch (...) {
            this->clean();
        }
    });
}

void ImageHelper::requestImage() {
    constexpr cpr::cpr_pf_arg_t maximum = 8 * 1024 * 1024;
    auto response = cpr::Get(tsvitch::HTTP::VERIFY, tsvitch::HTTP::PROXIES, tsvitch::HTTP::HEADERS,
                             cpr::Url{imageUrl}, cpr::Timeout{15000},
                             cpr::ProgressCallback([this](cpr::cpr_pf_arg_t total, cpr::cpr_pf_arg_t now,
                                                          cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, intptr_t) {
                                 return !isCancel && total <= maximum && now <= maximum;
                             }));
    if (response.error || response.status_code != 200 || response.text.empty() ||
        response.text.size() > maximum || isCancel) {
        clean();
        return;
    }
    int width = 0, height = 0, channels = 0;
    std::shared_ptr<std::vector<unsigned char>> pixels;
    const auto *data = reinterpret_cast<const unsigned char *>(response.text.data());
    const auto size = response.text.size();
    const auto svg = response.text.find("<svg");
    if (svg != std::string::npos && svg < 1024) {
        auto document = lunasvg::Document::loadFromData(response.text);
        if (document && document->width() > 0 && document->height() > 0 && std::isfinite(document->width()) &&
            std::isfinite(document->height())) {
            const auto scale = std::min(384.f / static_cast<float>(document->width()),
                                        576.f / static_cast<float>(document->height()));
            width = std::max(1, static_cast<int>(document->width() * scale));
            height = std::max(1, static_cast<int>(document->height() * scale));
            auto bitmap = document->renderToBitmap(width, height);
            if (bitmap.isNull()) {
                clean();
                return;
            }
            bitmap.convertToRGBA();
            pixels = std::make_shared<std::vector<unsigned char>>(bitmap.data(),
                                                                  bitmap.data() + width * height * 4);
        }
    } else {
        unsigned char *decoded = nullptr;
        bool webp = false;
#ifdef USE_WEBP
        webp = WebPGetInfo(data, size, &width, &height) != 0;
#endif
        const bool known =
            webp || stbi_info_from_memory(data, static_cast<int>(size), &width, &height, &channels);
        if (known && width > 0 && height > 0 && width <= 8192 && height <= 8192 &&
            uint64_t(width) * height <= 16 * 1024 * 1024) {
#ifdef USE_WEBP
            if (webp)
                decoded = WebPDecodeRGBA(data, size, &width, &height);
            else
#endif
                decoded = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channels, 4);
        }
        if (decoded) {
            const auto scale = std::min(1.f, std::min(384.f / width, 576.f / height));
            const int w = std::max(1, int(width * scale)), h = std::max(1, int(height * scale));
            pixels = std::make_shared<std::vector<unsigned char>>(w * h * 4);
            for (int y = 0; y < h; ++y) {
                const float sy = std::max(0.f, (y + .5f) * height / h - .5f);
                const int y0 = std::min(height - 1, int(sy)), y1 = std::min(height - 1, y0 + 1);
                const float fy = sy - y0;
                for (int x = 0; x < w; ++x) {
                    const float sx = std::max(0.f, (x + .5f) * width / w - .5f);
                    const int x0 = std::min(width - 1, int(sx)), x1 = std::min(width - 1, x0 + 1);
                    const float fx = sx - x0;
                    for (int c = 0; c < 4; ++c) {
                        const float top = decoded[(y0 * width + x0) * 4 + c] * (1 - fx) +
                                          decoded[(y0 * width + x1) * 4 + c] * fx;
                        const float bottom = decoded[(y1 * width + x0) * 4 + c] * (1 - fx) +
                                             decoded[(y1 * width + x1) * 4 + c] * fx;
                        (*pixels)[(y * w + x) * 4 + c] =
                            static_cast<unsigned char>(top * (1 - fy) + bottom * fy);
                    }
                }
            }
#ifdef USE_WEBP
            if (webp)
                WebPFree(decoded);
            else
#endif
                stbi_image_free(decoded);
            width = w;
            height = h;
        }
    }
    if (!pixels || pixels->empty() || isCancel) {
        clean();
        return;
    }
    const auto key = imageUrl;
    brls::sync([this, key, pixels, width, height] {
        if (!isCancel) {
            int texture = brls::TextureCache::instance().getCache(key);
            if (texture <= 0) {
                texture =
                    nvgCreateImageRGBA(brls::Application::getNVGContext(), width, height, 0, pixels->data());
                if (texture > 0)
                    brls::TextureCache::instance().addCache(key, texture);
            }
            if (texture > 0 && imageView)
                imageView->innerSetImage(texture);
        }
        clean();
    });
}

void ImageHelper::clean() {
    std::lock_guard<std::mutex> lock(requestMutex);

    if (this->imageView)
        this->imageView->ptrUnlock();

    if (this->currentIter != requestPool.end())
        requestPool.splice(requestPool.begin(), requestPool, this->currentIter);
    this->imageView = nullptr;
    this->currentIter = requestPool.end();
}

void ImageHelper::clear(brls::Image *view) {
    brls::TextureCache::instance().removeCache(view->getTexture());
    view->clear();

    std::lock_guard<std::mutex> lock(requestMutex);

    if (requestMap.find(view) == requestMap.end())
        return;

    brls::Logger::verbose("clear view: {} {}", (size_t)view, (size_t)(*requestMap[view]).get());

    if ((*requestMap[view])->imageView == view) {
        (*requestMap[view])->cancel();
    }
    requestMap.erase(view);
}

void ImageHelper::cancel() {
    brls::Logger::verbose("Cancel request: {}", this->imageUrl);
    this->isCancel = true;
}

void ImageHelper::setRequestThreads(size_t num) {
    REQUEST_THREADS = num;
    ImageThreadPool::instance().min_thread_num = 1;
    ImageThreadPool::instance().max_thread_num = num;
}

void ImageHelper::setImageView(brls::Image *view) { this->imageView = view; }

brls::Image *ImageHelper::getImageView() { return this->imageView; }
