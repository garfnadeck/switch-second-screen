#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <Arduino.h>
#include <lvgl.h>

struct CachedAsset {
    String path;
    uint8_t* data = nullptr;
    size_t size = 0;
    lv_img_dsc_t descriptor;
    bool loaded = false;
};

class AssetManager {
public:
    static bool initSD(int csPin = 10, int sckPin = 12, int misoPin = 13, int mosiPin = 11);
    static bool isSDReady();
    static bool preloadToPSRAM(const char* sdPath, CachedAsset& outAsset, uint16_t width, uint16_t height, lv_img_cf_t colorFormat = LV_IMG_CF_TRUE_COLOR_ALPHA);
    static void freeAsset(CachedAsset& asset);

private:
    static bool sdInitialized;
};

#endif // ASSET_MANAGER_H
