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
#include <emmintrin.h>
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
    {"Totalitarianism", 0},
    {"is any political system in which a citizen is", 0},
    {"Benito Mussolini", 1},
    {"One of Europe's first major dictators.In after", 1},
    {"One of Europe's first major dictators.In.after", 1}, // Sometimes messes up
    {"Fascism", 2},
    {"Refers to authoritarian political movement which ruled Italy under", 2},
    {"Vladimir Lenin", 3},
    {"Leader of the Bolshevik Party in Russia.Established the", 3},
    {"Joseph Stalin", 4},
    {"The Soviet dictator led the USSR through World War", 4},
    {"Adolf Hitler", 5},
    {"Leader of Nazi Germany during World War II", 5},
    {"Mein Kampf My Struggle", 6},
    {"Hitler's autobiography,in which Hitler calls for the", 6},
    {"Hitter's autobiography,in which Hitler calls for the", 6},
    {"Manchuria", 7},
    {"Resource-rich province in northern China.Japanese military leaders", 7},
    {"The Nye Committee", 8},
    {"Was responsible for documenting the huge profits that arms", 8},
    {"Neutrality Act of", 9},
    {"The act made it illegal for Americans to sell", 9},
    {"Francisco Franco", 10},
    {"led a rebellion in Spain,after the coalition of", 10},
    {"Emperor Hirohito", 11},
    {"The ruler of Japan during World War II", 11},
    {"General Tojo", 12},
    {"The military ruler of Japan", 12},
    {"Axis-Powers", 13},
    {"Together Germany,Italy,and Japan became known as the", 13},
    {"Sudetenland", 14},
    {"An area of Czechoslovakia with a large German-speaking", 14},
    {"Neville Chamberlain", 15},
    {"British prime minister who publicly promised to support France", 15},
    {"The Munich Conference", 16},
    {"On September Britain and France agreed to Hitler's", 16},
    {"Poland", 17},
    {"After the Munich conference,Hitler turned his sights on", 17},
    {"The Nazi-Soviet Nonaggression Pact", 18},
    {"Stalin agreed to the nonaggression pact with Germany because", 18},
    {"Blitzkrieg", 19},
    {"The Germans used a new type of warfare called", 19},
    {"The Germans used anew type of warfare called blitzkrieg", 19}, // Another mess up
    {"Rhineland", 20},
    {"Area west of the Rhine River in which Hitler", 20},
    {"Dunkirk", 21},
    {"A small town in northern France near the Belgian", 21},
    {"Vichy Regime", 22},
    {"The puppet government set up by Hitler in Southern", 22},
    {"Marshall Petain", 23},
    {"The head of the Vichy Regime", 23},
    {"Winston Churchill", 24},
    {"Prime minister of Britain,who replaced Neville Chamberlain.Churchill", 24},
    {"The Battle of Britain", 25},
    {"The air battle between Britain and Germany that began", 25},
    {"Royal Air Force R.A.F", 26},
    {"Britain's Air Force", 26},
    {"Hermann Goering", 27},
    {"Head of the German air force,called the Luftwaffe", 27},
    {"Luftwaffe", 28},
    {"Germany's Air Force", 28},
    {"The Nuremberg Laws", 29},
    {"Laws set up by the Nazis that took rights", 29},
    {"Gestapo", 30},
    {"The Nazi governments secret police", 30},
    {"Kristallnacht night of broken glass", 31},
    {"The anti-Jewish violence that erupted throughout Germany and", 31},
    {"Albert Einstein", 32},
    {"In the early s Albert Einstein was among the", 32},
    {"Holocaust", 33},
    {"Hitler's attempt to destroy the Jews in Europe", 33},
    {"Concentration Camps", 34},
    {"Detention centers where Jews and other undesirables\"were sent", 34},
    {"Detention centers where Jews and other undesirables'were sent", 34}, // Sometimes messes up
    {"Extermination Camps", 35},
    {"These were added to many concentration camps after the", 35},
    {"The Neutrality Act of", 36},
    {"Warring nations could buy weapons from the United States", 36},
    {"The Lend-Lease Act", 37},
    {"The United States would be able to lend or", 37},
    {"The Atlantic Charter", 38},
    {"It committed the two leaders to a postwar world", 38},
    {"Pearl Harbor", 39},
    {"The place where the Japanese tried to destroy the", 39},
    {"December", 40},
    {"The date that the Japanese bombed Pearl Harbor", 40},
    {"Admiral Yamamoto", 41},
    {"Japanese admiral that planned the sneak attack on Pearl", 41},
    {"", 42},
    {"Years of United States involvement in World War II", 42},
    {"Years of United States involvement in World War I", 42},
    {"Franklin D.Roosevelt FDR", 43},
    {"President of the United States through most of World", 43},
    {"Selective Service and Training Act", 44},
    {"First peacetime draft in the United States", 44},
    {"General George Marshall", 45},
    {"The chairman of the Joint Chiefs of Staff fought", 45},
    {"General Dwight Eisenhower", 46},
    {"The supreme allied commander in Europe", 46},
    {"Operation Torch", 47},
    {"The code name for the American invasion of North", 47},
    {"Erwin Rommel", 48},
    {"Germany's best field commander.Known as The Desert", 48},
    {"General Bernard Montgomery", 49},
    {"Great Britain's best field general", 49},
    {"General George Patton", 50},
    {"America's best field general.Known as Old Blood", 50},
    {"General Mark Clark", 51},
    {"General in charge of the Italian Campaign during World", 51},
    {"Operation Barbarossa", 52},
    {"The code name for the German invasion of the", 52},
    {"Stalingrad", 53},
    {"The German defeat at this city was the turning", 53},
    {"D-Day/Operation Overlord", 54},
    {"Codename for the Allied invasion of France", 54},
    {"June", 55},
    {"The date of the Allied invasion of France", 55},
    {"Battle of the Bulge", 56},
    {"The largest battle fought by the United States Forces", 56},
    {"Audie Murphy", 57},
    {"Served in the European Theatre,is the most decorated", 57},
    {"V-E Day", 58},
    {"Victory in Europe,May", 58},
    {"General Douglas MacArthur", 59},
    {"The supreme Allied commander in the Pacific", 59},
    {"Chester Nimitz", 60},
    {"Top Admiral in the United States Navy during World", 60},
    {"Coral Sea", 61},
    {"First naval battle ever fought using only airplanes;this", 61},
    {"Guadalcanal", 62},
    {"First time the American forces land on an island", 62},
    {"Island Hopping", 63},
    {"American strategy used to defeat Japan;Americans would only", 63},
    {"Midway", 64},
    {"The United States victory here was the turning point", 64},
    {"Leyte Gulf", 65},
    {"The victory in which the United States navy destroyed", 65},
    {"Kamikaze", 66},
    {"Japanese suicide planes,one plane one ship", 66},
    {"Asian Americans", 67},
    {"During World War II,the United States government placed", 67},
    {"Korematsu v.United States", 68},
    {"Supreme Court case that ruled relocation of Japanese Americans", 68},
    {"Bataan Death March", 69},
    {"Took place in the Philippines", 69},
    {"Iwo Jima/Okinawa", 70},
    {"lwo Jima/Okinawa", 70}, // Sometimes messes up 'Iwo'
    {"Because of the heavy casualties suffered on these islands", 70},
    {"Ernie Pyle", 71},
    {"Famous reporter for Stars and Stripes;he was killed", 71},
    {"Manhattan Project", 72},
    {"Code name for the development of the atomic bomb", 72},
    {"Dr.Oppenheimer", 73},
    {"The man in charge of the Manhattan Project", 73},
    {"Harry S Truman", 74},
    {"The American president that made the decision to drop", 74},
    {"USS Indianapolis", 75},
    {"Ship that delivered the atomic bomb to Tinian Island", 75},
    {"Enola Gay", 76},
    {"The B2bomber that dropped the first atomic bomb", 76},
    {"Hiroshima", 77},
    {"The city selected as the target for the first", 77},
    {"The Bock's Car", 78},
    {"The B2bomber that dropped the second atomic bomb", 78},
    {"Nagasaki", 79},
    {"The city selected as the target for the second", 79},
    {"USS Missouri", 80},
    {"Ship on which the Japanese surrendered at the end", 80},
    {"V-J Day", 81},
    {"Victory in Japan,September", 81},
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
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz,;'/ -.0123456789\"");
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

void ocrThread() {
    int pairID = 0;
    std::string text;

    COLORREF c;
    int r;
    int g;
    int b;
    while (true) {
        c = GetPixel(screenDC, 850, 800);
        if (!(GetRValue(c) == 0x1f && GetGValue(c) == 0x1c && GetBValue(c) == 0x8b))
            break;
        _mm_pause();
    }
    start = true;

    // timer.start();
    takeScreenshot();
    // timer.stop();
    // timer.printMicro("\n");

    for (size_t i = 0; i < 12; ++i) {
        // timer.start();
        text =
            getFirstWords(getText(grayscale(crop(screenshotBuffer, i)), cropWidth, cropHeight), 9);

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
    timer.stop();
    timer.printMilli("\n");

    tileIdByPair.fill(-1);
}

void clickThread() {
    // clickQueue.push(10);
    // clickQueue.push(11);

    while (!start) {
        _mm_pause();
    }

    timer.start();
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
