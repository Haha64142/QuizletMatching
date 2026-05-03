/* Main program:
 * 1. take full screenshot
 * 2. crop image and process it
 * 3. convert crop to pair id
 * 4. check if pair id exists in remembered pairs
 * 5. if yes, click the current tile and the tile in remembered pairs
 * 6. remove the pair id from remebered pairs
 * 7. else, add the pair id and tile id to remembered pairs
 * 8. repeat 2-7 changing the cropped section (but keep the same full screenshot)
 *
 * Image processing:
 * 1. resize to a 118x84 (1/3) or even smaller
 * 2. grayscale (possibly combine 2 and 3 into one step)
 * 3. threshold (R+G+B > 3*75)
 *
 * Variables and Definitions:
 * tile number: 0-11 representing the physical location of the tile on the screen
 * pair id: 0-46 representing each term + definition pair
 * tile hash: specific hash for each tile (94 unique tiles)
 *
 * click positions: list - index: tile number, value: coord to click
 * crop positions: list - index: tile number, value: top corner of the cropped tile
 * remembered pairs: unordered_map - key: pair id, value: tile number
 * */

#include <array>
#include <cstdint>
#include <iostream>
#include <stack>
#include <string.h>
#include <unordered_map>
#include <vector>
#include <windows.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

constexpr int screenWidth = 1920;
constexpr int screenHeight = 1080;

constexpr int cropWidth = 320;
constexpr int cropHeight = 160;

constexpr std::array<int, 12> cropPosX = {231, 605,  979, 1352, 231, 605,
                                          979, 1352, 231, 605,  979, 1352};
constexpr std::array<int, 12> cropPosY = {260, 260, 260, 260, 530, 530,
                                          530, 530, 799, 799, 799, 799};

constexpr std::array<int, 12> origClickPosX = {391,  760,  1134, 1512, 391,  760,
                                               1134, 1512, 391,  760,  1134, 1512};
constexpr std::array<int, 12> origClickPosY = {339, 339, 339, 339, 608, 608,
                                               608, 608, 881, 881, 881, 881};

constexpr std::array<int, 12> normalizePos(const std::array<int, 12> pos, int dimensionLength) {
    std::array<int, 12> newPos = {};
    for (size_t i = 0; i < 12; ++i) {
        newPos.at(i) = pos.at(i) * 65535 / (dimensionLength - 1);
    }
    return newPos;
}

constexpr std::array<int, 12> clickPosX = normalizePos(origClickPosX, screenWidth);
constexpr std::array<int, 12> clickPosY = normalizePos(origClickPosY, screenHeight);

const int resizeWidth = 120;
const int resizeHeight = 60;

constexpr int thresholdVal = 128 * 256; // 50%

INPUT mouse[2] = {};

