/* Main program:
 * 1. take full screenshot
 * 2. crop image and process it
 * 3. convert crop to pair id using tesseract
 * 4. check if pair id exists in remembered pairs
 * 5. if yes, click the current tile and the tile in remembered pairs
 * 6. remove the pair id from remebered pairs
 * 7. else, add the pair id and tile id to remembered pairs
 * 8. repeat 2-7 changing the cropped section (but keep the same full screenshot)
 *
 * Image processing:
 * 1. grayscale
 * 2. ocr using tesseract
 * 3. take only the first 3 words
 * 4. use unordered map for lookup
 *
 * Variables and Definitions:
 * tile number: 0-11 representing the physical location of the tile on the screen
 * pair id: 0-46 representing each term + definition pair
 * tile hash: specific hash for each tile (94 unique tiles)
 *
 * click positions: list - index: tile number, value: coord to click
 * crop positions: list - index: tile number, value: top corner of the cropped tile
 * remembered pairs: unordered_map - key: pair id, value: tile number
 * text data: unordered_map - key: first 3 words of tile, value: pair id
 * */
#include <iostream>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <immintrin.h>
#include <string.h>

#include <array>
#include <cstdint>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <tesseract/baseapi.h>
#include <wingdi.h>

class Timer {
  private:
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> endTime;

  public:
    void start() { startTime = std::chrono::high_resolution_clock::now(); }
    void stop() { endTime = std::chrono::high_resolution_clock::now(); }
    void printMicro(const std::string &suffix = "us") {
        std::chrono::duration<double, std::micro> elapsed = endTime - startTime;
        std::cout << elapsed.count() << suffix;
    }
    void printMilli(const std::string &suffix = "ms") {
        std::chrono::duration<double, std::milli> elapsed = endTime - startTime;
        std::cout << elapsed.count() << suffix;
    }
};

constexpr int screenWidth = 1920;
constexpr int screenHeight = 1080;

constexpr int cropWidth = 352;
constexpr int cropHeight = 120;

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

const int threshold = 192 * 256;

const int START_HOTKEY_ID = 0;
const int EXIT_HOTKEY_ID = 1;

std::queue<int> clickQueue;
std::mutex queueMutex;
std::condition_variable cv;

std::atomic_bool start = false;
std::atomic_bool finished = false;

std::vector<uint8_t> screenshotBuffer(screenWidth * screenHeight * 4);
std::vector<uint8_t> cropBuffer(cropWidth * cropHeight * 4);
std::vector<uint8_t> grayBuffer(cropWidth * cropHeight * 1);

// Need to update size by hand. It's the highest pairID + 1
std::array<int, 82> tileIdByPair;

INPUT mouse[2] = {};

HDC screenDC = GetDC(NULL);
HDC memoryDC = CreateCompatibleDC(screenDC);
HBITMAP targetBitmap = CreateCompatibleBitmap(screenDC, screenWidth, screenHeight);
HGDIOBJ oldObj = SelectObject(memoryDC, targetBitmap);

BITMAPINFO bmi{};

tesseract::TessBaseAPI tess;

Timer timer;

