#include "core/asset_manager.h"

#ifdef ARDUINO_ARCH_ESP32
#include <SD.h>
#include <SPI.h>
#endif

bool AssetManager::sdInitialized = false;

bool AssetManager::initSD(int csPin, int sckPin, int misoPin, int mosiPin) {
#ifdef ARDUINO_ARCH_ESP32
    SPI.begin(sckPin, misoPin, mosiPin, csPin);
    if (!SD.begin(csPin, SPI, 40000000)) {
        Serial.println("[AssetManager] Falha ao montar Cartão MicroSD!");
        sdInitialized = false;
        return false;
    }
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("[AssetManager] MicroSD montado com sucesso! Tamanho: %lluMB\n", cardSize);
    sdInitialized = true;
    return true;
#else
    (void)csPin; (void)sckPin; (void)misoPin; (void)mosiPin;
    Serial.println("[AssetManager] Modo Simulação (Pico W): Leitura física de SD desativada.");
    sdInitialized = false;
    return false;
#endif
}

bool AssetManager::isSDReady() {
    return sdInitialized;
}

bool AssetManager::preloadToPSRAM(const char* sdPath, CachedAsset& outAsset, uint16_t width, uint16_t height, lv_img_cf_t colorFormat) {
    if (outAsset.loaded) {
        freeAsset(outAsset);
    }

#ifdef ARDUINO_ARCH_ESP32
    if (!sdInitialized) return false;

    File file = SD.open(sdPath, FILE_READ);
    if (!file) {
        Serial.printf("[AssetManager] Arquivo não encontrado no SD: %s\n", sdPath);
        return false;
    }

    size_t fileSize = file.size();
    uint8_t* buffer = (uint8_t*)ps_malloc(fileSize);
    if (!buffer) {
        Serial.println("[AssetManager] ERRO CRÍTICO: Falha ao alocar memória na PSRAM!");
        file.close();
        return false;
    }

    file.read(buffer, fileSize);
    file.close();

    outAsset.path = String(sdPath);
    outAsset.data = buffer;
    outAsset.size = fileSize;
    outAsset.loaded = true;

    outAsset.descriptor.header.always_zero = 0;
    outAsset.descriptor.header.w = width;
    outAsset.descriptor.header.h = height;
    outAsset.descriptor.header.cf = colorFormat;
    outAsset.descriptor.data_size = fileSize;
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
