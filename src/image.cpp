// ***************************************************************** -*- C++ -*-
/*
 * Copyright (C) 2004-2021 Exiv2 authors
 * This program is part of the Exiv2 distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301 USA.
 */
// *****************************************************************************
// included header files
#include "config.h"

#include "image.hpp"
#include "image_int.hpp"
#include "error.hpp"
#include "enforce.hpp"
#include "futils.hpp"
#include "safe_op.hpp"
#include "slice.hpp"

#ifdef   EXV_ENABLE_BMFF
#include "bmffimage.hpp"
#endif// EXV_ENABLE_BMFF
#include "cr2image.hpp"
#include "crwimage.hpp"
#include "epsimage.hpp"
#include "jpgimage.hpp"
#include "mrwimage.hpp"
#ifdef   EXV_HAVE_LIBZ
# include "pngimage.hpp"
#endif// EXV_HAVE_LIBZ
#include "rafimage.hpp"
#include "tiffimage.hpp"
#include "tiffimage_int.hpp"
#include "tiffcomposite_int.hpp"
#include "tiffvisitor_int.hpp"
#include "webpimage.hpp"
#include "orfimage.hpp"
#include "gifimage.hpp"
#include "psdimage.hpp"
#include "tgaimage.hpp"
#include "bmpimage.hpp"
#include "jp2image.hpp"
#include "nikonmn_int.hpp"

#include "rw2image.hpp"
#include "pgfimage.hpp"
#include "xmpsidecar.hpp"

// + standard includes
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <iostream>
#include <limits>
#include <set>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef _MSC_VER
# define S_ISREG(m)      (((m) & S_IFMT) == S_IFREG)
#endif
#ifdef EXV_HAVE_UNISTD_H
# include <unistd.h>                            // stat
#endif

// *****************************************************************************
namespace {

    using namespace Exiv2;

    //! Struct for storing image types and function pointers.
    struct Registry {
        //! Comparison operator to compare a Registry structure with an image type
        bool operator==(const int& imageType) const { return imageType == imageType_; }

        // DATA
        int            imageType_;
        NewInstanceFct newInstance_;
        IsThisTypeFct  isThisType_;
        AccessMode     exifSupport_;
        AccessMode     iptcSupport_;
        AccessMode     xmpSupport_;
        AccessMode     commentSupport_;
    };

    const Registry registry[] = {
        //image type       creation fct     type check  Exif mode    IPTC mode    XMP mode     Comment mode
        //---------------  ---------------  ----------  -----------  -----------  -----------  ------------
        { ImageType::jpeg, newJpegInstance, isJpegType, amReadWrite, amReadWrite, amReadWrite, amReadWrite },
        { ImageType::exv,  newExvInstance,  isExvType,  amReadWrite, amReadWrite, amReadWrite, amReadWrite },
        { ImageType::cr2,  newCr2Instance,  isCr2Type,  amReadWrite, amReadWrite, amReadWrite, amNone      },
        { ImageType::crw,  newCrwInstance,  isCrwType,  amReadWrite, amNone,      amNone,      amReadWrite },
        { ImageType::mrw,  newMrwInstance,  isMrwType,  amRead,      amRead,      amRead,      amNone      },
        { ImageType::tiff, newTiffInstance, isTiffType, amReadWrite, amReadWrite, amReadWrite, amNone      },
        { ImageType::webp, newWebPInstance, isWebPType, amReadWrite, amNone,      amReadWrite, amNone      },
        { ImageType::dng,  newTiffInstance, isTiffType, amReadWrite, amReadWrite, amReadWrite, amNone      },
        { ImageType::nef,  newTiffInstance, isTiffType, amReadWrite, amReadWrite, amReadWrite, amNone      },
        { ImageType::pef,  newTiffInstance, isTiffType, amReadWrite, amReadWrite, amReadWrite, amNone      },
        { ImageType::arw,  newTiffInstance, isTiffType, amRead,      amRead,      amRead,      amNone      },
        { ImageType::rw2,  newRw2Instance,  isRw2Type,  amRead,      amRead,      amRead,      amNone      },
        { ImageType::sr2,  newTiffInstance, isTiffType, amRead,      amRead,      amRead,      amNone      },
        { ImageType::srw,  newTiffInstance, isTiffType, amReadWrite, amReadWrite, amReadWrite, amNone      },
        { ImageType::orf,  newOrfInstance,  isOrfType,  amReadWrite, amReadWrite, amReadWrite, amNone      },
#ifdef EXV_HAVE_LIBZ
        { ImageType::png,  newPngInstance,  isPngType,  amReadWrite, amReadWrite, amReadWrite, amReadWrite },
#endif // EXV_HAVE_LIBZ
        { ImageType::pgf,  newPgfInstance,  isPgfType,  amReadWrite, amReadWrite, amReadWrite, amReadWrite },
        { ImageType::raf,  newRafInstance,  isRafType,  amRead,      amRead,      amRead,      amNone      },
        { ImageType::eps,  newEpsInstance,  isEpsType,  amNone,      amNone,      amReadWrite, amNone      },
        { ImageType::xmp,  newXmpInstance,  isXmpType,  amReadWrite, amReadWrite, amReadWrite, amNone      },
        { ImageType::gif,  newGifInstance,  isGifType,  amNone,      amNone,      amNone,      amNone      },
        { ImageType::psd,  newPsdInstance,  isPsdType,  amReadWrite, amReadWrite, amReadWrite, amNone      },
        { ImageType::tga,  newTgaInstance,  isTgaType,  amNone,      amNone,      amNone,      amNone      },
        { ImageType::bmp,  newBmpInstance,  isBmpType,  amNone,      amNone,      amNone,      amNone      },
        { ImageType::jp2,  newJp2Instance,  isJp2Type,  amReadWrite, amReadWrite, amReadWrite, amNone      },
#ifdef EXV_ENABLE_BMFF
        { ImageType::bmff, newBmffInstance, isBmffType, amRead,      amRead,      amRead,      amNone      },
#endif // EXV_ENABLE_BMFF
        // End of list marker
        { ImageType::none, nullptr,               nullptr,          amNone,      amNone,      amNone,      amNone      }
    };

}  // namespace

