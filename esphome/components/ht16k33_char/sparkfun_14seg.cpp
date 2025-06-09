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
  const std::unordered_map<char, uint16_t> char_map = {
      {' ', 0b0000000000000000}, {'!', 0b0000000000000110}, {'"', 0b0000001000100000}, {'#', 0b0001001011001110},
      {'$', 0b0001001011101101}, {'%', 0b0000110000100100}, {'&', 0b0010001101011101}, {'\'', 0b0000010000000000},
      {'(', 0b0010010000000000}, {')', 0b0000100100000000}, {'*', 0b0011111111000000}, {'+', 0b0001001011000000},
      {',', 0b0000100000000000}, {'-', 0b0000000011000000}, {'/', 0b0000110000000000}, {'0', 0b0000110000111111},
      {'1', 0b0000000000000110}, {'2', 0b0000000011011011}, {'3', 0b0000000010001111}, {'4', 0b0000000011100110},
      {'5', 0b0010000001101001}, {'6', 0b0000000011111101}, {'7', 0b0001010000000001}, {'8', 0b0000000011111111},
      {'9', 0b0000000011101111}, {':', 0b0001001000000000}, {';', 0b0000101000000000}, {'<', 0b0010010000000000},
      {'=', 0b0000000011001000}, {'>', 0b0000100100000000}, {'?', 0b0001000010000011}, {'@', 0b0000001010111011},
      {'A', 0b0000000011110111}, {'B', 0b0001001010001111}, {'C', 0b0000000000111001}, {'D', 0b0001001000001111},
      {'E', 0b0000000011111001}, {'F', 0b0000000001110001}, {'G', 0b0000000010111101}, {'H', 0b0000000011110110},
      {'I', 0b0001001000000000}, {'J', 0b0000000000011110}, {'K', 0b0010010001110000}, {'L', 0b0000000000111000},
      {'M', 0b0000010100110110}, {'N', 0b0010000100110110}, {'O', 0b0000000000111111}, {'P', 0b0000000011110011},
      {'Q', 0b0010000000111111}, {'R', 0b0010000011110011}, {'S', 0b0000000011101101}, {'T', 0b0001001000000001},
      {'U', 0b0000000000111110}, {'V', 0b0000110000110000}, {'W', 0b0010100000110110}, {'X', 0b0010110100000000},
      {'Y', 0b0001010100000000}, {'Z', 0b0000110000001001}, {'[', 0b0000000000111001}, {'\\', 0b0010000100000000},
      {']', 0b0000000000001111}, {'^', 0b0000000000100011}, {'_', 0b0000000000001000}, {'`', 0b0000000100000000},
      {'a', 0b0001000001011000}, {'b', 0b0010000001111000}, {'c', 0b0000000011011000}, {'d', 0b0000100010001110},
      {'e', 0b0000100001011000}, {'f', 0b0000000001110001}, {'g', 0b0000000110001111}, {'h', 0b0001000001110000},
      {'i', 0b0001000000000000}, {'j', 0b0000000000001110}, {'k', 0b0011011000000000}, {'l', 0b0000000000110000},
      {'m', 0b0001000011010100}, {'n', 0b0001000001010000}, {'o', 0b0000000011011100}, {'p', 0b0000010001110001},
      {'q', 0b0010000011100011}, {'r', 0b0000000001010000}, {'s', 0b0010000010001000}, {'t', 0b0000000001111000},
      {'u', 0b0000000000011100}, {'v', 0b0010000000000100}, {'w', 0b0010100000010100}, {'x', 0b0010100011000000},
      {'y', 0b0000001010001110}, {'z', 0b0000100001001000}, {'{', 0b0000100101001001}, {'|', 0b0001001000000000},
      {'}', 0b0010010010001001}, {'~', 0b0000000011000000},
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

// Convert the font table to the format that the Sparkfun display expects.
//  The font table was developed for Adafruit displays, and needs to be adapted
//  for the Sparkfun devices. To do this, we:
//    - Adding an extra zero between bit six and seven(the MSB of the first byte of the font).
//    - Switching segment L and N. The Adafruit and Sparkfun devices have different definitaions
//      of this segment.
//  This could be precomputed, but the simple bit shifting allows us to use a common font table for
//  the 14 segment devices to save memory at the expense of a slight performance hit.
uint16_t Sparkfun14Seg::convert_font_(uint16_t font_in) {
  uint16_t font_out = ((font_in & 0xFF80) << 1) | (font_in & 0x7F);

  if (((font_out & 0x1000) != 0x0000) && !((font_out & 0x4000) != 0x0000)) {
    // Segment L is lit, need to switch to segment N
    font_out = (font_out | 0x4000) & ~(0x1000);
  } else if ((font_out & 0x4000) != 0x0000) {
    // Segment N is lit, need to switch to segment L
    font_out = (font_out | 0x1000) & ~(0x4000);
  }

  return font_out;
}

// Write a character at position 'char_position' to the memory buffer.
void Sparkfun14Seg::write_to_buffer_(uint16_t char_to_write, uint8_t char_position) {
  // char_position should be 0-3
  if ((char_position >= 0) && (char_position <= 3)) {
    uint16_t digit_def = this->convert_font_(char_to_write);

    for (uint8_t i = 0; i < 8; i++) {
      // i counts through the com positions
      this->buffer_[i * 2 + 1] |= ((digit_def >> i) & 0x01) << (char_position);
      this->buffer_[i * 2 + 1] |= ((digit_def >> (i + 8)) & 0x01) << (char_position + 4);
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
  const std::unordered_map<char, uint16_t> char_map = {
      {' ', 0b0000000000000000}, {'!', 0b0000000000000110}, {'"', 0b0000001000100000}, {'#', 0b0001001011001110},
      {'$', 0b0001001011101101}, {'%', 0b0000110000100100}, {'&', 0b0010001101011101}, {'\'', 0b0000010000000000},
      {'(', 0b0010010000000000}, {')', 0b0000100100000000}, {'*', 0b0011111111000000}, {'+', 0b0001001011000000},
      {',', 0b0000100000000000}, {'-', 0b0000000011000000}, {'/', 0b0000110000000000}, {'0', 0b0000110000111111},
      {'1', 0b0000000000000110}, {'2', 0b0000000011011011}, {'3', 0b0000000010001111}, {'4', 0b0000000011100110},
      {'5', 0b0010000001101001}, {'6', 0b0000000011111101}, {'7', 0b0001010000000001}, {'8', 0b0000000011111111},
      {'9', 0b0000000011101111}, {':', 0b0001001000000000}, {';', 0b0000101000000000}, {'<', 0b0010010000000000},
      {'=', 0b0000000011001000}, {'>', 0b0000100100000000}, {'?', 0b0001000010000011}, {'@', 0b0000001010111011},
      {'A', 0b0000000011110111}, {'B', 0b0001001010001111}, {'C', 0b0000000000111001}, {'D', 0b0001001000001111},
      {'E', 0b0000000011111001}, {'F', 0b0000000001110001}, {'G', 0b0000000010111101}, {'H', 0b0000000011110110},
      {'I', 0b0001001000000000}, {'J', 0b0000000000011110}, {'K', 0b0010010001110000}, {'L', 0b0000000000111000},
      {'M', 0b0000010100110110}, {'N', 0b0010000100110110}, {'O', 0b0000000000111111}, {'P', 0b0000000011110011},
      {'Q', 0b0010000000111111}, {'R', 0b0010000011110011}, {'S', 0b0000000011101101}, {'T', 0b0001001000000001},
      {'U', 0b0000000000111110}, {'V', 0b0000110000110000}, {'W', 0b0010100000110110}, {'X', 0b0010110100000000},
      {'Y', 0b0001010100000000}, {'Z', 0b0000110000001001}, {'[', 0b0000000000111001}, {'\\', 0b0010000100000000},
      {']', 0b0000000000001111}, {'^', 0b0000000000100011}, {'_', 0b0000000000001000}, {'`', 0b0000000100000000},
      {'a', 0b0001000001011000}, {'b', 0b0010000001111000}, {'c', 0b0000000011011000}, {'d', 0b0000100010001110},
      {'e', 0b0000100001011000}, {'f', 0b0000000001110001}, {'g', 0b0000000110001111}, {'h', 0b0001000001110000},
      {'i', 0b0001000000000000}, {'j', 0b0000000000001110}, {'k', 0b0011011000000000}, {'l', 0b0000000000110000},
      {'m', 0b0001000011010100}, {'n', 0b0001000001010000}, {'o', 0b0000000011011100}, {'p', 0b0000010001110001},
      {'q', 0b0010000011100011}, {'r', 0b0000000001010000}, {'s', 0b0010000010001000}, {'t', 0b0000000001111000},
      {'u', 0b0000000000011100}, {'v', 0b0010000000000100}, {'w', 0b0010100000010100}, {'x', 0b0010100011000000},
      {'y', 0b0000001010001110}, {'z', 0b0000100001001000}, {'{', 0b0000100101001001}, {'|', 0b0001001000000000},
      {'}', 0b0010010010001001}, {'~', 0b0000000011000000},
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

// Convert the font table to the format that the Sparkfun display expects.
//  The font table was developed for Adafruit displays, and needs to be adapted
//  for the Sparkfun devices. To do this, we:
//    - Adding an extra zero between bit six and seven(the MSB of the first byte of the font).
//    - Switching segment L and N. The Adafruit and Sparkfun devices have different definitaions
//      of this segment.
//  Additionally, this function flips the font so that it will look correct when the device is
//  upside-down.
//
//  This could be precomputed, but the simple bit shifting allows us to use a common font table for
//  the 14 segment devices to save memory at the expense of a slight performance hit.
uint16_t Sparkfun14SegFlip::convert_font_(uint16_t font_in) {
  uint16_t font_out = ((font_in & 0xFF80) << 1) | (font_in & 0x7F);
  uint16_t font_out2 = 0;

  if (((font_out & 0x1000) != 0x0000) && !((font_out & 0x4000) != 0x0000)) {
    // Segment L is lit, need to switch to segment N
    font_out = (font_out | 0x4000) & ~(0x1000);
  } else if ((font_out & 0x4000) != 0x0000) {
    // Segment N is lit, need to switch to segment L
    font_out = (font_out | 0x1000) & ~(0x4000);
  }

  // Flip the font.
  font_out2 = ((font_out & 0x0007) << 3) | ((font_out & 0x0038) >> 3) | ((font_out & 0x0E00) << 3) |
              ((font_out & 0x7000) >> 3) | ((font_out & 0x0040) << 2) | ((font_out & 0x0100) >> 2);

  return font_out2;
}

// Write a character at position 'char_position' to the memory buffer.
//  Note that for this flipped device, char_position is the logical position of the character.
//  For example, char_position = 0 is the left most character on the display. char_position is
//  converted in this function to correctly place the digits on the flipped display.
void Sparkfun14SegFlip::write_to_buffer_(uint16_t char_to_write, uint8_t char_position) {
  // char_position should be 0-3
  if ((char_position >= 0) && (char_position <= 3)) {
    uint16_t digit_def = this->convert_font_(char_to_write);
    uint8_t flipped_char_position = 3 - char_position;

    for (uint8_t i = 0; i < 8; i++) {
      // i counts through the com positions
      this->buffer_[i * 2 + 1] |= ((digit_def >> i) & 0x01) << (flipped_char_position);
      this->buffer_[i * 2 + 1] |= ((digit_def >> (i + 8)) & 0x01) << (flipped_char_position + 4);
    }
  }
}

}  // namespace ht16k33_char
}  // namespace esphome
