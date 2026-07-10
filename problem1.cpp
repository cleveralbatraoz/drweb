// HA archive header parser

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ha
{

constexpr size_t kFileCountOffset = 2;
constexpr size_t kFirstEntryOffset = 4;
constexpr size_t kFixedFieldsSize = 0x11; // method + sizes + CRC + time
constexpr size_t kNamesOffset = kFirstEntryOffset + kFixedFieldsSize;
constexpr size_t kMaxNameLength = 0xFF;
constexpr size_t kNameBufferSize = kMaxNameLength + 1;

enum CompressionMethod : uint32_t
{
    kMethodCpy = 0, // no compression
    kMethodAsc = 1,
    kMethodHsc = 2,
};

constexpr size_t kSampleOffset = 5;
constexpr size_t kSampleSize = 0x8000;
constexpr uint32_t kMinSampleLength = 266; // short samples skip the check
constexpr uint32_t kByteValueCount = 256;
constexpr uint32_t kBandUpperDivisor = 236; // 1/236 frequency
constexpr uint32_t kBandLowerDivisor = 266; // 1/266 frequency
constexpr uint32_t kMinUniformValues = 15;

// Host scanner flags
enum ScanResult : uint32_t
{
    kNotRecognized = 0x0000,
    kCorruptHeader = 0x0020, // file name truncated or too long
    kRecognized = 0x1000,
};

class Reader
{
  public:
    Reader(const uint8_t * data, size_t size) noexcept : data_(data), size_(size), pos_(0)
    {
    }

    void seek(size_t offset) noexcept
    {
        pos_ = offset;
    }

    size_t position() const noexcept
    {
        return pos_;
    }

    size_t size() const noexcept
    {
        return size_;
    }

    size_t read(void * dst, size_t count) noexcept
    {
        if (pos_ >= size_)
        {
            return 0;
        }
        const size_t available = size_ - pos_;
        if (count > available)
        {
            count = available;
        }
        std::memcpy(dst, data_ + pos_, count);
        pos_ += count;
        return count;
    }

  private:
    const uint8_t * data_;
    size_t size_;
    size_t pos_;
};

static bool ReadU8(Reader & in, uint32_t & value) noexcept
{
    uint8_t b;
    if (in.read(&b, 1) != 1)
    {
        return false;
    }
    value = b;
    return true;
}

static bool ReadU16le(Reader & in, uint32_t & value) noexcept
{
    uint8_t b[2];
    if (in.read(b, 2) != 2)
    {
        return false;
    }
    value = (uint32_t)b[0] | ((uint32_t)b[1] << 8);
    return true;
}

static bool ReadU32le(Reader & in, uint32_t & value) noexcept
{
    uint8_t b[4];
    if (in.read(b, 4) != 4)
    {
        return false;
    }
    value =
        (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return true;
}

// ASCIIZ, at most kMaxNameLength chars
static bool ReadName(Reader & in, char * buffer) noexcept
{
    for (size_t i = 0;;)
    {
        uint8_t b;
        if (in.read(&b, 1) != 1)
        {
            return false;
        }
        buffer[i] = (char)b;
        if (b == 0)
        {
            return true;
        }
        if (++i > kMaxNameLength)
        {
            return false;
        }
    }
}

// Record shared with the host scanner
struct HostArchiveInfo
{
    uint32_t slots[16];
};
constexpr size_t kInfoSlotFormat = 6;
constexpr uint32_t kInfoFormatHa = 4;
constexpr size_t kInfoSlotArchiveSize = 10;

// Inputs, results and scratch buffers
struct ScanContext
{
    Reader archive;

    uint32_t method = 0;
    uint32_t compressedSize = 0;
    uint32_t originalSize = 0;
    char name[kNameBufferSize] = {}; // path + file name
    HostArchiveInfo hostInfo = {};

    uint8_t sample[kSampleSize];
    uint32_t histogram[kByteValueCount];

    ScanContext(const uint8_t * data, size_t size) noexcept : archive(data, size)
    {
    }
};

static bool LooksCompressed(const uint8_t * sample, uint32_t length,
                            uint32_t histogram[kByteValueCount]) noexcept
{
    if (length <= kMinSampleLength)
    {
        return true; // too little data for statistics
    }

    std::memset(histogram, 0, kByteValueCount * sizeof(uint32_t));
    for (uint32_t i = 0; i < length; ++i)
    {
        ++histogram[sample[i]];
    }

    const uint32_t coef = (0xFFFFFFFFu - 1u) / length;
    const uint32_t upper = (length * coef) / kBandUpperDivisor;
    const uint32_t lower = (length * coef) / kBandLowerDivisor - coef;

    uint32_t uniformValues = 0;
    for (uint32_t v = 0; v < kByteValueCount; ++v)
    {
        const uint32_t scaledFreq = histogram[v] * coef;
        if (scaledFreq < upper && scaledFreq > lower)
        {
            ++uniformValues;
        }
    }
    return uniformValues >= kMinUniformValues;
}

// Parses the first entry header
ScanResult ParseHeader(ScanContext & ctx)
{
    Reader & in = ctx.archive;
    const size_t archiveSize = in.size();

    // Must be present
    in.seek(kFileCountOffset);
    uint32_t fileCount;
    if (!ReadU16le(in, fileCount))
    {
        return kNotRecognized;
    }

    uint32_t methodByte;
    if (!ReadU8(in, methodByte))
    {
        return kNotRecognized;
    }
    ctx.method = methodByte & 0x0Fu; // high nibble is ignored
    if (ctx.method != kMethodCpy && ctx.method != kMethodAsc && ctx.method != kMethodHsc)
    {
        return kNotRecognized;
    }

    if (!ReadU32le(in, ctx.compressedSize))
    {
        return kNotRecognized;
    }
    if (!ReadU32le(in, ctx.originalSize))
    {
        return kNotRecognized;
    }
    if (ctx.originalSize < ctx.compressedSize)
    {
        return kNotRecognized;
    }

    in.seek(kNamesOffset); // skip CRC-32 and timestamp

    // Path, file name in the same buffer
    if (!ReadName(in, ctx.name))
    {
        return kNotRecognized;
    }
    if (!ReadName(in, ctx.name))
    {
        return kCorruptHeader;
    }

    // length + data
    uint32_t machineInfoLength;
    if (!ReadU8(in, machineInfoLength))
    {
        return kNotRecognized;
    }
    const size_t dataOffset = in.position() + machineInfoLength;

    if (ctx.compressedSize > archiveSize || dataOffset > archiveSize - ctx.compressedSize)
    {
        return kNotRecognized;
    }

    ctx.hostInfo.slots[kInfoSlotFormat] = kInfoFormatHa;
    ctx.hostInfo.slots[kInfoSlotArchiveSize] = (uint32_t)archiveSize;

    if (ctx.method != kMethodCpy) // CPY stores data as is
    {
        in.seek(kSampleOffset);
        const size_t sampleLength = in.read(ctx.sample, kSampleSize);
        if (!LooksCompressed(ctx.sample, (uint32_t)sampleLength, ctx.histogram))
        {
            return kNotRecognized;
        }
    }

    return kRecognized;
}

} // namespace ha