// *****************************************************************************
// class member definitions
namespace Exiv2 {
    Image::Image(int imageType, uint16_t supportedMetadata, BasicIo::UniquePtr io)
        : io_(std::move(io)),
          pixelWidth_(0),
          pixelHeight_(0),
          imageType_(imageType),
          supportedMetadata_(supportedMetadata),
#ifdef EXV_HAVE_XMP_TOOLKIT
          writeXmpFromPacket_(false),
#else
          writeXmpFromPacket_(true),
#endif
          byteOrder_(invalidByteOrder),
          init_(true)
    {
    }

    void Image::printStructure(std::ostream&, PrintStructureOption,int /*depth*/)
    {
        throw Error(kerUnsupportedImageType, io_->path());
    }

    bool Image::isStringType(uint16_t type)
    {
        return type == Exiv2::asciiString
            || type == Exiv2::unsignedByte
            || type == Exiv2::signedByte
            || type == Exiv2::undefined
            ;
    }
    bool Image::isShortType(uint16_t type) {
         return type == Exiv2::unsignedShort
             || type == Exiv2::signedShort
             ;
    }
    bool Image::isLongType(uint16_t type) {
         return type == Exiv2::unsignedLong
             || type == Exiv2::signedLong
             ;
    }
    bool Image::isLongLongType(uint16_t type) {
        return type == Exiv2::unsignedLongLong
            || type == Exiv2::signedLongLong
            ;
    }
    bool Image::isRationalType(uint16_t type) {
         return type == Exiv2::unsignedRational
             || type == Exiv2::signedRational
             ;
    }
    bool Image::is2ByteType(uint16_t type)
    {
        return isShortType(type);
    }
    bool Image::is4ByteType(uint16_t type)
    {
        return isLongType(type)
            || type == Exiv2::tiffFloat
            || type == Exiv2::tiffIfd
            ;
    }
    bool Image::is8ByteType(uint16_t type)
    {
        return isRationalType(type)
             || isLongLongType(type)
             || type == Exiv2::tiffIfd8
             || type == Exiv2::tiffDouble
            ;
    }
    bool Image::isPrintXMP(uint16_t type, Exiv2::PrintStructureOption option)
    {
        return type == 700 && option == kpsXMP;
    }
    bool Image::isPrintICC(uint16_t type, Exiv2::PrintStructureOption option)
    {
        return type == 0x8773 && option == kpsIccProfile;
    }

    bool Image::isBigEndianPlatform()
    {
        union {
            uint32_t i;
            char c[4];
        } e = { 0x01000000 };

        return e.c[0] != 0;
    }
    bool Image::isLittleEndianPlatform() { return !isBigEndianPlatform(); }

