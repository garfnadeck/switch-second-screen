#include "core/asset_manager.h"

#ifdef ARDUINO_ARCH_ESP32
#include <SD_MMC.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include "hardware/es3c35p_pins.h"
#endif

bool AssetManager::sdInitialized = false;

bool AssetManager::initSD(int csPin, int sckPin, int misoPin, int mosiPin) {
    (void)csPin; (void)sckPin; (void)misoPin; (void)mosiPin;
#ifdef ARDUINO_ARCH_ESP32
    if (sdInitialized) return true;
    // Pinagem SDIO 4-bit validada na ES3C35P (CLK=5, CMD=4, D0=6, D1=7, D2=2, D3=3) a 20 MHz
    SD_MMC.setPins(ES3C35P_SD_CLK, ES3C35P_SD_CMD, ES3C35P_SD_D0, ES3C35P_SD_D1, ES3C35P_SD_D2, ES3C35P_SD_D3);
    if (!SD_MMC.begin("/sdcard", false, true, 20000)) {
        // Fallback para SDIO 1-bit se necessario
        SD_MMC.setPins(ES3C35P_SD_CLK, ES3C35P_SD_CMD, ES3C35P_SD_D0);
        if (!SD_MMC.begin("/sdcard", true, true, 20000)) {
            Serial.println("[AssetManager] MicroSD nao montou via SD_MMC (usando fallback LittleFS).");
            sdInitialized = false;
            return false;
        }
    }
    uint64_t cardSize = SD_MMC.cardSize() / (1024ULL * 1024ULL);
    Serial.printf("[AssetManager] MicroSD montado via SD_MMC! Tamanho: %llu MB\n", cardSize);
    sdInitialized = true;
    ensureDir("/assets");
    ensureDir("/assets/za");
    ensureDir("/assets/za/maps");
    ensureDir("/assets/za/sprites");
    return true;
#else
    sdInitialized = false;
    return false;
#endif
}

bool AssetManager::isSDReady() {
    return sdInitialized;
}

uint64_t AssetManager::sdTotalMB() {
#ifdef ARDUINO_ARCH_ESP32
    if (!sdInitialized) return 0;
    return SD_MMC.totalBytes() / (1024ULL * 1024ULL);
#else
    return 0;
#endif
}

uint64_t AssetManager::sdUsedMB() {
#ifdef ARDUINO_ARCH_ESP32
    if (!sdInitialized) return 0;
    return SD_MMC.usedBytes() / (1024ULL * 1024ULL);
#else
    return 0;
#endif
}

bool AssetManager::ensureDir(const char* dirPath) {
#ifdef ARDUINO_ARCH_ESP32
    if (!sdInitialized || !dirPath || !dirPath[0]) return false;
    String p(dirPath);
    for (int i = 1; i <= (int)p.length(); i++) {
        if (i == (int)p.length() || p[i] == '/') {
            String sub = p.substring(0, i);
            if (sub.length() > 0 && !SD_MMC.exists(sub.c_str())) {
                SD_MMC.mkdir(sub.c_str());
            }
        }
    }
    return SD_MMC.exists(dirPath);
#else
    (void)dirPath;
    return false;
#endif
}

bool AssetManager::writeFile(const char* sdPath, const uint8_t* data, size_t len, bool append) {
#ifdef ARDUINO_ARCH_ESP32
    if (!sdPath || !sdPath[0]) return false;
    String p(sdPath);
    int lastSlash = p.lastIndexOf('/');
    if (lastSlash > 0 && sdInitialized) {
        ensureDir(p.substring(0, lastSlash).c_str());
    }
    File f;
    if (sdInitialized) {
        f = SD_MMC.open(sdPath, append ? FILE_APPEND : FILE_WRITE);
    } else {
        f = LittleFS.open(sdPath, append ? "a" : "w");
    }
    if (!f) return false;
    size_t written = (len > 0 && data) ? f.write(data, len) : 0;
    f.close();
    return written == len;
#else
    (void)sdPath; (void)data; (void)len; (void)append;
    return false;
#endif
}

bool AssetManager::preloadToPSRAM(const char* sdPath, CachedAsset& outAsset, uint16_t width, uint16_t height, lv_img_cf_t colorFormat) {
    if (outAsset.loaded) {
        freeAsset(outAsset);
    }

#ifdef ARDUINO_ARCH_ESP32
    File file;
    if (sdInitialized && SD_MMC.exists(sdPath)) {
        file = SD_MMC.open(sdPath, FILE_READ);
    } else if (LittleFS.exists(sdPath)) {
        file = LittleFS.open(sdPath, "r");
    }
    if (!file) {
        return false;
    }

    size_t bpp = (colorFormat == LV_IMG_CF_TRUE_COLOR_ALPHA) ? 3 : 2;
    size_t expectedSize = (size_t)width * (size_t)height * bpp;
    size_t fileSize = file.size();
    if (fileSize < expectedSize || expectedSize == 0) {
        file.close();
        return false;
    }

    uint8_t* buffer = (uint8_t*)heap_caps_malloc(expectedSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) {
        buffer = (uint8_t*)malloc(expectedSize);
    }
    if (!buffer) {
        file.close();
        return false;
    }

    size_t got = file.read(buffer, expectedSize);
    file.close();
    if (got != expectedSize) {
        free(buffer);
        return false;
    }

    outAsset.path = String(sdPath);
    outAsset.data = buffer;
    outAsset.size = expectedSize;
    outAsset.loaded = true;

    memset(&outAsset.descriptor, 0, sizeof(outAsset.descriptor));
    outAsset.descriptor.header.always_zero = 0;
    outAsset.descriptor.header.w = width;
    outAsset.descriptor.header.h = height;
    outAsset.descriptor.header.cf = colorFormat;
    outAsset.descriptor.data_size = expectedSize;
    outAsset.descriptor.data = buffer;

    return true;
#else
    (void)sdPath; (void)outAsset; (void)width; (void)height; (void)colorFormat;
    return false;
#endif
}

void AssetManager::freeAsset(CachedAsset& asset) {
    if (asset.loaded && asset.data != nullptr) {
        free(asset.data);
        asset.data = nullptr;
        asset.size = 0;
        asset.loaded = false;
    }
}