void setupMouse() {
    mouse[0].type = INPUT_MOUSE;
    mouse[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;

    mouse[1].type = INPUT_MOUSE;
    mouse[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
}

void click(int tileNumber) {
    std::cout << "Click coords: " << clickPosX[tileNumber] << ", " << clickPosY[tileNumber] << '\n';
    mouse[0].mi.dx = clickPosX[tileNumber];
    mouse[0].mi.dy = clickPosY[tileNumber];

    SendInput(2, mouse, sizeof(INPUT));
}

std::vector<uint8_t> takeScreenshot() {
    HDC screenDC = GetDC(NULL);
    HDC memoryDC = CreateCompatibleDC(screenDC);
    HBITMAP targetBitmap = CreateCompatibleBitmap(screenDC, screenWidth, screenHeight);
    HGDIOBJ oldObj = SelectObject(memoryDC, targetBitmap);
    BitBlt(memoryDC, 0, 0, screenWidth, screenHeight, screenDC, 0, 0, SRCCOPY);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = screenWidth;
    bmi.bmiHeader.biHeight = -screenHeight;

    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint8_t> pixels(screenWidth * screenHeight * 4);
    GetDIBits(memoryDC, targetBitmap, 0, screenHeight, pixels.data(), &bmi, DIB_RGB_COLORS);

    // Copy screenshot to clipboard for debugging
    // size_t headerSize = sizeof(BITMAPINFOHEADER);
    // size_t pixelSize = pixels.size();
    // size_t totalSize = headerSize + pixelSize;
    //
    // HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, totalSize);
    //
    // void *dest = GlobalLock(hMem);
    //
    // memcpy(dest, &bmi.bmiHeader, headerSize);
    // memcpy(static_cast<uint8_t *>(dest) + headerSize, pixels.data(), pixelSize);
    //
    // GlobalUnlock(hMem);
    //
    // OpenClipboard(NULL);
    // EmptyClipboard();
    // SetClipboardData(CF_DIB, hMem);
    // CloseClipboard();

    SelectObject(memoryDC, oldObj);
    DeleteObject(targetBitmap);
    DeleteDC(memoryDC);
    ReleaseDC(NULL, screenDC);

    return pixels;
}

std::vector<uint8_t> crop(const std::vector<uint8_t> &src, int srcWidth, int srcHeight, int x,
                          int y, int cropWidth, int cropHeight) {
    std::vector<uint8_t> out(cropWidth * cropHeight * 4);
    for (int row = 0; row < cropHeight; ++row) {
        int srcIdx = ((y + row) * srcWidth + x) * 4;
        int destIdx = row * cropWidth * 4;

        memcpy(&out[destIdx], &src[srcIdx], cropWidth * 4);
    }

    // Copy to clipboard for debugging
    // BITMAPINFO bmi{};
    // bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    // bmi.bmiHeader.biWidth = cropWidth;
    // bmi.bmiHeader.biHeight = -cropHeight;
    //
    // bmi.bmiHeader.biPlanes = 1;
    // bmi.bmiHeader.biBitCount = 32;
    // bmi.bmiHeader.biCompression = BI_RGB;
    //
    // size_t headerSize = sizeof(BITMAPINFOHEADER);
    // size_t pixelSize = out.size();
    // size_t totalSize = headerSize + pixelSize;
    //
    // HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, totalSize);
    //
    // void *dest = GlobalLock(hMem);
    //
    // memcpy(dest, &bmi.bmiHeader, headerSize);
    // memcpy(static_cast<uint8_t *>(dest) + headerSize, out.data(), pixelSize);
    //
    // GlobalUnlock(hMem);
    //
    // OpenClipboard(NULL);
    // EmptyClipboard();
    // SetClipboardData(CF_DIB, hMem);
    // CloseClipboard();

    return out;
}

std::vector<uint8_t> crop(const std::vector<uint8_t> &src, int tileNumber) {
    return crop(src, screenWidth, screenHeight, cropPosX.at(tileNumber), cropPosY.at(tileNumber),
                cropWidth, cropHeight);
}

std::vector<uint8_t> resize(const std::vector<uint8_t> &src, int inputWidth, int inputHeight,
                            int outputWidth, int outputHeight) {
    std::vector<uint8_t> out(outputWidth * outputHeight * 4);
    stbir_resize_uint8_linear(src.data(), inputWidth, inputHeight, 0, out.data(), outputWidth,
                              outputHeight, 0, STBIR_BGRA);

    // Copy to clipboard for debugging
    // BITMAPINFO bmi{};
    // bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    // bmi.bmiHeader.biWidth = outputWidth;
    // bmi.bmiHeader.biHeight = -outputHeight;
    //
    // bmi.bmiHeader.biPlanes = 1;
    // bmi.bmiHeader.biBitCount = 32;
    // bmi.bmiHeader.biCompression = BI_RGB;
    //
    // size_t headerSize = sizeof(BITMAPINFOHEADER);
    // size_t pixelSize = out.size();
    // size_t totalSize = headerSize + pixelSize;
    //
    // HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, totalSize);
    //
    // void *dest = GlobalLock(hMem);
    //
    // memcpy(dest, &bmi.bmiHeader, headerSize);
    // memcpy(static_cast<uint8_t *>(dest) + headerSize, out.data(), pixelSize);
    //
    // GlobalUnlock(hMem);
    //
    // OpenClipboard(NULL);
    // EmptyClipboard();
    // SetClipboardData(CF_DIB, hMem);
    // CloseClipboard();

    return out;
}

std::vector<uint8_t> resize(const std::vector<uint8_t> &src) {
    return resize(src, cropWidth, cropHeight, resizeWidth, resizeHeight);
}

std::vector<uint8_t> threshold(const std::vector<uint8_t> &src) {
    std::vector<uint8_t> out(src.size() / 32);
    uint8_t byte = 0x0;
    int bitCount = 0;
    size_t outIdx = 0;

    for (size_t i = 0; i < src.size(); i += 4) {
        bool bit = (77 * src[i + 2] + 150 * src[i + 1] + 29 * src[i]) > thresholdVal;
        byte = (byte << 1) | bit;

        ++bitCount;

        if (bitCount == 8) {
            out[outIdx++] = byte;
            byte = 0x0;
            bitCount = 0;
        }
    }

    return out;
}

std::unordered_map<size_t, int> pairIDs = {
    // Terms
    {0, 0},
    {1, 1},
    {2, 2},
    {4018, 3},
    {4, 4},
    {5, 5},
    {6, 6},
    {7, 7},
    {4479, 8},
    {9, 9},
    {3939, 10},
    {13572, 11},
    {14337, 12},
    {2040, 13},
    {14, 14},
    {3198, 15},
    {16, 16},
    {17, 17},
    {18, 18},
    {3636, 19},
    {20, 20},
    {21, 21},
    {22, 22},
    {23, 23},
    {24, 24},
    {25, 25},
    {26, 26},
    {27, 27},
    {28, 28},
    {29, 29},
    {30, 30},
    {9423, 31},
    {32, 32},
    {33, 33},
    {34, 34},
    {35, 35},
    {13355, 36},
    {2653, 37},
    {38, 38},
    {39, 39},
    {40, 40},
    {41, 41},
    {42, 42},
    {43, 43},
    {44, 44},
    {5397, 45},
    {46, 46},
    // Definitions
    {47, 0},
    {48, 1},
    {49, 2},
    {29073, 3},
    {51, 4},
    {52, 5},
    {53, 6},
    {54, 7},
    {37921, 8},
    {56, 9},
    {28307, 10},
    {21293, 11},
    {34960, 12},
    {37246, 13},
    {61, 14},
    {32395, 15},
    {63, 16},
    {64, 17},
    {65, 18},
    {32878, 19},
    {67, 20},
    {68, 21},
    {69, 22},
    {70, 23},
    {71, 24},
    {72, 25},
    {73, 26},
    {74, 27},
    {75, 28},
    {76, 29},
    {77, 30},
    {43105, 31},
    {79, 32},
    {80, 33},
    {81, 34},
    {82, 35},
    {31935, 36},
    {12409, 37},
    {85, 38},
    {86, 39},
    {87, 40},
    {88, 41},
    {89, 42},
    {90, 43},
    {91, 44},
    {21435, 45},
    {93, 46},
};

int main() {
    setupMouse();

    std::unordered_map<int, int> rememberedPairs;
    int pairID = 0;
    std::stack<int> clickNumbers;

    const std::vector<uint8_t> fullScreenshot = takeScreenshot();
    bool exit = false;

    for (size_t i = 0; i < 12; ++i) {
        std::vector<uint8_t> processedImage = threshold(resize(crop(fullScreenshot, i)));
        size_t sum = 0;
        for (size_t i = 0; i < processedImage.size(); ++i) {
            sum += processedImage[i];
        }
        // std::cout << sum << '\n';
        auto it2 = pairIDs.find(sum);
        if (it2 != pairIDs.end()) {
            pairID = it2->second;
        } else {
            std::cout << "Sum for tile number " << i << " is " << sum << '\n';
            exit = true;
        }

        // pairID = i / 2;
        // std::cout << "Pair id: " << pairID << '\n';
        std::unordered_map<int, int>::const_iterator it = rememberedPairs.find(pairID);
        if (it != rememberedPairs.end()) {
            // std::cout << "Clicking " << i << " and " << it->second << '\n';
            clickNumbers.push(it->second);
            clickNumbers.push(i);
            rememberedPairs.erase(pairID);
        } else {
            // std::cout << "Adding " << i << '\n';
            rememberedPairs.emplace(pairID, i);
        }
    }

    if (exit)
        return 1;

    for (size_t i = 0; i < 12; ++i) {
        click(clickNumbers.top());
        clickNumbers.pop();
        Sleep(160);
    }

    return 0;
}