    uint64_t Image::byteSwap(uint64_t value, bool bSwap)
    {
        uint64_t result = 0;
        auto source_value = reinterpret_cast<byte*>(&value);
        auto destination_value = reinterpret_cast<byte*>(&result);

        for (int i = 0; i < 8; i++)
            destination_value[i] = source_value[8 - i - 1];

        return bSwap ? result : value;
    }

    uint32_t Image::byteSwap(uint32_t value, bool bSwap)
    {
        uint32_t result = 0;
        result |= (value & 0x000000FF) << 24;
        result |= (value & 0x0000FF00) << 8;
        result |= (value & 0x00FF0000) >> 8;
        result |= (value & 0xFF000000) >> 24;
        return bSwap ? result : value;
    }

    uint16_t Image::byteSwap(uint16_t value, bool bSwap)
    {
        uint16_t result = 0;
        result |= (value & 0x00FF) << 8;
        result |= (value & 0xFF00) >> 8;
        return bSwap ? result : value;
    }

    uint16_t Image::byteSwap2(const DataBuf& buf,size_t offset,bool bSwap) 
    {
        uint16_t v = 0;
        auto p = reinterpret_cast<char*>(&v);
        p[0] = buf.read_uint8(offset);
        p[1] = buf.read_uint8(offset+1);
        return Image::byteSwap(v,bSwap);
    }

    uint32_t Image::byteSwap4(const DataBuf& buf,size_t offset,bool bSwap) 
    {
        uint32_t v = 0;
        auto p = reinterpret_cast<char*>(&v);
        p[0] = buf.read_uint8(offset);
        p[1] = buf.read_uint8(offset+1);
        p[2] = buf.read_uint8(offset+2);
        p[3] = buf.read_uint8(offset+3);
        return Image::byteSwap(v,bSwap);
    }

    uint64_t Image::byteSwap8(const DataBuf& buf,size_t offset,bool bSwap) 
    {
        uint64_t v = 0;
        auto p = reinterpret_cast<byte*>(&v);

        for(int i = 0; i < 8; i++)
            p[i] = buf.read_uint8(offset + i);

        return Image::byteSwap(v,bSwap);
    }

    const char* Image::typeName(uint16_t tag)
    {
        //! List of TIFF image tags
        const char* result = nullptr;
        switch (tag ) {
            case Exiv2::unsignedByte     : result = "BYTE"      ; break;
            case Exiv2::asciiString      : result = "ASCII"     ; break;
            case Exiv2::unsignedShort    : result = "SHORT"     ; break;
            case Exiv2::unsignedLong     : result = "LONG"      ; break;
            case Exiv2::unsignedRational : result = "RATIONAL"  ; break;
            case Exiv2::signedByte       : result = "SBYTE"     ; break;
            case Exiv2::undefined        : result = "UNDEFINED" ; break;
            case Exiv2::signedShort      : result = "SSHORT"    ; break;
            case Exiv2::signedLong       : result = "SLONG"     ; break;
            case Exiv2::signedRational   : result = "SRATIONAL" ; break;
            case Exiv2::tiffFloat        : result = "FLOAT"     ; break;
            case Exiv2::tiffDouble       : result = "DOUBLE"    ; break;
            case Exiv2::tiffIfd          : result = "IFD"       ; break;
            default                      : result = "unknown"   ; break;
        }
        return result;
    }

    static bool typeValid(uint16_t type)
    {
        return type >= 1 && type <= 13 ;
    }

