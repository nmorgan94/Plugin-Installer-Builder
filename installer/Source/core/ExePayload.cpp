#include "ExePayload.h"

namespace
{
    const juce::String magic ("PluginPayload1 ");
    constexpr int sizeDigits = 16;

    // Where a signature's certificate table starts, from the exe's PE header
    juce::int64 endBeforeSignature (juce::FileInputStream& in)
    {
        const auto fileSize = in.getTotalLength();

        if (! in.setPosition (0x3c))
            return fileSize;

        const auto peHeader = in.readInt();
        if (peHeader <= 0 || ! in.setPosition (peHeader) || in.readInt() != 0x00004550) // "PE\0\0"
            return fileSize;

        in.setPosition (peHeader + 24);
        const auto is64Bit = in.readShort() == 0x20b;

        // Certificate entry in the data directories
        in.setPosition (peHeader + 24 + (is64Bit ? 112 : 96) + 4 * 8);
        const auto certificates = static_cast<juce::int64> (static_cast<juce::uint32> (in.readInt()));
        return certificates > 0 && certificates <= fileSize ? certificates : fileSize;
    }
}

namespace exePayload
{
    std::optional<Range> find (const juce::File& exe)
    {
        juce::FileInputStream in (exe);
        if (! in.openedOk())
            return std::nullopt;

        auto end = endBeforeSignature (in);

        // Signing can pad with up to 7 zero bytes first
        for (int i = 0; i < 7 && end > 0 && in.setPosition (end - 1) && in.readByte() == 0; ++i)
            --end;

        const auto footerLength = magic.length() + sizeDigits;
        if (end < footerLength || ! in.setPosition (end - footerLength))
            return std::nullopt;

        juce::MemoryBlock footer;
        in.readIntoMemoryBlock (footer, footerLength);
        const auto text = footer.toString();

        if (! text.startsWith (magic))
            return std::nullopt;

        const auto size = text.substring (magic.length()).getHexValue64();
        const auto start = end - footerLength - size;

        if (size <= 0 || start <= 0)
            return std::nullopt;

        return Range { start, size };
    }

    bool extract (const juce::File& exe, const juce::File& folder)
    {
        const auto range = find (exe);
        auto stream = std::make_unique<juce::FileInputStream> (exe);

        if (! range || ! stream->openedOk())
            return false;

        juce::ZipFile zip (std::make_unique<juce::SubregionStream> (stream.release(), range->start, range->size, true));
        return zip.getNumEntries() > 0 && zip.uncompressTo (folder).wasOk();
    }

    bool copyWithoutPayload (const juce::File& exe, const juce::File& destination)
    {
        const auto range = find (exe);
        if (! range)
            return exe.copyFileTo (destination);

        if (! destination.deleteFile())
            return false;

        juce::FileInputStream in (exe);
        juce::FileOutputStream out (destination);

        if (! in.openedOk() || ! out.openedOk() || out.writeFromInputStream (in, range->start) != range->start)
            return false;

        out.flush();
        return out.getStatus().wasOk();
    }
}
