#ifndef BITPACKER_HPP
#define BITPACKER_HPP

#include <Arduino.h>

class BitPacker {
public:
  // Constructor: Initialize with maximum bit capacity (default 128 bits)
  BitPacker(uint16_t maxBits = 128);

  // Destructor: Free buffer
  ~BitPacker();

  // Add a field to the bitstream
  bool addField(uint64_t value, uint8_t bits);

  // Extract a field from the bitstream
  uint64_t extractField(uint8_t bits);

  // Pack bitstream into 7-bit bytes
  bool pack7Bit(uint8_t* output, uint8_t& outputSize);

  // Unpack 7-bit bytes into bitstream
  bool unpack7Bit(const uint8_t* input, uint8_t inputSize);

  // Get total bits packed
  uint16_t getTotalBits() const;

  // Reset the packer
  void reset();

private:
  uint8_t* buffer_;      // Bitstream buffer
  uint16_t maxBits_;     // Maximum bit capacity
  uint8_t bufferSize_;   // Buffer size in bytes
  uint16_t bitPosition_; // Current bit position (MSB to LSB)
  uint16_t totalBits_;   // Total bits packed
};

#endif // BITPACKER_HPP