    static std::set<long> visits; // #547
    void Image::printIFDStructure(BasicIo& io, std::ostream& out, Exiv2::PrintStructureOption option,uint32_t start,bool bSwap,char c,int depth)
    {
        depth++;
        if ( depth == 1 ) visits.clear();
        bool bFirst  = true  ;

        // buffer
        const size_t dirSize = 32;
        DataBuf  dir(dirSize);
        bool bPrint = option == kpsBasic || option == kpsRecursive;

        do {
            // Read top of directory
            io.seekOrThrow(start, BasicIo::beg, kerCorruptedMetadata);
            io.readOrThrow(dir.data(), 2, kerCorruptedMetadata);
            uint16_t   dirLength = byteSwap2(dir,0,bSwap);
            // Prevent infinite loops. (GHSA-m479-7frc-gqqg)
            enforce(dirLength > 0, kerCorruptedMetadata);

            if ( dirLength > 500 ) // tooBig
                throw Error(kerTiffDirectoryTooLarge);

            if ( bFirst && bPrint ) {
                out << Internal::indent(depth) << Internal::stringFormat("STRUCTURE OF TIFF FILE (%c%c): ",c,c) << io.path() << std::endl;
            }

            // Read the dictionary
            for ( int i = 0 ; i < dirLength ; i ++ ) {
                if ( visits.find(io.tell()) != visits.end()  ) { // #547
                    throw Error(kerCorruptedMetadata);
                }
                visits.insert(io.tell());
                
                if ( bFirst && bPrint ) {
                    out << Internal::indent(depth)
                        << " address |    tag                              |     "
                        << " type |    count |    offset | value\n";
                }
                bFirst = false;

                io.readOrThrow(dir.data(), 12, kerCorruptedMetadata);
                uint16_t tag    = byteSwap2(dir,0,bSwap);
                uint16_t type   = byteSwap2(dir,2,bSwap);
                uint32_t count  = byteSwap4(dir,4,bSwap);
                uint32_t offset = byteSwap4(dir,8,bSwap);

                // Break for unknown tag types else we may segfault.
                if ( !typeValid(type) ) {
                    EXV_ERROR << "invalid type in tiff structure" << type << std::endl;
                    start = 0; // break from do loop
                    throw Error(kerInvalidTypeValue);
                }

                std::string sp;  // output spacer

                //prepare to print the value
                uint32_t kount  = isPrintXMP(tag,option) ? count // haul in all the data
                                : isPrintICC(tag,option) ? count // ditto
                                : isStringType(type)     ? (count > 32 ? 32 : count) // restrict long arrays
                                : count > 5              ? 5
                                : count
                                ;
                uint32_t pad    = isStringType(type) ? 1 : 0;
                uint32_t size   = isStringType(type) ? 1
                                : is2ByteType(type)  ? 2
                                : is4ByteType(type)  ? 4
                                : is8ByteType(type)  ? 8
                                : 1
                                ;

                // if ( offset > io.size() ) offset = 0; // Denial of service?

                // #55 and #56 memory allocation crash test/data/POC8
                const uint64_t allocate64 = static_cast<uint64_t>(size) * count + pad + 20;
                if ( allocate64 > io.size() ) {
                    throw Error(kerInvalidMalloc);
                }
                // Overflow check
                enforce(allocate64 <= static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()), kerCorruptedMetadata);
                enforce(allocate64 <= static_cast<uint64_t>(std::numeric_limits<long>::max()), kerCorruptedMetadata);
                const long allocate = static_cast<long>(allocate64);
                DataBuf  buf(allocate);  // allocate a buffer
                buf.clear();
                buf.copyBytes(0, dir.c_data(8), 4);  // copy dir[8:11] into buffer (short strings)

                // We have already checked that this multiplication cannot overflow.
                const uint32_t count_x_size = count*size;
                const bool bOffsetIsPointer = count_x_size > 4;

                if ( bOffsetIsPointer ) {         // read into buffer
                    const long restore = io.tell(); // save
                    io.seekOrThrow(offset, BasicIo::beg, kerCorruptedMetadata); // position
                    io.readOrThrow(buf.data(), static_cast<long>(count_x_size), kerCorruptedMetadata); // read
                    io.seekOrThrow(restore, BasicIo::beg, kerCorruptedMetadata); // restore
                }

                if ( bPrint ) {
                    const uint32_t address = start + 2 + i*12 ;
                    const std::string offsetString = bOffsetIsPointer?
                        Internal::stringFormat("%10u", offset):
                        "";

                    out << Internal::indent(depth)
                    << Internal::stringFormat("%8u | %#06x %-28s |%10s |%9u |%10s | "
                                              ,address,tag,tagName(tag).c_str(),typeName(type),count,offsetString.c_str());
                    if ( isShortType(type) ){
                        for ( size_t k = 0 ; k < kount ; k++ ) {
                            out << sp << byteSwap2(buf,k*size,bSwap);
                            sp = " ";
                        }
                    } else if ( isLongType(type) ){
                        for ( size_t k = 0 ; k < kount ; k++ ) {
                            out << sp << byteSwap4(buf,k*size,bSwap);
                            sp = " ";
                        }

                    } else if ( isRationalType(type) ){
                        for ( size_t k = 0 ; k < kount ; k++ ) {
                            uint32_t a = byteSwap4(buf,k*size+0,bSwap);
                            uint32_t b = byteSwap4(buf,k*size+4,bSwap);
                            out << sp << a << "/" << b;
                            sp = " ";
                        }
                    } else if ( isStringType(type) ) {
                        out << sp << Internal::binaryToString(makeSlice(buf, 0, kount));
                    }

                    sp = kount == count ? "" : " ...";
                    out << sp << std::endl;

                    if ( option == kpsRecursive && (tag == 0x8769 /* ExifTag */ || tag == 0x014a/*SubIFDs*/  || type == tiffIfd) ) {
                        for ( size_t k = 0 ; k < count ; k++ ) {
                            const long restore = io.tell();
                            offset = byteSwap4(buf,k*size,bSwap);
                            printIFDStructure(io,out,option,offset,bSwap,c,depth);
                            io.seekOrThrow(restore, BasicIo::beg, kerCorruptedMetadata);
                        }
                    } else if ( option == kpsRecursive && tag == 0x83bb /* IPTCNAA */ ) {
                        if (count > 0) {
                            if (static_cast<size_t>(Safe::add(count, offset)) > io.size()) {
                                throw Error(kerCorruptedMetadata);
                            }

                            const long restore = io.tell();
                            io.seekOrThrow(offset, BasicIo::beg, kerCorruptedMetadata);  // position
                            std::vector<byte> bytes(count) ;  // allocate memory
                            // TODO: once we have C++11 use bytes.data()
                            io.readOrThrow(&bytes[0], count, kerCorruptedMetadata);
                            io.seekOrThrow(restore, BasicIo::beg, kerCorruptedMetadata);
                            // TODO: once we have C++11 use bytes.data()
                            IptcData::printStructure(out, makeSliceUntil(&bytes[0], count), depth);
                        }
                    }  else if ( option == kpsRecursive && tag == 0x927c /* MakerNote */ && count > 10) {
                        const long restore = io.tell();  // save

                        uint32_t jump= 10           ;
                        byte     bytes[20]          ;
                        const auto chars = reinterpret_cast<const char*>(&bytes[0]);
                        io.seekOrThrow(offset, BasicIo::beg, kerCorruptedMetadata);  // position
                        io.readOrThrow(bytes, jump, kerCorruptedMetadata)     ;  // read
                        bytes[jump]=0               ;

                        bool bNikon = ::strcmp("Nikon"    ,chars) == 0;
                        bool bSony  = ::strcmp("SONY DSC ",chars) == 0;

                        if ( bNikon ) {
                            // tag is an embedded tiff
                            const long byteslen = count-jump;
                            DataBuf bytes(byteslen);  // allocate a buffer
                            io.readOrThrow(bytes.data(), byteslen, kerCorruptedMetadata);  // read
                            MemIo memIo(bytes.c_data(), byteslen)    ;  // create a file
                            printTiffStructure(memIo,out,option,depth);
                        } else {
                            // tag is an IFD
                            uint32_t punt = bSony ? 12 : 0 ;
                            io.seekOrThrow(0, BasicIo::beg, kerCorruptedMetadata);  // position
                            printIFDStructure(io,out,option,offset+punt,bSwap,c,depth);
                        }

                        io.seekOrThrow(restore, BasicIo::beg, kerCorruptedMetadata); // restore
                    }
                }

                if ( isPrintXMP(tag,option) ) {
                    buf.write_uint8(count, 0);
                    out << buf.c_str();
                }
                if ( isPrintICC(tag,option) ) {
                    out.write(buf.c_str(), count);
                }
            }
            if ( start ) {
                io.readOrThrow(dir.data(), 4, kerCorruptedMetadata);
                start = byteSwap4(dir,0,bSwap);
            }
        } while (start) ;