const std::unordered_map<std::string, int> textData = {
    {"totalitarianism", 0},
    {"isanypoliticalsysteminwhichacitiz", 0},
    {"benitomussolini", 1},
    {"oneofeuropesfirstmajordictatorsin", 1},
    {"fascism", 2},
    {"referstoauthoritarianpoliticalmov", 2},
    {"vladimirlenin", 3},
    {"leaderofthebolshevikpartyinrussia", 3},
    {"josephstalin", 4},
    {"thesovietdictatorledtheussrthroug", 4},
    {"adolfhitler", 5},
    {"leaderofnazigermanyduringworldwar", 5},
    {"meinkampfmystruggle", 6},
    {"hitlersautobiographyinwhichhitler", 6},
    {"manchuria", 7},
    {"resourcerichprovinceinnorthernchi", 7},
    {"thenyecommittee", 8},
    {"wasresponsiblefordocumentingthehu", 8},
    {"neutralityactof1935", 9},
    {"theactmadeitillegalforamericansto", 9},
    {"franciscofranco", 10},
    {"ledarebellioninspainafterthecoali", 10},
    {"emperorhirohito", 11},
    {"therulerofjapanduringworldwarii", 11},
    {"generaltojo", 12},
    {"themilitaryrulerofjapan", 12},
    {"axispowers", 13},
    {"togethergermanyitalyandjapanbecam", 13},
    {"sudetenland", 14},
    {"anareaofczechoslovakiawithalargeg", 14},
    {"nevillechamberlain", 15},
    {"britishprimeministerwhopubliclypr", 15},
    {"themunichconference", 16},
    {"onseptember291938britainandfrance", 16},
    {"poland", 17},
    {"afterthemunichconferencehitlertur", 17},
    {"thenazisovietnonaggressionpact", 18},
    {"stalinagreedtothenonaggressionpac", 18},
    {"blitzkrieg", 19},
    {"thegermansusedanewtypeofwarfareca", 19},
    {"rhineland", 20},
    {"areawestoftherhineriverinwhichhit", 20},
    {"dunkirk", 21},
    {"asmalltowninnorthernfrancenearthe", 21},
    {"vichyregime", 22},
    {"thepuppetgovernmentsetupbyhitleri", 22},
    {"marshallpetain", 23},
    {"theheadofthevichyregime", 23},
    {"winstonchurchill", 24},
    {"primeministerofbritainwhoreplaced", 24},
    {"thebattleofbritain", 25},
    {"theairbattlebetweenbritainandgerm", 25},
    {"royalairforceraf", 26},
    {"britainsairforce", 26},
    {"hermanngoering", 27},
    {"headofthegermanairforcecalledthel", 27},
    {"luftwaffe", 28},
    {"germanysairforce", 28},
    {"thenuremberglaws", 29},
    {"lawssetupbythenazisthattookrights", 29},
    {"gestapo", 30},
    {"thenazigovernmentssecretpolice", 30},
    {"kristallnachtnightofbrokenglass", 31},
    {"theantijewishviolencethateruptedt", 31},
    {"alberteinstein", 32},
    {"intheearly1930salberteinsteinwasa", 32},
    {"holocaust", 33},
    {"hitlersattempttodestroythejewsine", 33},
    {"concentrationcamps", 34},
    {"detentioncenterswherejewsandother", 34},
    {"exterminationcamps", 35},
    {"thesewereaddedtomanyconcentration", 35},
    {"theneutralityactof1939", 36},
    {"warringnationscouldbuyweaponsfrom", 36},
    {"thelendleaseact", 37},
    {"theunitedstateswouldbeabletolendo", 37},
    {"theatlanticcharter", 38},
    {"itcommittedthetwoleaderstoapostwa", 38},
    {"pearlharbor", 39},
    {"theplacewherethejapanesetriedtode", 39},
    {"december71941", 40},
    {"thedatethatthejapanesebombedpearl", 40},
    {"admiralyamamoto", 41},
    {"japaneseadmiralthatplannedthesnea", 41},
    {"19411945", 42},
    {"yearsofunitedstatesinvolvementinw", 42},
    {"franklindrooseveltfdr", 43},
    {"presidentoftheunitedstatesthrough", 43},
    {"selectiveserviceandtrainingact", 44},
    {"firstpeacetimedraftintheunitedsta", 44},
    {"generalgeorgemarshall", 45},
    {"thechairmanofthejointchiefsofstaf", 45},
    {"generaldwighteisenhower", 46},
    {"thesupremealliedcommanderineurope", 46},
    {"operationtorch", 47},
    {"thecodenamefortheamericaninvasion", 47},
    {"erwinrommel", 48},
    {"germanysbestfieldcommanderknownas", 48},
    {"generalbernardmontgomery", 49},
    {"greatbritainsbestfieldgeneral", 49},
    {"generalgeorgepatton", 50},
    {"americasbestfieldgeneralknownasol", 50},
    {"generalmarkclark", 51},
    {"generalinchargeoftheitaliancampai", 51},
    {"operationbarbarossa", 52},
    {"thecodenameforthegermaninvasionof", 52},
    {"stalingrad", 53},
    {"thegermandefeatatthiscitywasthetu", 53},
    {"ddayoperationoverlord", 54},
    {"codenameforthealliedinvasionoffra", 54},
    {"june61944", 55},
    {"thedateofthealliedinvasionoffranc", 55},
    {"battleofthebulge", 56},
    {"thelargestbattlefoughtbytheunited", 56},
    {"audiemurphy", 57},
    {"servedintheeuropeantheatreisthemo", 57},
    {"veday", 58},
    {"victoryineuropemay81945", 58},
    {"generaldouglasmacarthur", 59},
    {"thesupremealliedcommanderinthepac", 59},
    {"chesternimitz", 60},
    {"topadmiralintheunitedstatesnavydu", 60},
    {"coralsea", 61},
    {"firstnavalbattleeverfoughtusingon", 61},
    {"guadalcanal", 62},
    {"firsttimetheamericanforceslandona", 62},
    {"islandhopping", 63},
    {"americanstrategyusedtodefeatjapan", 63},
    {"midway", 64},
    {"theunitedstatesvictoryherewasthet", 64},
    {"leytegulf", 65},
    {"thevictoryinwhichtheunitedstatesn", 65},
    {"kamikaze", 66},
    {"japanesesuicideplanesoneplaneones", 66},
    {"asianamericans", 67},
    {"duringworldwariitheunitedstatesgo", 67},
    {"korematsuvunitedstates", 68},
    {"supremecourtcasethatruledrelocati", 68},
    {"bataandeathmarch", 69},
    {"tookplaceinthephilippines", 69},
    {"iwojimaokinawa", 70},
    {"becauseoftheheavycasualtiessuffer", 70},
    {"erniepyle", 71},
    {"famousreporterforstarsandstripesh", 71},
    {"manhattanproject", 72},
    {"codenameforthedevelopmentoftheato", 72},
    {"droppenheimer", 73},
    {"themaninchargeofthemanhattanproje", 73},
    {"harrystruman", 74},
    {"theamericanpresidentthatmadethede", 74},
    {"ussindianapolis", 75},
    {"shipthatdeliveredtheatomicbombtot", 75},
    {"enolagay", 76},
    {"theb29bomberthatdroppedthefirstat", 76},
    {"hiroshima", 77},
    {"thecityselectedasthetargetforthef", 77},
    {"thebockscar", 78},
    {"theb29bomberthatdroppedtheseconda", 78},
    {"nagasaki", 79},
    {"thecityselectedasthetargetforthes", 79},
    {"ussmissouri", 80},
    {"shiponwhichthejapanesesurrendered", 80},
    {"vjday", 81},
    {"victoryinjapanseptember21945", 81},
};

