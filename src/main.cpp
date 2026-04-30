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

#include <iostream>

int cropWidth = 351;
int cropHeight = 248;

int cropPosX[12] = {216, 590, 964, 1337, 216, 590, 964, 1337, 216, 590, 964, 1337};
int cropPosY[12] = {215, 215, 215, 215, 485, 485, 485, 485, 754, 754, 754, 754};

int clickPosX[12] = {391, 760, 1134, 1512, 391, 760, 1134, 1512, 391, 760, 1134, 1512};
int clickPosY[12] = {339, 339, 339, 339, 608, 608, 608, 608, 881, 881, 881, 881};

int main() {
    std::cout << "Hello World\n";
    return 0;
}