        if ( bPrint ) {
            out << Internal::indent(depth) << "END " << io.path() << std::endl;
        }
        out.flush();
        depth--;
    }

    void Image::printTiffStructure(BasicIo& io, std::ostream& out, Exiv2::PrintStructureOption option,int depth,size_t offset /*=0*/)
    {
        if ( option == kpsBasic || option == kpsXMP || option == kpsRecursive || option == kpsIccProfile ) {
            // buffer
            const size_t dirSize = 32;
            DataBuf  dir(dirSize);

            // read header (we already know for certain that we have a Tiff file)
            io.readOrThrow(dir.data(),  8, kerCorruptedMetadata);
            char c = static_cast<char>(dir.read_uint8(0));
            bool bSwap   = ( c == 'M' && isLittleEndianPlatform() )
                        || ( c == 'I' && isBigEndianPlatform()    )
                        ;
            uint32_t start = byteSwap4(dir,4,bSwap);
            printIFDStructure(io, out, option, start + static_cast<uint32_t>(offset), bSwap, c, depth);
        }
    }

    void Image::clearMetadata()
    {
        clearExifData();
        clearIptcData();
        clearXmpPacket();
        clearXmpData();
        clearComment();
        clearIccProfile();
    }

    ExifData& Image::exifData()
    {
        return exifData_;
    }

    IptcData& Image::iptcData()
    {
        return iptcData_;
    }

    XmpData& Image::xmpData()
    {
        return xmpData_;
    }

    std::string& Image::xmpPacket()
    {
        // Serialize the current XMP
        if (xmpData_.count() > 0 && !writeXmpFromPacket()) {
            XmpParser::encode(xmpPacket_, xmpData_,
                              XmpParser::useCompactFormat |
                              XmpParser::omitAllFormatting);
        }
        return xmpPacket_;
    }

    void Image::setMetadata(const Image& image)
    {
        if (checkMode(mdExif) & amWrite) {
            setExifData(image.exifData());
        }
        if (checkMode(mdIptc) & amWrite) {
            setIptcData(image.iptcData());
        }
        if (checkMode(mdIccProfile) & amWrite) {
            setIccProfile(DataBuf(image.iccProfile()));
        }
        if (checkMode(mdXmp) & amWrite) {
            setXmpPacket(image.xmpPacket());
            setXmpData(image.xmpData());
        }
        if (checkMode(mdComment) & amWrite) {
            setComment(image.comment());
        }
    }

    void Image::clearExifData()
    {
        exifData_.clear();
    }

    void Image::setExifData(const ExifData& exifData)
    {
        exifData_ = exifData;
    }

    void Image::clearIptcData()
    {
        iptcData_.clear();
    }

    void Image::setIptcData(const IptcData& iptcData)
    {
        iptcData_ = iptcData;
    }

    void Image::clearXmpPacket()
    {
        xmpPacket_.clear();
        writeXmpFromPacket(true);
    }

    void Image::setXmpPacket(const std::string& xmpPacket)
    {
        xmpPacket_ = xmpPacket;
        if ( XmpParser::decode(xmpData_, xmpPacket) ) {
            throw Error(kerInvalidXMP);
        }
        xmpPacket_ = xmpPacket;
    }

    void Image::clearXmpData()
    {
        xmpData_.clear();
        writeXmpFromPacket(false);
    }

    void Image::setXmpData(const XmpData& xmpData)
    {
        xmpData_ = xmpData;
        writeXmpFromPacket(false);
    }

