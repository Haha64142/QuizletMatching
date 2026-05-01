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
 * 1. resize to 100x100 (maybe even 64x64)
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
#include <iostream>
#include <stack>
#include <unordered_map>
#include <windows.h>

constexpr int screenWidth = 1920;
constexpr int screenHeight = 1080;

constexpr int cropWidth = 351;
constexpr int cropHeight = 248;

constexpr std::array<int, 12> cropPosX = {216, 590,  964, 1337, 216, 590,
                                      964, 1337, 216, 590,  964, 1337};
constexpr std::array<int, 12> cropPosY = {215, 215, 215, 215, 485, 485, 485, 485, 754, 754, 754, 754};

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

int main() {
    setupMouse();

    std::unordered_map<int, int> rememberedPairs;
    int pairID = 0;

    std::stack<int> clickNumbers;

    // TODO: Take main screenshot
    for (size_t i = 0; i < 12; ++i) {
        // TODO: crop tile 'i' and process
        // TODO: convert cropped section to pair id
        pairID = i / 2;
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

    for (size_t i = 0; i < 12; ++i) {
        click(clickNumbers.top());
        clickNumbers.pop();
        Sleep(150);
    }

    return 0;
}
