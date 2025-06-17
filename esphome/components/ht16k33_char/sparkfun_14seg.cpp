#include <unordered_map>
#include "esphome/core/log.h"
#include "sparkfun_14seg.h"

/******************************************************************************
 *Sparkfun 14 segment QWIIC displays
 *  Product Link: https://www.sparkfun.com/sparkfun-qwiic-alphanumeric-display-red.html
 *
 *  Device specific functions for the Sparkfun 14 segment displays. Both right-side-up and
 *  upside-down orientations are supported. To use these in your device, use device type
 *  'SPARKFUN_14_SEG' or 'SPARKFUN_14_SEG_FLIPPED'. These devices can display pretty much all
 *  of the ASCII characters. If you ask it to print an unsupported character, that digit on the
 *  display will be left blank.
 *
 *  Schematic: https://cdn.sparkfun.com/assets/c/7/2/8/a/Qwiic_Alphanumeric_Display.pdf
 *  Display Datasheet: https://cdn.sparkfun.com/assets/c/8/7/2/5/VK16K33Datasheet.pdf
 *
 *****************************************************************************/

namespace esphome {
namespace ht16k33_char {

static const char *const TAG = "ht16k33_char";

// Position is the index in the character buffer of the first digit to display. position 0 is the
//  begining of the buffer Returns the index of the first character to display on the next display.
//  (what we would give as `position` to the next call to this function).
uint8_t Sparkfun14Seg::send_to_display(i2c::I2CDevice *display, uint8_t position) {
  uint8_t i;
  char char_to_find;
  bool special_character_found;
  // Sparkfun 14-Segment Font
  const std::unordered_map<char, uint16_t> char_map = {
      {' ', 0b0000000000000000}, {'!', 0b0000000000000110}, {'"', 0b0000010000100000}, {'#', 0b0010010101001110},
      {'$', 0b0010010101101101}, {'%', 0b0100100000100100}, {'&', 0b0001011001011101}, {'\'', 0b0000100000000000},
      {'(', 0b0001100000000000}, {')', 0b0100001000000000}, {'*', 0b0111111101000000}, {'+', 0b0010010101000000},
      {',', 0b0100000000000000}, {'-', 0b0000000101000000}, {'/', 0b0100100000000000}, {'0', 0b0100100000111111},
      {'1', 0b0000000000000110}, {'2', 0b0000000101011011}, {'3', 0b0000000100001111}, {'4', 0b0000000101100110},
      {'5', 0b0001000001101001}, {'6', 0b0000000101111101}, {'7', 0b0010100000000001}, {'8', 0b0000000101111111},
      {'9', 0b0000000101101111}, {':', 0b0010010000000000}, {';', 0b0100010000000000}, {'<', 0b0001100000000000},
      {'=', 0b0000000101001000}, {'>', 0b0100001000000000}, {'?', 0b0010000100000011}, {'@', 0b0000010100111011},
      {'A', 0b0000000101110111}, {'B', 0b0010010100001111}, {'C', 0b0000000000111001}, {'D', 0b0010010000001111},
      {'E', 0b0000000101111001}, {'F', 0b0000000001110001}, {'G', 0b0000000100111101}, {'H', 0b0000000101110110},
      {'I', 0b0010010000000000}, {'J', 0b0000000000011110}, {'K', 0b0001100001110000}, {'L', 0b0000000000111000},
      {'M', 0b0000101000110110}, {'N', 0b0001001000110110}, {'O', 0b0000000000111111}, {'P', 0b0000000101110011},
      {'Q', 0b0001000000111111}, {'R', 0b0001000101110011}, {'S', 0b0000000101101101}, {'T', 0b0010010000000001},
      {'U', 0b0000000000111110}, {'V', 0b0100100000110000}, {'W', 0b0101000000110110}, {'X', 0b0101101000000000},
      {'Y', 0b0010101000000000}, {'Z', 0b0100100000001001}, {'[', 0b0000000000111001}, {'\\', 0b0001001000000000},
      {']', 0b0000000000001111}, {'^', 0b0000000000100011}, {'_', 0b0000000000001000}, {'`', 0b0000001000000000},
      {'a', 0b0010000001011000}, {'b', 0b0001000001111000}, {'c', 0b0000000101011000}, {'d', 0b0100000100001110},
      {'e', 0b0100000001011000}, {'f', 0b0000000001110001}, {'g', 0b0000001100001111}, {'h', 0b0010000001110000},
      {'i', 0b0010000000000000}, {'j', 0b0000000000001110}, {'k', 0b0011110000000000}, {'l', 0b0000000000110000},
      {'m', 0b0010000101010100}, {'n', 0b0010000001010000}, {'o', 0b0000000101011100}, {'p', 0b0000100001110001},
      {'q', 0b0001000101100011}, {'r', 0b0000000001010000}, {'s', 0b0001000100001000}, {'t', 0b0000000001111000},
      {'u', 0b0000000000011100}, {'v', 0b0001000000000100}, {'w', 0b0101000000010100}, {'x', 0b0101000101000000},
      {'y', 0b0000010100001110}, {'z', 0b0100000001001000}, {'{', 0b0100001001001001}, {'|', 0b0010010000000000},
      {'}', 0b0001100100001001}, {'~', 0b0000000101000000},
  };

  uint8_t char_buffer_location;

  this->buffer_[0] = HT16K33_DISPLAY_DATA_ADDRESS;

  // Clear any old data from the buffer
  for (int i = 1; i < 16; i++) {
    this->buffer_[i] = 0x00;
  }

  char_buffer_location = position;
  i = 0;
  special_character_found = false;

  // In this while loop, `i` represents the digit that will display the character. We count through
  // the four digits in the display and set them to the next four characters in the character buffer.
  while (i < 4) {
    if (char_buffer_location >= this->char_buffer_.length()) {
      // char_buffer_location is past the end of the character buffer.
      if (this->continuous_) {
        // We want a continuous display where the message starts over immediately.
        char_buffer_location = 0;
      } else {
        // Blank the digits past the end of the display.
        this->write_to_buffer_(0x0000, i);
        i++;
      }
    }

    else {
      // The character to find is within the bounds of the buffer array.
      char_to_find = this->char_buffer_.at(char_buffer_location);

      // Look for special characters. For this display, there is a colon between digit one and two,
      //  and a period after digit two. The display will try (badly) to display a colon if it is
      //  placed in any other location. This causes scrolling with a colon to look wierd. This seems
      //  like a very edge case, so I did not try to fix it. A period in any other location will
      //  display as blank.
      if (!special_character_found) {
        if ((char_to_find == ':') && (i == 2)) {
          // Colon at position 3
          special_character_found = true;
          char_buffer_location++;
          this->buffer_[2] |= 0x01;
          continue;
        } else if ((char_to_find == '.') && (i == 3)) {
          // Period at position 4
          special_character_found = true;
          char_buffer_location++;
          this->buffer_[4] |= 0x01;
          continue;
        }
      }

      auto it = char_map.find(char_to_find);
      if (it != char_map.end()) {
        this->write_to_buffer_((it->second), i);
        special_character_found = false;
        i++;
      } else {
        // Digit is not in the map. Blank the digit.
        this->write_to_buffer_(0x0000, i);
        special_character_found = false;
        i++;
      }

      char_buffer_location++;
    }
  }

  display->write(this->buffer_, 16, true);
  return char_buffer_location;
}

// Write a character at position 'char_position' to the memory buffer.
void Sparkfun14Seg::write_to_buffer_(uint16_t char_to_write, uint8_t char_position) {
  // char_position should be 0-3
  if ((char_position >= 0) && (char_position <= 3)) {
    for (uint8_t i = 0; i < 8; i++) {
      // i counts through the com positions
      this->buffer_[i * 2 + 1] |= ((char_to_write >> i) & 0x01) << (char_position);
      this->buffer_[i * 2 + 1] |= ((char_to_write >> (i + 8)) & 0x01) << (char_position + 4);
    }
  }
}

// Position is the position in the character buffer. position 0 is the begining of the buffer
// Returns the index of the first character to display in the buffer (what we would give as `position` to the next call
// to this function).
uint8_t Sparkfun14SegFlip::send_to_display(i2c::I2CDevice *display, uint8_t position) {
  uint8_t i;
  char char_to_find;
  bool special_character_found;
  // Sparkfun 14-Segment Font, Flipped
  const std::unordered_map<char, uint16_t> char_map = {
      {' ', 0b0000000000000000}, {'!', 0b0000000000110000}, {'"', 0b0010000000000100}, {'#', 0b0010010101110001},
      {'$', 0b0010010101101101}, {'%', 0b0100100000100100}, {'&', 0b0011001100101011}, {'\'', 0b0100000000000000},
      {'(', 0b0100001000000000}, {')', 0b0001100000000000}, {'*', 0b0111111101000000}, {'+', 0b0010010101000000},
      {',', 0b0000100000000000}, {'-', 0b0000000101000000}, {'/', 0b0100100000000000}, {'0', 0b0100100000111111},
      {'1', 0b0000000000110000}, {'2', 0b0000000101011011}, {'3', 0b0000000001111001}, {'4', 0b0000000101110100},
      {'5', 0b0000001100001101}, {'6', 0b0000000101101111}, {'7', 0b0100010000001000}, {'8', 0b0000000101111111},
      {'9', 0b0000000101111101}, {':', 0b0010010000000000}, {';', 0b0010100000000000}, {'<', 0b0100001000000000},
      {'=', 0b0000000101000001}, {'>', 0b0001100000000000}, {'?', 0b0000010001011000}, {'@', 0b0010000001011111},
      {'A', 0b0000000101111110}, {'B', 0b0010010001111001}, {'C', 0b0000000000001111}, {'D', 0b0010010000111001},
      {'E', 0b0000000101001111}, {'F', 0b0000000100001110}, {'G', 0b0000000001101111}, {'H', 0b0000000101110110},
      {'I', 0b0010010000000000}, {'J', 0b0000000000110011}, {'K', 0b0100001100000110}, {'L', 0b0000000000000111},
      {'M', 0b0101000000110110}, {'N', 0b0001001000110110}, {'O', 0b0000000000111111}, {'P', 0b0000000101011110},
      {'Q', 0b0000001000111111}, {'R', 0b0000001101011110}, {'S', 0b0000000101101101}, {'T', 0b0010010000001000},
      {'U', 0b0000000000110111}, {'V', 0b0100100000000110}, {'W', 0b0000101000110110}, {'X', 0b0101101000000000},
      {'Y', 0b0101010000000000}, {'Z', 0b0100100000001001}, {'[', 0b0000000000001111}, {'\\', 0b0001001000000000},
      {']', 0b0000000000111001}, {'^', 0b0000000000011100}, {'_', 0b0000000000000001}, {'`', 0b0001000000000000},
      {'a', 0b0000010100000011}, {'b', 0b0000001100000111}, {'c', 0b0000000101000011}, {'d', 0b0000100001110001},
      {'e', 0b0000100100000011}, {'f', 0b0000000100001110}, {'g', 0b0001000001111001}, {'h', 0b0000010100000110},
      {'i', 0b0000010000000000}, {'j', 0b0000000000110001}, {'k', 0b0110011000000000}, {'l', 0b0000000000000110},
      {'m', 0b0000010101100010}, {'n', 0b0000010100000010}, {'o', 0b0000000101100011}, {'p', 0b0100000100001110},
      {'q', 0b0000001101011100}, {'r', 0b0000000100000010}, {'s', 0b0000001001000001}, {'t', 0b0000000100000111},
      {'u', 0b0000000000100011}, {'v', 0b0000001000100000}, {'w', 0b0000101000100010}, {'x', 0b0000101101000000},
      {'y', 0b0010000001110001}, {'z', 0b0000100100000001}, {'{', 0b0001100100001001}, {'|', 0b0010010000000000},
      {'}', 0b0100001001001001}, {'~', 0b0000000101000000},
  };

  uint8_t char_buffer_location;

  this->buffer_[0] = HT16K33_DISPLAY_DATA_ADDRESS;

  // Clear any old data from the buffer
  for (int i = 1; i < 16; i++) {
    this->buffer_[i] = 0x00;
  }

  char_buffer_location = position;
  i = 0;
  special_character_found = false;

  // In this while loop, `i` represents the digit that will display the character. We count through
  // the four digits in the display and set them to the next four characters in the character buffer.
  while (i < 4) {
    if (char_buffer_location >= this->char_buffer_.length()) {
      // char_buffer_location is past the end of the character buffer.
      if (this->continuous_) {
        // We want a continuous display where the message starts over immediately.
        char_buffer_location = 0;
      } else {
        // Blank the digits past the end of the display.
        this->write_to_buffer_(0x0000, i);
        i++;
      }
    }

    else {
      // The character to find is within the bounds of the buffer array.
      char_to_find = this->char_buffer_.at(char_buffer_location);

      // Look for special characters. For this flipped display, there is a colon between digit one
      //  and two, and a period at the top of the display between digit zero and one. The display
      //  will try (badly) to display a colon if it is placed in any other location. This causes
      //  scrolling with a colon to look wierd. This seems like a very edge case, so I did not try
      //  to fix it. The period at the top of the display is not implemented.
      if (!special_character_found) {
        if ((char_to_find == ':') && (i == 2)) {
          // Colon at position 3
          special_character_found = true;
          char_buffer_location++;
          this->buffer_[2] |= 0x01;
          continue;
        }
      }

      auto it = char_map.find(char_to_find);
      if (it != char_map.end()) {
        this->write_to_buffer_((it->second), i);
        special_character_found = false;
        i++;
      } else {
        // Digit is not in the map. Blank the digit.
        this->write_to_buffer_(0x0000, i);
        special_character_found = false;
        i++;
      }

      char_buffer_location++;
    }
  }

  display->write(this->buffer_, 16, true);
  return char_buffer_location;
}

// Write a character at position 'char_position' to the memory buffer.
//  Note that for this flipped device, char_position is the logical position of the character.
//  For example, char_position = 0 is the left most character on the display. char_position is
//  converted in this function to correctly place the digits on the flipped display.
void Sparkfun14SegFlip::write_to_buffer_(uint16_t char_to_write, uint8_t char_position) {
  // char_position should be 0-3
  if ((char_position >= 0) && (char_position <= 3)) {
    uint8_t flipped_char_position = 3 - char_position;

    for (uint8_t i = 0; i < 8; i++) {
      // i counts through the com positions
      this->buffer_[i * 2 + 1] |= ((char_to_write >> i) & 0x01) << (flipped_char_position);
      this->buffer_[i * 2 + 1] |= ((char_to_write >> (i + 8)) & 0x01) << (flipped_char_position + 4);
    }
  }
}

}  // namespace ht16k33_char
}  // namespace esphome