inline void sleep_ms(int millis) { std::this_thread::sleep_for(std::chrono::milliseconds(millis)); }

void setupMouse() {
    mouse[0].type = INPUT_MOUSE;
    mouse[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    // mouse[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;

    mouse[1].type = INPUT_MOUSE;
    mouse[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
}

int initTesseract() {
    int res = tess.Init(NULL, "eng");
    if (res) {
        std::cerr << "Could not initialize Tesseract\n";
        return res;
    }
    tess.SetPageSegMode(tesseract::PageSegMode::PSM_SINGLE_BLOCK);
    tess.SetVariable("tessedit_char_whitelist",
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    return 0;
}

int setupHotkeys() {
    if (!RegisterHotKey(NULL, START_HOTKEY_ID, MOD_ALT | MOD_NOREPEAT, 'R')) {
        std::cerr << "Failed to register hotkey: START_HOTKEY\n";
        return 1;
    }

    if (!RegisterHotKey(NULL, EXIT_HOTKEY_ID, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'C')) {
        std::cerr << "Failed to register hotkey: EXIT_HOTKEY";
        return 1;
    }

    return 0;
}

int setupScreenshot() {
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = screenWidth;
    bmi.bmiHeader.biHeight = -screenHeight;

    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    return 0;
}

void click(int tileNumber) {
    mouse[0].mi.dx = clickPosX[tileNumber];
    mouse[0].mi.dy = clickPosY[tileNumber];

    SendInput(2, mouse, sizeof(INPUT));
}

void takeScreenshot() {
    BitBlt(memoryDC, 0, 0, screenWidth, screenHeight, screenDC, 0, 0, SRCCOPY);

    GetDIBits(memoryDC, targetBitmap, 0, screenHeight, screenshotBuffer.data(), &bmi,
              DIB_RGB_COLORS);

    // Copy screenshot to clipboard for debugging
    // size_t headerSize = sizeof(BITMAPINFOHEADER);
    // size_t pixelSize = screenshotBuffer.size();
    // size_t totalSize = headerSize + pixelSize;
    //
    // HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, totalSize);
    //
    // void *dest = GlobalLock(hMem);
    //
    // memcpy(dest, &bmi.bmiHeader, headerSize);
    // memcpy(static_cast<uint8_t *>(dest) + headerSize, screenshotBuffer.data(), pixelSize);
    //
    // GlobalUnlock(hMem);
    //
    // OpenClipboard(NULL);
    // EmptyClipboard();
    // SetClipboardData(CF_DIB, hMem);
    // CloseClipboard();
}

std::vector<uint8_t> crop(const std::vector<uint8_t> &src, int srcWidth, int srcHeight, int x,
                          int y, int cropWidth, int cropHeight) {
    for (int row = 0; row < cropHeight; ++row) {
        int srcIdx = ((y + row) * srcWidth + x) * 4;
        int destIdx = row * cropWidth * 4;

        memcpy(&cropBuffer[destIdx], &src[srcIdx], cropWidth * 4);
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
    // size_t pixelSize = cropBuffer.size();
    // size_t totalSize = headerSize + pixelSize;
    //
    // HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, totalSize);
    //
    // void *dest = GlobalLock(hMem);
    //
    // memcpy(dest, &bmi.bmiHeader, headerSize);
    // memcpy(static_cast<uint8_t *>(dest) + headerSize, cropBuffer.data(), pixelSize);
    //
    // GlobalUnlock(hMem);
    //
    // OpenClipboard(NULL);
    // EmptyClipboard();
    // SetClipboardData(CF_DIB, hMem);
    // CloseClipboard();

    return cropBuffer;
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
    for (size_t i = 0; i < grayBuffer.size(); ++i) {
        size_t srcIdx = 4 * i;
        grayBuffer[i] =
            (54 * src[srcIdx + 2] + 183 * src[srcIdx + 1] + 19 * src[srcIdx]) < threshold ? 0 : 255;
    }

    // Copy to clipboard for debugging
    // BITMAPINFO bmi{};
    // bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    // bmi.bmiHeader.biWidth = cropWidth;
    // bmi.bmiHeader.biHeight = -cropHeight;
    //
    // bmi.bmiHeader.biPlanes = 1;
    // bmi.bmiHeader.biBitCount = 8;
    // bmi.bmiHeader.biCompression = BI_RGB;
    // bmi.bmiHeader.biClrUsed = 256;
    //
    // RGBQUAD palette[256];
    // for (size_t i = 0; i < 256; ++i) {
    //     palette[i].rgbRed = i;
    //     palette[i].rgbGreen = i;
    //     palette[i].rgbBlue = i;
    //     palette[i].rgbReserved = 0;
    // }
    //
    // size_t headerSize = sizeof(BITMAPINFOHEADER);
    // size_t paletteSize = sizeof(palette);
    // size_t pixelSize = grayBuffer.size();
    // size_t totalSize = headerSize + paletteSize + pixelSize;
    //
    // HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, totalSize);
    //
    // void *dest = GlobalLock(hMem);
    //
    // memcpy(dest, &bmi.bmiHeader, headerSize);
    // memcpy(static_cast<uint8_t *>(dest) + headerSize, palette, paletteSize);
    // memcpy(static_cast<uint8_t *>(dest) + headerSize + paletteSize, grayBuffer.data(),
    // pixelSize);
    //
    // GlobalUnlock(hMem);
    //
    // OpenClipboard(NULL);
    // EmptyClipboard();
    // SetClipboardData(CF_DIB, hMem);
    // CloseClipboard();

    return grayBuffer;
}

std::string getText(const std::vector<uint8_t> &src, int width, int height) {
    tess.SetImage(src.data(), width, height, 1, width);
    char *textPtr = tess.GetUTF8Text();
    std::string text(textPtr);
    delete[] textPtr;

    text.erase(std::remove_if(text.begin(), text.end(), isspace), text.end());
    text = text.substr(0, 33);
    for (char &c : text) {
        c |= 0b00100000; // Flip the 5th bit to a 1, which in ASCII, changes uppercase to lowercase
    }
    return text;
}

void ocrThread() {
    int pairID = 0;
    std::string text;

    while (!start) {
        _mm_pause();
    }
    timer.start();

    // timer.start();
    takeScreenshot();
    // timer.stop();
    // timer.printMicro("\n");

    for (size_t i = 0; i < 10; ++i) {
        // timer.start();
        text = getText(grayscale(crop(screenshotBuffer, i)), cropWidth, cropHeight);

        auto itText = textData.find(text);
        if (itText != textData.end()) {
            pairID = itText->second;
        } else {
            std::cout << "Text for tile " << i << " is: " << text << "\n";
        }

        if (tileIdByPair[pairID] == -1) {
            tileIdByPair[pairID] = i;
        } else {
            {
                std::lock_guard<std::mutex> lock(queueMutex);

                clickQueue.push(tileIdByPair[pairID]);
                clickQueue.push(i);
            }
            cv.notify_one();
        }
        // timer.stop();
        // timer.print("\n");
    }

    finished = true;

    tileIdByPair.fill(-1);
}

void clickThread() {
    clickQueue.push(10);
    clickQueue.push(11);

    while (GetRValue(GetPixel(screenDC, 850, 800)) == 0x1f) {
        _mm_pause();
    }
    start = true;

    int tile;
    std::cout << "Clicking: \n";
    // bool timed = false;

    for (size_t i = 0; true; ++i) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);

            cv.wait(lock, [] { return !clickQueue.empty() || finished; });

            if (clickQueue.empty() && finished)
                break;

            tile = clickQueue.front();
            clickQueue.pop();
        }

        // if (!timed) {
        //     timed = true;
        // }
        click(tile);
        // std::cout << tile << ", ";
        if (i % 2) {
            sleep_ms(160);
        }
    }
    timer.stop();
    timer.printMilli("\n");

    finished = false;
    start = false;
    // timed = true;
    std::cout << "\nDone\n\n";
}

int main() {
    setupMouse();
    int initResult = initTesseract();
    if (initResult)
        return initResult;
    if (setupHotkeys())
        return 1;
    if (setupScreenshot())
        return 1;
    tileIdByPair.fill(-1);

    std::cout << "Press ALT + R to start the quizlet matching\n"
                 "Press CTRL + ALT + C to exit\n\n";

    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_HOTKEY) {
            if (msg.wParam == START_HOTKEY_ID) {
                std::thread producer(ocrThread);
                std::thread consumer(clickThread);

                producer.join();
                consumer.join();
            } else if (msg.wParam == EXIT_HOTKEY_ID) {
                break;
            }
        }
    }

    UnregisterHotKey(NULL, START_HOTKEY_ID);
    UnregisterHotKey(NULL, EXIT_HOTKEY_ID);
    SelectObject(memoryDC, oldObj);
    DeleteObject(targetBitmap);
    DeleteDC(memoryDC);
    ReleaseDC(NULL, screenDC);
    tess.End();
    return 0;
}