#ifdef EXV_HAVE_XMP_TOOLKIT
    void Image::writeXmpFromPacket(bool flag)
    {
        writeXmpFromPacket_ = flag;
    }
#else
    void Image::writeXmpFromPacket(bool) {}
#endif

    void Image::clearComment()
    {
        comment_.erase();
    }

    void Image::setComment(const std::string& comment)
    {
        comment_ = comment;
    }

    void Image::setIccProfile(Exiv2::DataBuf&& iccProfile,bool bTestValid)
    {
        if ( bTestValid ) {
            if (iccProfile.size() < static_cast<long>(sizeof(long))) {
                throw Error(kerInvalidIccProfile);
            }
            const long size = iccProfile.read_uint32(0, bigEndian);
            if (size != iccProfile.size()) {
                throw Error(kerInvalidIccProfile);
            }
        }
        iccProfile_ = std::move(iccProfile);
    }

    void Image::clearIccProfile()
    {
        iccProfile_.reset();
    }

    void Image::setByteOrder(ByteOrder byteOrder)
    {
        byteOrder_ = byteOrder;
    }

    ByteOrder Image::byteOrder() const
    {
        return byteOrder_;
    }

    int Image::pixelWidth() const
    {
        return pixelWidth_;
    }

    int Image::pixelHeight() const
    {
        return pixelHeight_;
    }

    const ExifData& Image::exifData() const
    {
        return exifData_;
    }

    const IptcData& Image::iptcData() const
    {
        return iptcData_;
    }

    const XmpData& Image::xmpData() const
    {
        return xmpData_;
    }

    std::string Image::comment() const
    {
        return comment_;
    }

    const std::string& Image::xmpPacket() const
    {
        return xmpPacket_;
    }

    BasicIo& Image::io() const
    {
        return *io_;
    }

    bool Image::writeXmpFromPacket() const
    {
        return writeXmpFromPacket_;
    }

    const NativePreviewList& Image::nativePreviews() const
    {
        return nativePreviews_;
    }

    bool Image::good() const
    {
        if (io_->open() != 0)
            return false;
        IoCloser closer(*io_);
        return ImageFactory::checkType(imageType_, *io_, false);
    }

    bool Image::supportsMetadata(MetadataId metadataId) const
    {
        return (supportedMetadata_ & metadataId) != 0;
    }

    AccessMode Image::checkMode(MetadataId metadataId) const
    {
        return ImageFactory::checkMode(imageType_, metadataId);
    }

    const std::string& Image::tagName(uint16_t tag)
    {
        if ( init_ ) {
            int idx;
            const TagInfo* ti ;
            for (ti = Internal::  mnTagList(), idx = 0; ti[idx].tag_ != 0xffff; ++idx) tags_[ti[idx].tag_] = ti[idx].name_;
            for (ti = Internal:: iopTagList(), idx = 0; ti[idx].tag_ != 0xffff; ++idx) tags_[ti[idx].tag_] = ti[idx].name_;
            for (ti = Internal:: gpsTagList(), idx = 0; ti[idx].tag_ != 0xffff; ++idx) tags_[ti[idx].tag_] = ti[idx].name_;
            for (ti = Internal:: ifdTagList(), idx = 0; ti[idx].tag_ != 0xffff; ++idx) tags_[ti[idx].tag_] = ti[idx].name_;
            for (ti = Internal::exifTagList(), idx = 0; ti[idx].tag_ != 0xffff; ++idx) tags_[ti[idx].tag_] = ti[idx].name_;
            for (ti = Internal:: mpfTagList(), idx = 0; ti[idx].tag_ != 0xffff; ++idx) tags_[ti[idx].tag_] = ti[idx].name_;
            for (ti = Internal::Nikon1MakerNote::tagList(), idx = 0
                                                    ; ti[idx].tag_ != 0xffff; ++idx) tags_[ti[idx].tag_] = ti[idx].name_;
        }
        init_ = false;

        return tags_[tag] ;
    }

    AccessMode ImageFactory::checkMode(int type, MetadataId metadataId)
    {
        const Registry* r = find(registry, type);
        if (!r) throw Error(kerUnsupportedImageType, type);
        AccessMode am = amNone;
        switch (metadataId) {
        case mdNone:
            break;
        case mdExif:
            am = r->exifSupport_;
            break;
        case mdIptc:
            am = r->iptcSupport_;
            break;
        case mdXmp:
            am = r->xmpSupport_;
            break;
        case mdComment:
            am = r->commentSupport_;
            break;
        case mdIccProfile: break;

        // no default: let the compiler complain
        }
        return am;
    }

    bool ImageFactory::checkType(int type, BasicIo& io, bool advance)
    {
        const Registry* r = find(registry, type);
        if (nullptr != r) {
            return r->isThisType_(io, advance);
        }
        return false;
    }

    int ImageFactory::getType(const std::string& path)
    {
        FileIo fileIo(path);
        return getType(fileIo);
    }

