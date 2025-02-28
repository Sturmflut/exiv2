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
/*
  File:      crwimage.cpp
  Author(s): Andreas Huggel (ahu) <ahuggel@gmx.net>
  History:   28-Aug-05, ahu: created

 */
// *****************************************************************************
// included header files
#include "config.h"

#include "crwimage.hpp"
#include "crwimage_int.hpp"
#include "error.hpp"
#include "futils.hpp"
#include "value.hpp"
#include "tags.hpp"
#include "tags_int.hpp"

// + standard includes
#include <iostream>
#include <iomanip>
#include <stack>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <cassert>


// *****************************************************************************
// class member definitions
namespace Exiv2 {

    using namespace Internal;

    CrwImage::CrwImage(BasicIo::UniquePtr io, bool /*create*/)
        : Image(ImageType::crw, mdExif | mdComment, std::move(io))
    {
    } // CrwImage::CrwImage

    std::string CrwImage::mimeType() const
    {
        return "image/x-canon-crw";
    }

    int CrwImage::pixelWidth() const
    {
        auto widthIter = exifData_.findKey(Exiv2::ExifKey("Exif.Photo.PixelXDimension"));
        if (widthIter != exifData_.end() && widthIter->count() > 0) {
            return widthIter->toLong();
        }
        return 0;
    }

    int CrwImage::pixelHeight() const
    {
        auto heightIter = exifData_.findKey(Exiv2::ExifKey("Exif.Photo.PixelYDimension"));
        if (heightIter != exifData_.end() && heightIter->count() > 0) {
            return heightIter->toLong();
        }
        return 0;
    }

    void CrwImage::setIptcData(const IptcData& /*iptcData*/)
    {
        // not supported
        throw(Error(kerInvalidSettingForImage, "IPTC metadata", "CRW"));
    }

    void CrwImage::readMetadata()
    {
#ifdef EXIV2_DEBUG_MESSAGES
        std::cerr << "Reading CRW file " << io_->path() << "\n";
#endif
        if (io_->open() != 0) {
            throw Error(kerDataSourceOpenFailed, io_->path(), strError());
        }
        IoCloser closer(*io_);
        // Ensure that this is the correct image type
        if (!isCrwType(*io_, false)) {
            if (io_->error() || io_->eof()) throw Error(kerFailedToReadImageData);
            throw Error(kerNotACrwImage);
        }
        clearMetadata();
        DataBuf file(static_cast<long>(io().size()));
        io_->read(file.data(), file.size());

        CrwParser::decode(this, io_->mmap(), static_cast<uint32_t>(io_->size()));

    } // CrwImage::readMetadata

    void CrwImage::writeMetadata()
    {
#ifdef EXIV2_DEBUG_MESSAGES
        std::cerr << "Writing CRW file " << io_->path() << "\n";
#endif
        // Read existing image
        DataBuf buf;
        if (io_->open() == 0) {
            IoCloser closer(*io_);
            // Ensure that this is the correct image type
            if (isCrwType(*io_, false)) {
                // Read the image into a memory buffer
                buf.alloc(static_cast<long>(io_->size()));
                io_->read(buf.data(), buf.size());
                if (io_->error() || io_->eof()) {
                    buf.reset();
                }
            }
        }

        Blob blob;
        CrwParser::encode(blob, buf.c_data(), buf.size(), this);

        // Write new buffer to file
        auto tempIo = std::make_unique<MemIo>();
        tempIo->write((!blob.empty() ? &blob[0] : nullptr), static_cast<long>(blob.size()));
        io_->close();
        io_->transfer(*tempIo); // may throw

    } // CrwImage::writeMetadata

    void CrwParser::decode(CrwImage* pCrwImage, const byte* pData, uint32_t size)
    {
        assert(pCrwImage != 0);
        assert(pData != 0);

        // Parse the image, starting with a CIFF header component
        CiffHeader header;
        header.read(pData, size);
#ifdef EXIV2_DEBUG_MESSAGES
        header.print(std::cerr);
#endif
        header.decode(*pCrwImage);

        // a hack to get absolute offset of preview image inside CRW structure
        CiffComponent* preview = header.findComponent(0x2007, 0x0000);
        if (preview) {
            (pCrwImage->exifData())["Exif.Image2.JPEGInterchangeFormat"] = uint32_t(preview->pData() - pData);
            (pCrwImage->exifData())["Exif.Image2.JPEGInterchangeFormatLength"] = preview->size();
        }
    } // CrwParser::decode

    void CrwParser::encode(
              Blob&     blob,
        const byte*     pData,
              uint32_t  size,
        const CrwImage* pCrwImage
    )
    {
        // Parse image, starting with a CIFF header component
        CiffHeader header;
        if (size != 0) {
            header.read(pData, size);
        }

        // Encode Exif tags from image into the CRW parse tree and write the
        // structure to the binary image blob
        CrwMap::encode(&header, *pCrwImage);
        header.write(blob);
    }

    // *************************************************************************
    // free functions
    Image::UniquePtr newCrwInstance(BasicIo::UniquePtr io, bool create)
    {
        auto image = std::make_unique<CrwImage>(std::move(io), create);
        if (!image->good()) {
            image.reset();
        }
        return image;
    }

    bool isCrwType(BasicIo& iIo, bool advance)
    {
        bool result = true;
        byte tmpBuf[14];
        iIo.read(tmpBuf, 14);
        if (iIo.error() || iIo.eof()) {
            return false;
        }
        if (!(   ('I' == tmpBuf[0] && 'I' == tmpBuf[1])
              || ('M' == tmpBuf[0] && 'M' == tmpBuf[1]))) {
            result = false;
        }
        if (result && std::memcmp(tmpBuf + 6, CiffHeader::signature(), 8) != 0) {
            result = false;
        }
        if (!advance || !result) iIo.seek(-14, BasicIo::cur);
        return result;
    }

}                                       // namespace Exiv2
