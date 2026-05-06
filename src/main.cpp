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
 * 1. grayscale
 * 2. ocr using tesseract
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

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <string.h>
#include <string>
#include <tesseract/baseapi.h>
#include <unordered_map>
#include <vector>
#include <windows.h>

constexpr int screenWidth = 1920;
constexpr int screenHeight = 1080;

constexpr int cropWidth = 350;
constexpr int cropHeight = 160;

constexpr std::array<int, 12> cropPosX = {216, 590,  964, 1337, 216, 590,
                                          964, 1337, 216, 590,  964, 1337};
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

INPUT mouse[2] = {};

tesseract::TessBaseAPI tess;

const std::unordered_map<std::string, int> textData = {
    {"The Election of", 0},
    {"Republican Herbert Hoover", 0},
    {"Stock Market", 1},
    {"A system of", 1},
    {"Margin", 2},
    {"Buying a stock", 2},
    {"Margin Call", 3},
    {"Demand by a", 3},
    {"Installment Plan", 4},
    {"People would make", 4},
    {"Black Tuesday", 5},
    {"On October prices", 5},
    {"Banks in a", 6},
    {"Depositors lost their", 6},
    {"Overproduction/Surplus", 7},
    {"Most economists agree", 7},
    {"Dust Bowl", 8},
    {"Poor agricultural practices", 8},
    {"John Steinbeck", 9},
    {"Wrote,The Grapes", 9},
    {"Public Works", 10},
    {"Projects such as", 10},
    {"Reconstruction Finance Corporation", 11},
    {"Set up to", 11},
    {"Emergency Relief and", 12},
    {"Called for billion", 12},
    {"Farmers", 13},
    {"In the summer", 13},
    {"Hoovervilles", 14},
    {"Because people blamed", 14},
    {"Bonus Army", 15},
    {"Veterans that set", 15},
    {"Douglas MacArthur", 16},
    {"Army chief of", 16},
    {"Herbert Hoover", 17},
    {"Failed to resolve", 17},
    {"Franklin Roosevelt FDR", 18},
    {"After serving as", 18},
    {"New Deal", 19},
    {"Roosevelt's policies", 19},
    {"Bank Holiday", 20},
    {"Closing of remaining", 20},
    {"The Brain Trust", 21},
    {"Name given to", 21},
    {"The Emergency Banking", 22},
    {"Required federal examiners", 22},
    {"Fireside Chats", 23},
    {"Direct talks FDR", 23},
    {"Securities and Exchange", 24},
    {"An independent agency", 24},
    {"Federal Deposit Insurance", 25},
    {"Provides government insurance", 25},
    {"Agriculture Adjustment Administration", 26},
    {"Paid farmers not", 26},
    {"The Civilian Conservation", 27},
    {"Employed single men", 27},
    {"Federal Emergency Relief", 28},
    {"Granted federal money", 28},
    {"Public Works Administration", 29},
    {"Provided employment in", 29},
    {"The Civil Works", 30},
    {"Worked somewhat like", 30},
    {"American Liberty League", 31},
    {"Business leaders and", 31},
    {"Huey Long", 32},
    {"The most serious", 32},
    {"Works Progress Administration", 33},
    {"Combated unemployment created", 33},
    {"The Social Security", 34},
    {"Its major goal", 34},
    {"Tennessee Valley Authority", 35},
    {"Program that brought", 35},
    {"Supreme Court Court", 36},
    {"Roosevelt wanted to", 36},
    {"Hobos", 37},
    {"Homeless wanderers who", 37},
    {"Deficit Spending", 38},
    {"The economic theory", 38},
    {"Deficit Spending", 39},
    {"Spending more than", 39},
    {"Okies", 40},
    {"Farmers who packed", 40},
    {"Loess", 41},
    {"Windblown topsoil;lands", 41},
    {"National Youth Administration", 42},
    {"Provided jobs and", 42},
    {"Federal Housing Administration", 43},
    {"Very small down", 43},
    {"Home Owners Loan", 44},
    {"Refinance loans so", 44},
    {"Soup Kitchens", 45},
    {"During the Depression", 45},
    {"Socialism", 46},
    {"As the American", 46},
};