#ifdef EXV_UNICODE_PATH
    int ImageFactory::getType(const std::wstring& wpath)
    {
        FileIo fileIo(wpath);
        return getType(fileIo);
    }

#endif
    int ImageFactory::getType(const byte* data, long size)
    {
        MemIo memIo(data, size);
        return getType(memIo);
    }

    int ImageFactory::getType(BasicIo& io)
    {
        if (io.open() != 0) return ImageType::none;
        IoCloser closer(io);
        for (unsigned int i = 0; registry[i].imageType_ != ImageType::none; ++i) {
            if (registry[i].isThisType_(io, false)) {
                return registry[i].imageType_;
            }
        }
        return ImageType::none;
    } // ImageFactory::getType

    BasicIo::UniquePtr ImageFactory::createIo(const std::string& path, bool useCurl)
    {
        Protocol fProt = fileProtocol(path);

#ifdef EXV_USE_CURL
        if (useCurl && (fProt == pHttp || fProt == pHttps || fProt == pFtp)) {
            return std::make_unique<CurlIo>(path); // may throw
        }
#endif

        if (fProt == pHttp)
            return std::make_unique<HttpIo>(path); // may throw
        if (fProt == pFileUri)
            return std::make_unique<FileIo>(pathOfFileUrl(path));
        if (fProt == pStdin || fProt == pDataUri)
            return std::make_unique<XPathIo>(path); // may throw

        return std::make_unique<FileIo>(path);

        (void)(useCurl);
    } // ImageFactory::createIo

