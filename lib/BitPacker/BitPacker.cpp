#include "BitPacker.hpp"

BitPacker::BitPacker(uint16_t maxBits) : maxBits_(maxBits), bitPosition_(maxBits), totalBits_(0)
{
    bufferSize_ = (maxBits + 7) / 8; // Ceiling(maxBits/8)
    buffer_ = new uint8_t[bufferSize_];
    reset();
}

BitPacker::~BitPacker()
{
    delete[] buffer_;
}

bool BitPacker::addField(uint64_t value, uint8_t bits)
{
    if (bitPosition_ < bits || totalBits_ + bits > maxBits_)
    {
        return false; // Overflow
    }
    bitPosition_ -= bits;
    totalBits_ += bits;

    // Write bits to buffer
    for (uint8_t i = 0; i < bits; i++)
    {
        uint16_t bitPos = bitPosition_ + i;
        uint8_t byteIdx = bitPos / 8;
        uint8_t bitIdx = 7 - (bitPos % 8);
        if (value & (1ULL << (bits - 1 - i)))
        {
            buffer_[byteIdx] |= (1U << bitIdx);
        }
        else
        {
            buffer_[byteIdx] &= ~(1U << bitIdx);
        }
    }
    return true;
}

uint64_t BitPacker::extractField(uint8_t bits)
{
    if (bitPosition_ < bits)
    {
        return 0; // Underflow
    }
    bitPosition_ -= bits;

    // Read bits from buffer
    uint64_t value = 0;
    for (uint8_t i = 0; i < bits; i++)
    {
        uint16_t bitPos = bitPosition_ + i;
        uint8_t byteIdx = bitPos / 8;
        uint8_t bitIdx = 7 - (bitPos % 8);
        if (buffer_[byteIdx] & (1U << bitIdx))
        {
            value |= (1ULL << (bits - 1 - i));
        }
    }
    return value;
}

bool BitPacker::pack7Bit(uint8_t *output, uint8_t &outputSize)
{
    outputSize = (totalBits_ + 6) / 7; // Ceiling(totalBits/7)
    if (!output)
        return false;

    for (uint8_t i = 0; i < outputSize; i++)
    {
        uint64_t value = 0;
        for (uint8_t j = 0; j < 7; j++)
        {
            uint16_t bitPos = (maxBits_ - 1) - (i * 7 + j); // Read from highest bit downward
            if (bitPos < (maxBits_ - totalBits_) || bitPos >= maxBits_)
                break;
            uint8_t byteIdx = bitPos / 8;
            uint8_t bitIdx = 7 - (bitPos % 8);
            if (buffer_[byteIdx] & (1U << bitIdx))
            {
                value |= (1ULL << (6 - j));
            }
        }
        output[i] = value & 0x7F;
    }
    return true;
}

bool BitPacker::unpack7Bit(const uint8_t *input, uint8_t inputSize)
{
    reset();                            // Clear existing bitstream
    uint16_t totalBits = inputSize * 7; // Max bits from input
    if (totalBits > maxBits_)
        return false; // Overflow

    for (uint8_t i = 0; i < inputSize; i++)
    {
        uint64_t value = input[i] & 0x7F;
        for (uint8_t j = 0; j < 7; j++)
        {
            uint16_t bitPos = (maxBits_ - 1) - (i * 7 + j); // Write to high end, reversed
            if (bitPos < (maxBits_ - totalBits))
                break;
            uint8_t byteIdx = bitPos / 8;
            uint8_t bitIdx = 7 - (bitPos % 8);
            if (value & (1ULL << (6 - j)))
            {
                buffer_[byteIdx] |= (1U << bitIdx);
            }
            else
            {
                buffer_[byteIdx] &= ~(1U << bitIdx);
            }
        }
    }
    totalBits_ = totalBits;
    bitPosition_ = maxBits_; // Ready for extraction
    return true;
}

uint16_t BitPacker::getTotalBits() const
{
    return totalBits_;
}

void BitPacker::reset()
{
    for (uint8_t i = 0; i < bufferSize_; i++)
    {
        buffer_[i] = 0;
    }
    bitPosition_ = maxBits_;
    totalBits_ = 0;
}