void setupMouse() {
    mouse[0].type = INPUT_MOUSE;
    mouse[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;

    mouse[1].type = INPUT_MOUSE;
    mouse[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
}

int initTesseract() {
    if (tess.Init(NULL, "eng")) {
        std::cerr << "Could not initialize Tesseract\n";
        return 1;
    }
    return 0;
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

/**
 * Convert bgra vector to grayscale using luminance formula in Rec. 709
 *
 * It uses `0.2126 * R + 0.7152 * G + 0.0722 * B`. The numbers are multiplied by 256 to get integer
 * values for faster calculations
 *
 * Rec. 709 is used by ImageMagick, DaVinci Resolve, Adobe Premiere Pro, GIMP, and is almost
 * identical to sRGB, the most commonly used standard for RGB color.
 */
std::vector<uint8_t> grayscale(const std::vector<uint8_t> &src) {
    std::vector<uint8_t> out(src.size() / 4);
    for (size_t i = 0; i < out.size(); ++i) {
        size_t srcIdx = 4 * i;
        out[i] = (54 * src[srcIdx + 2] + 183 * src[srcIdx + 1] + 19 * src[srcIdx]) / 256;
    }
    return out;
}

std::string getText(const std::vector<uint8_t> &src, int width, int height) {
    tess.SetImage(src.data(), width, height, 1, width);
    char *textPtr = tess.GetUTF8Text();
    std::string text(textPtr);
    delete[] textPtr;

    std::replace(text.begin(), text.end(), '\n', ' ');
    return text;
}

/**
 * Gets the first `idealWords` amount of words from `inText`
 *
 * Words are anything a-z or A-Z
 * It only includes the first non-space character after each word (punctuation counts as a space)
 * The first and last characters are always letters in non-empty strings
 */
std::string getFirstWords(const std::string &inText, int idealWords = 3) {
    std::string outText;
    int words = 0;
    bool inWord = false;

    for (const char &c : inText) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            inWord = true;
            outText += c;
        } else if (inWord) {
            inWord = false;
            if ((++words) == idealWords)
                break;
            outText += c;
        }
    }

    if (outText.empty())
        return "";

    char c = outText.back();
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
        outText.pop_back();
    }

    return outText;
}

int main() {
    setupMouse();
    int initResult = initTesseract();
    if (initResult)
        return initResult;

    std::unordered_map<int, int> rememberedPairs;
    int pairID = 0;
    std::string text;

    std::array<int, 12> clickNumbers;
    size_t storeIdx = 0;

    bool exit = false;

    const std::vector<uint8_t> fullScreenshot = takeScreenshot();

    for (size_t i = 0; i < 12; ++i) {
        text = getFirstWords(getText(grayscale(crop(fullScreenshot, i)), cropWidth, cropHeight));

        auto itText = textData.find(text);
        if (itText != textData.end()) {
            pairID = itText->second;
        } else {
            std::cout << "Text for tile " << i << " is: " << text << "\n";
            exit = true;
        }
        // pairID = i / 2;
        // std::cout << "Pair id: " << pairID << '\n';
        std::unordered_map<int, int>::const_iterator it = rememberedPairs.find(pairID);
        if (it != rememberedPairs.end()) {
            // std::cout << "Clicking " << i << " and " << it->second << '\n';
            clickNumbers[storeIdx++] = it->second;
            clickNumbers[storeIdx++] = i;
            rememberedPairs.erase(pairID);
        } else {
            // std::cout << "Adding " << i << '\n';
            rememberedPairs.emplace(pairID, i);
        }
    }

    crop(fullScreenshot, 5);
    if (exit)
        return 1;

    for (const int &tile : clickNumbers) {
        click(tile);
        Sleep(160);
    }

    return 0;
}