#ifdef EXV_UNICODE_PATH
    BasicIo::UniquePtr ImageFactory::createIo(const std::wstring& wpath, bool useCurl)
    {
        Protocol fProt = fileProtocol(wpath);
#ifdef EXV_USE_CURL
        if (useCurl && (fProt == pHttp || fProt == pHttps || fProt == pFtp)) {
            return std::make_unique<CurlIo>(wpath);
        }
#endif
        if (fProt == pHttp)
            return std::make_unique<HttpIo>(wpath);
        if (fProt == pFileUri)
            return std::make_unique<FileIo>(pathOfFileUrl(wpath));
        if (fProt == pStdin || fProt == pDataUri)
            return std::make_unique<XPathIo>(wpath); // may throw
        return std::make_unique<FileIo>(wpath);
    }
#endif
    Image::UniquePtr ImageFactory::open(const std::string& path, bool useCurl)
    {
        auto image = open(ImageFactory::createIo(path, useCurl)); // may throw
        if (!image)
            throw Error(kerFileContainsUnknownImageType, path);
        return image;
    }

#ifdef EXV_UNICODE_PATH
    Image::UniquePtr ImageFactory::open(const std::wstring& wpath, bool useCurl)
    {
        auto image = open(ImageFactory::createIo(wpath, useCurl)); // may throw
        if (!image)
            throw WError(kerFileContainsUnknownImageType, wpath);
        return image;
    }

#endif
    Image::UniquePtr ImageFactory::open(const byte* data, long size)
    {
        auto io = std::make_unique<MemIo>(data, size);
        auto image = open(std::move(io)); // may throw
        if (!image)
            throw Error(kerMemoryContainsUnknownImageType);
        return image;
    }

    Image::UniquePtr ImageFactory::open(BasicIo::UniquePtr io)
    {
        if (io->open() != 0) {
            throw Error(kerDataSourceOpenFailed, io->path(), strError());
        }
        for (unsigned int i = 0; registry[i].imageType_ != ImageType::none; ++i) {
            if (registry[i].isThisType_(*io, false)) {
                return registry[i].newInstance_(std::move(io), false);
            }
        }
        return nullptr;
    }

    Image::UniquePtr ImageFactory::create(int type, const std::string& path)
    {
        auto fileIo = std::make_unique<FileIo>(path);
        // Create or overwrite the file, then close it
        if (fileIo->open("w+b") != 0) {
            throw Error(kerFileOpenFailed, path, "w+b", strError());
        }
        fileIo->close();

        BasicIo::UniquePtr io(std::move(fileIo));
        auto image = create(type, std::move(io));
        if (!image)
            throw Error(kerUnsupportedImageType, type);
        return image;
    }

#ifdef EXV_UNICODE_PATH
    Image::UniquePtr ImageFactory::create(int type,
                                        const std::wstring& wpath)
    {
        auto fileIo = std::make_unique<FileIo>(wpath);
        // Create or overwrite the file, then close it
        if (fileIo->open("w+b") != 0) {
            throw WError(kerFileOpenFailed, wpath, "w+b", strError().c_str());
        }
        fileIo->close();

        BasicIo::UniquePtr io(std::move(fileIo));
        auto image = create(type, std::move(io));
        if (!image)
            throw Error(kerUnsupportedImageType, type);
        return image;
    }

#endif
    Image::UniquePtr ImageFactory::create(int type)
    {
        auto io = std::make_unique<MemIo>();
        auto image = create(type, std::move(io));
        if (!image)
            throw Error(kerUnsupportedImageType, type);
        return image;
    }

    Image::UniquePtr ImageFactory::create(int type, BasicIo::UniquePtr io)
    {
        // BasicIo instance does not need to be open
        const Registry* r = find(registry, type);
        if (nullptr != r) {
            return r->newInstance_(std::move(io), true);
        }
        return nullptr;
    }

// *****************************************************************************
// template, inline and free functions

    void append(Blob& blob, const byte* buf, uint32_t len)
    {
        if (len != 0) {
            assert(buf != 0);
            Blob::size_type size = blob.size();
            if (blob.capacity() - size < len) {
                blob.reserve(size + 65536);
            }
            blob.resize(size + len);
            std::memcpy(&blob[size], buf, len);
        }
    } // append

}                                       // namespace Exiv2
