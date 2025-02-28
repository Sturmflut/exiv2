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
#ifndef TAGS_HPP_
#define TAGS_HPP_

// *****************************************************************************
#include "exiv2lib_export.h"

// included header files
#include "metadatum.hpp"

// + standard includes
#include <string>
#include <iosfwd>
#include <memory>

// *****************************************************************************
// namespace extensions
namespace Exiv2 {

// *****************************************************************************
// class declarations
    class ExifData;
    class ExifKey;
    class Value;
    struct TagInfo;

// *****************************************************************************
// type definitions

    //! Type for a function pointer for functions interpreting the tag value
    typedef std::ostream& (*PrintFct)(std::ostream&, const Value&, const ExifData* pExifData);
    //! A function returning a tag list.
    typedef const TagInfo* (*TagListFct)();

// *****************************************************************************
// class definitions

    //! The details of an Exif group. Groups include IFDs and binary arrays.
    struct EXIV2API GroupInfo {
        struct GroupName;
        bool operator==(int ifdId) const;        //!< Comparison operator for IFD id
        bool operator==(const GroupName& groupName) const; //!< Comparison operator for group name
        int ifdId_;                              //!< IFD id
        const char* ifdName_;                    //!< IFD name
        const char* groupName_;                  //!< Group name, unique for each group.
        TagListFct tagList_;                     //!< Tag list
    };

    //! Search key to find a GroupInfo by its group name.
    struct EXIV2API GroupInfo::GroupName {
        explicit GroupName(std::string groupName);
        std::string g_;                          //!< Group name
    };

    //! Tag information
    struct EXIV2API TagInfo {
        uint16_t tag_;                          //!< Tag
        const char* name_;                      //!< One word tag label
        const char* title_;                     //!< Tag title
        const char* desc_;                      //!< Short tag description
        int ifdId_;                             //!< Link to the (preferred) IFD
        int sectionId_;                         //!< Section id
        TypeId typeId_;                         //!< Type id
        int16_t count_;                         //!< The number of values (not bytes!), 0=any, -1=count not known.
        PrintFct printFct_;                     //!< Pointer to tag print function
    }; // struct TagInfo

    //! Access to Exif group and tag lists and misc. tag reference methods, implemented as a static class.
    class EXIV2API ExifTags {
    public:
        //! Prevent construction: not implemented.
        ExifTags() = delete;
        //! Prevent copy-construction: not implemented.
        ExifTags(const ExifTags& rhs) = delete;
        //! Prevent assignment: not implemented.
        ExifTags& operator=(const ExifTags& rhs) = delete;

        //! Return read-only list of built-in groups
        static const GroupInfo* groupList();
        //! Return read-only list of built-in \em groupName tags.
        static const TagInfo* tagList(const std::string& groupName);
        //! Print a list of all standard Exif tags to output stream
        static void taglist(std::ostream& os);
        //! Print the list of tags for \em groupName
        static void taglist(std::ostream& os, const std::string& groupName);

        //! Return the name of the section for an Exif \em key.
        static const char* sectionName(const ExifKey& key);
        //! Return the default number of components (not bytes!) \em key has. (0=any, -1=count not known)
        static uint16_t defaultCount(const ExifKey& key);
        //! Return the name of the IFD for the group.
        static const char* ifdName(const std::string& groupName);

        /*!
          @brief Return true if \em groupName is a makernote group.
        */
        static bool isMakerGroup(const std::string& groupName);
        /*!
          @brief Return true if \em groupName is a TIFF or Exif IFD, else false.
                 This is used to differentiate between standard Exif IFDs
                 and IFDs associated with the makernote.
        */
        static bool isExifGroup(const std::string& groupName);

    }; // class ExifTags

    /*!
      @brief Concrete keys for Exif metadata and access to Exif tag reference data.
     */
    class EXIV2API ExifKey : public Key {
    public:
        //! Shortcut for an %ExifKey auto pointer.
        typedef std::unique_ptr<ExifKey> UniquePtr;

        //! @name Creators
        //@{
        /*!
          @brief Constructor to create an Exif key from a key string.

          @param key The key string.
          @throw Error if the first part of the key is not '<b>Exif</b>' or
                 the remaining parts of the key cannot be parsed and
                 converted to a group name and tag name.
        */
        explicit ExifKey(const std::string& key);
        /*!
          @brief Constructor to create an Exif key from the tag number and
                 group name.
          @param tag The tag value
          @param groupName The name of the group, i.e., the second part of
                 the Exif key.
          @throw Error if the key cannot be constructed from the tag number
                 and group name.
         */
        ExifKey(uint16_t tag, const std::string& groupName);
        /*!
          @brief Constructor to create an Exif key from a TagInfo instance.
          @param ti The TagInfo instance
          @throw Error if the key cannot be constructed from the tag number
                 and group name.
         */
        explicit ExifKey(const TagInfo& ti);

        //! Copy constructor
        ExifKey(const ExifKey& rhs);
        //! Destructor
        ~ExifKey() override;
        //@}

        //! @name Manipulators
        //@{
        /*!
          @brief Assignment operator.
         */
        ExifKey& operator=(const ExifKey& rhs);
        //! Set the index.
        void setIdx(int idx);
        //@}

        //! @name Accessors
        //@{
        std::string key() const override;
        const char* familyName() const override;
        std::string groupName() const override;
        //! Return the IFD id as an integer. (Do not use, this is meant for library internal use.)
        int ifdId() const;
        std::string tagName() const override;
        uint16_t tag() const override;
        std::string tagLabel() const override;
        //! Return the tag description.
        std::string tagDesc() const;        // Todo: should be in the base class
        //! Return the default type id for this tag.
        TypeId defaultTypeId() const;       // Todo: should be in the base class

        UniquePtr clone() const;
        //! Return the index (unique id of this key within the original Exif data, 0 if not set)
        int idx() const;
        //@}

    private:
        //! Internal virtual copy constructor.
        ExifKey* clone_() const override;

        // Pimpl idiom
        struct Impl;
        std::unique_ptr<Impl> p_;

    }; // class ExifKey

// *****************************************************************************
// free functions

    //! Output operator for TagInfo
    EXIV2API std::ostream& operator<<(std::ostream& os, const TagInfo& ti);

}                                       // namespace Exiv2

#endif                                  // #ifndef TAGS_HPP_
