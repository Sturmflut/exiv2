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
  File:      easyaccess.cpp
 */
// *****************************************************************************
// included header files
#include "easyaccess.hpp"

// *****************************************************************************
namespace {

    using namespace Exiv2;

    /*!
      @brief Search \em ed for a Metadatum specified by the \em keys.
             The \em keys are searched in the order of their appearance, the
             first available Metadatum is returned.

      @param ed The %Exif metadata container to search
      @param keys Array of keys to look for
      @param count Number of elements in the array
     */
    ExifData::const_iterator findMetadatum(const ExifData& ed,
                                           const char* keys[],
                                           int count)
    {
        for (int i = 0; i < count; ++i) {
            auto pos = ed.findKey(ExifKey(keys[i]));
            if (pos != ed.end()) return pos;
        }
        return ed.end();
    } // findMetadatum

} // anonymous namespace

// *****************************************************************************
// class member definitions
namespace Exiv2 {

    ExifData::const_iterator orientation(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Image.Orientation",
            "Exif.Panasonic.Rotation",
            "Exif.MinoltaCs5D.Rotation",
            "Exif.MinoltaCs5D.Rotation2",
            "Exif.MinoltaCs7D.Rotation",
            "Exif.Sony1MltCsA100.Rotation",
            "Exif.Sony1Cs.Rotation",
            "Exif.Sony2Cs.Rotation",
            "Exif.Sony1Cs2.Rotation",
            "Exif.Sony2Cs2.Rotation",
            "Exif.Sony1MltCsA100.Rotation"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator isoSpeed(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.ISOSpeedRatings",
            "Exif.Image.ISOSpeedRatings",
            "Exif.CanonSi.ISOSpeed",
            "Exif.CanonCs.ISOSpeed",
            "Exif.Nikon1.ISOSpeed",
            "Exif.Nikon2.ISOSpeed",
            "Exif.Nikon3.ISOSpeed",
            "Exif.NikonIi.ISO",
            "Exif.NikonIi.ISO2",
            "Exif.MinoltaCsNew.ISOSetting",
            "Exif.MinoltaCsOld.ISOSetting",
            "Exif.MinoltaCs5D.ISOSpeed",
            "Exif.MinoltaCs7D.ISOSpeed",
            "Exif.Sony1Cs.ISOSetting",
            "Exif.Sony2Cs.ISOSetting",
            "Exif.Sony1Cs2.ISOSetting",
            "Exif.Sony2Cs2.ISOSetting",
            "Exif.Sony1MltCsA100.ISOSetting",
            "Exif.Pentax.ISO",
            "Exif.PentaxDng.ISO",
            "Exif.Olympus.ISOSpeed",
            "Exif.Samsung2.ISO",
            "Exif.Casio.ISO",
            "Exif.Casio2.ISO",
            "Exif.Casio2.ISOSpeed"
        };

        struct SensKeyNameList {
            int count;
            const char* keys[3];
        };

        // covers Exif.Phot.SensitivityType values 1-7. Note that SOS, REI and
        // ISO do differ in their meaning. Values coming first in a list (and
        // existing as a tag) are picked up first and used as the "ISO" value.
        static const SensKeyNameList sensitivityKey[] = {
            { 1, { "Exif.Photo.StandardOutputSensitivity" }},
            { 1, { "Exif.Photo.RecommendedExposureIndex" }},
            { 1, { "Exif.Photo.ISOSpeed" }},
            { 2, { "Exif.Photo.RecommendedExposureIndex", "Exif.Photo.StandardOutputSensitivity" }},
            { 2, { "Exif.Photo.ISOSpeed", "Exif.Photo.StandardOutputSensitivity" }},
            { 2, { "Exif.Photo.ISOSpeed", "Exif.Photo.RecommendedExposureIndex" }},
            { 3, { "Exif.Photo.ISOSpeed", "Exif.Photo.RecommendedExposureIndex", "Exif.Photo.StandardOutputSensitivity" }}
        };

        static const char* sensitivityType[] = {
            "Exif.Photo.SensitivityType"
        };

        // Find the first ISO value which is not "0"
        const int cnt = EXV_COUNTOF(keys);
        auto md = ed.end();
        long iso_val = -1;
        for (int idx = 0; idx < cnt; ) {
            md = findMetadatum(ed, keys + idx, cnt - idx);
            if (md == ed.end()) break;
            std::ostringstream os;
            md->write(os, &ed);
            bool ok = false;
            iso_val = parseLong(os.str(), ok);
            if (ok && iso_val > 0) break;
            while (strcmp(keys[idx++], md->key().c_str()) != 0 && idx < cnt) {}
            md = ed.end();
        }

        // there is either a possible ISO "overflow" or no legacy
        // ISO tag at all. Check for SensitivityType tag and the referenced
        // ISO value (see EXIF 2.3 Annex G)
        long iso_tmp_val = -1;
        while (iso_tmp_val == -1 && (iso_val == 65535 || md == ed.end())) {
            auto md_st = findMetadatum(ed, sensitivityType, 1);
            // no SensitivityType? exit with existing data
            if (md_st == ed.end())
                break;
            // otherwise pick up actual value and grab value accordingly
            std::ostringstream os;
            md_st->write(os, &ed);
            bool ok = false;
            const long st_val = parseLong(os.str(), ok);
            // SensitivityType out of range or cannot be parsed properly
            if (!ok || st_val < 1 || st_val > 7)
                break;
            // pick up list of ISO tags, and check for at least one of
            // them available.
            const SensKeyNameList *sensKeys = &sensitivityKey[st_val - 1];
            md_st = ed.end();
            for (int idx = 0; idx < sensKeys->count; md_st = ed.end()) {
                md_st = findMetadatum(ed, const_cast<const char**>(sensKeys->keys), sensKeys->count);
                if (md_st == ed.end())
                    break;
                std::ostringstream os_iso;
                md_st->write(os_iso, &ed);
                ok = false;
                iso_tmp_val = parseLong(os_iso.str(), ok);
                // something wrong with the value
                if (ok || iso_tmp_val > 0) {
                    md = md_st;
                    break;
                }
                while (strcmp(sensKeys->keys[idx++], md_st->key().c_str()) != 0 && idx < sensKeys->count) {}
            }
            break;
        }

        return md;
    }

    ExifData::const_iterator dateTimeOriginal(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.DateTimeOriginal",
            "Exif.Image.DateTimeOriginal"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator flashBias(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.CanonSi.FlashBias",
            "Exif.Panasonic.FlashBias",
            "Exif.Olympus.FlashBias",
            "Exif.OlympusCs.FlashExposureComp",
            "Exif.Minolta.FlashExposureComp",
            "Exif.SonyMinolta.FlashExposureComp",
            "Exif.Sony1.FlashExposureComp",
            "Exif.Sony2.FlashExposureComp"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator exposureMode(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.ExposureProgram",
            "Exif.Image.ExposureProgram",
            "Exif.CanonCs.ExposureProgram",
            "Exif.MinoltaCs7D.ExposureMode",
            "Exif.MinoltaCs5D.ExposureMode",
            "Exif.MinoltaCsNew.ExposureMode",
            "Exif.MinoltaCsOld.ExposureMode",
            "Exif.Sony1MltCsA100.ExposureMode",
            "Exif.Sony1Cs.ExposureProgram",
            "Exif.Sony2Cs.ExposureProgram",
            "Exif.Sigma.ExposureMode"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator sceneMode(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.CanonCs.EasyMode",
            "Exif.Fujifilm.PictureMode",
            "Exif.MinoltaCsNew.SubjectProgram",
            "Exif.MinoltaCsOld.SubjectProgram",
            "Exif.Minolta.SceneMode",
            "Exif.SonyMinolta.SceneMode",
            "Exif.Sony1.SceneMode",
            "Exif.Sony2.SceneMode",
            "Exif.OlympusCs.SceneMode",
            "Exif.Panasonic.ShootingMode",
            "Exif.Panasonic.SceneMode",
            "Exif.Pentax.PictureMode",
            "Exif.PentaxDng.PictureMode",
            "Exif.Photo.SceneCaptureType"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator macroMode(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.CanonCs.Macro",
            "Exif.Fujifilm.Macro",
            "Exif.Olympus.Macro",
            "Exif.OlympusCs.MacroMode",
            "Exif.Panasonic.Macro",
            "Exif.MinoltaCsNew.MacroMode",
            "Exif.MinoltaCsOld.MacroMode",
            "Exif.Sony1.Macro",
            "Exif.Sony2.Macro"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator imageQuality(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.CanonCs.Quality",
            "Exif.Fujifilm.Quality",
            "Exif.Sigma.Quality",
            "Exif.Nikon1.Quality",
            "Exif.Nikon2.Quality",
            "Exif.Nikon3.Quality",
            "Exif.Olympus.Quality",
            "Exif.OlympusCs.Quality",
            "Exif.Panasonic.Quality",
            "Exif.Minolta.Quality",
            "Exif.MinoltaCsNew.Quality",
            "Exif.MinoltaCsOld.Quality",
            "Exif.MinoltaCs5D.Quality",
            "Exif.MinoltaCs7D.Quality",
            "Exif.Sony1MltCsA100.Quality",
            "Exif.Sony1.JPEGQuality",
            "Exif.Sony1.Quality",
            "Exif.Sony1Cs.Quality",
            "Exif.Sony2.JPEGQuality",
            "Exif.Sony2.Quality",
            "Exif.Sony2Cs.Quality",
            "Exif.Casio.Quality",
            "Exif.Casio2.QualityMode",
            "Exif.Casio2.Quality"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator whiteBalance(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.CanonSi.WhiteBalance",
            "Exif.Fujifilm.WhiteBalance",
            "Exif.Sigma.WhiteBalance",
            "Exif.Nikon1.WhiteBalance",
            "Exif.Nikon2.WhiteBalance",
            "Exif.Nikon3.WhiteBalance",
            "Exif.Olympus.WhiteBalance",
            "Exif.OlympusCs.WhiteBalance",
            "Exif.Panasonic.WhiteBalance",
            "Exif.MinoltaCs5D.WhiteBalance",
            "Exif.MinoltaCs7D.WhiteBalance",
            "Exif.MinoltaCsNew.WhiteBalance",
            "Exif.MinoltaCsOld.WhiteBalance",
            "Exif.Minolta.WhiteBalance",
            "Exif.Sony1MltCsA100.WhiteBalance",
            "Exif.SonyMinolta.WhiteBalance",
            "Exif.Sony1.WhiteBalance",
            "Exif.Sony2.WhiteBalance",
            "Exif.Sony1.WhiteBalance2",
            "Exif.Sony2.WhiteBalance2",
            "Exif.Casio.WhiteBalance",
            "Exif.Casio2.WhiteBalance",
            "Exif.Casio2.WhiteBalance2",
            "Exif.Photo.WhiteBalance"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator lensName(const ExifData& ed)
    {
        static const char* keys[] = {
            // Exif.Canon.LensModel only reports focal length.
            // Try Exif.CanonCs.LensType first.
            "Exif.CanonCs.LensType",
            "Exif.Photo.LensModel",
            "Exif.NikonLd1.LensIDNumber",
            "Exif.NikonLd2.LensIDNumber",
            "Exif.NikonLd3.LensIDNumber",
            "Exif.Pentax.LensType",
            "Exif.PentaxDng.LensType",
            "Exif.Minolta.LensID",
            "Exif.SonyMinolta.LensID",
            "Exif.Sony1.LensID",
            "Exif.Sony2.LensID",
            "Exif.OlympusEq.LensType",
            "Exif.Panasonic.LensType",
            "Exif.Samsung2.LensType"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator saturation(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.Saturation",
            "Exif.CanonCs.Saturation",
            "Exif.MinoltaCsNew.Saturation",
            "Exif.MinoltaCsOld.Saturation",
            "Exif.MinoltaCs7D.Saturation",
            "Exif.MinoltaCs5D.Saturation",
            "Exif.Fujifilm.Color",
            "Exif.Nikon3.Saturation",
            "Exif.Panasonic.Saturation",
            "Exif.Pentax.Saturation",
            "Exif.PentaxDng.Saturation",
            "Exif.Sigma.Saturation",
            "Exif.Casio.Saturation",
            "Exif.Casio2.Saturation",
            "Exif.Casio2.Saturation2"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator sharpness(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.Sharpness",
            "Exif.CanonCs.Sharpness",
            "Exif.Fujifilm.Sharpness",
            "Exif.MinoltaCsNew.Sharpness",
            "Exif.MinoltaCsOld.Sharpness",
            "Exif.MinoltaCs7D.Sharpness",
            "Exif.MinoltaCs5D.Sharpness",
            "Exif.Olympus.SharpnessFactor",
            "Exif.Panasonic.Sharpness",
            "Exif.Pentax.Sharpness",
            "Exif.PentaxDng.Sharpness",
            "Exif.Sigma.Sharpness",
            "Exif.Casio.Sharpness",
            "Exif.Casio2.Sharpness",
            "Exif.Casio2.Sharpness2"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator contrast(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.Contrast",
            "Exif.CanonCs.Contrast",
            "Exif.Fujifilm.Tone",
            "Exif.MinoltaCsNew.Contrast",
            "Exif.MinoltaCsOld.Contrast",
            "Exif.MinoltaCs7D.Contrast",
            "Exif.MinoltaCs5D.Contrast",
            "Exif.Olympus.Contrast",
            "Exif.Panasonic.Contrast",
            "Exif.Pentax.Contrast",
            "Exif.PentaxDng.Contrast",
            "Exif.Sigma.Contrast",
            "Exif.Casio.Contrast",
            "Exif.Casio2.Contrast",
            "Exif.Casio2.Contrast2"

        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator sceneCaptureType(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.SceneCaptureType",
            "Exif.Olympus.SpecialMode"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator meteringMode(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.MeteringMode",
            "Exif.Image.MeteringMode",
            "Exif.CanonCs.MeteringMode",
            "Exif.Sony1MltCsA100.MeteringMode"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator make(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Image.Make"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator model(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Image.Model"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator exposureTime(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.ExposureTime",
            "Exif.Image.ExposureTime",
            "Exif.Samsung2.ExposureTime"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator fNumber(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.FNumber",
            "Exif.Image.FNumber",
            "Exif.Samsung2.FNumber"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator shutterSpeedValue(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.ShutterSpeedValue",
            "Exif.Image.ShutterSpeedValue"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator apertureValue(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.ApertureValue",
            "Exif.Image.ApertureValue"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator brightnessValue(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.BrightnessValue",
            "Exif.Image.BrightnessValue"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator exposureBiasValue(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.ExposureBiasValue",
            "Exif.Image.ExposureBiasValue"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator maxApertureValue(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.MaxApertureValue",
            "Exif.Image.MaxApertureValue"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator subjectDistance(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.SubjectDistance",
            "Exif.Image.SubjectDistance",
            "Exif.CanonSi.SubjectDistance",
            "Exif.CanonFi.FocusDistanceUpper",
            "Exif.CanonFi.FocusDistanceLower",
            "Exif.MinoltaCsNew.FocusDistance",
            "Exif.Nikon1.FocusDistance",
            "Exif.Nikon3.FocusDistance",
            "Exif.NikonLd2.FocusDistance",
            "Exif.NikonLd3.FocusDistance",
            "Exif.Olympus.FocusDistance",
            "Exif.OlympusFi.FocusDistance",
            "Exif.Casio.ObjectDistance",
            "Exif.Casio2.ObjectDistance"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator lightSource(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.LightSource",
            "Exif.Image.LightSource"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator flash(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.Flash",
            "Exif.Image.Flash"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator serialNumber(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Image.CameraSerialNumber",
            "Exif.Canon.SerialNumber",
            "Exif.Nikon3.SerialNumber",
            "Exif.Nikon3.SerialNO",
            "Exif.Fujifilm.SerialNumber",
            "Exif.Olympus.SerialNumber2",
            "Exif.Sigma.SerialNumber"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator focalLength(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.FocalLength",
            "Exif.Image.FocalLength",
            "Exif.Canon.FocalLength",
            "Exif.NikonLd2.FocalLength",
            "Exif.NikonLd3.FocalLength",
            "Exif.MinoltaCsNew.FocalLength",
            "Exif.Pentax.FocalLength",
            "Exif.PentaxDng.FocalLength",
            "Exif.Casio2.FocalLength"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator subjectArea(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.SubjectArea",
            "Exif.Image.SubjectLocation"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator flashEnergy(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.FlashEnergy",
            "Exif.Image.FlashEnergy"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator exposureIndex(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.ExposureIndex",
            "Exif.Image.ExposureIndex"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator sensingMethod(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.SensingMethod",
            "Exif.Image.SensingMethod"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

    ExifData::const_iterator afPoint(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.CanonPi.AFPointsUsed",
            "Exif.CanonPi.AFPointsUsed20D",
            "Exif.CanonSi.AFPointUsed",
            "Exif.CanonCs.AFPoint",
            "Exif.MinoltaCs7D.AFPoints",
            "Exif.Nikon1.AFFocusPos",
            "Exif.NikonAf.AFPoint",
            "Exif.NikonAf.AFPointsInFocus",
            "Exif.NikonAf2.AFPointsUsed",
            "Exif.NikonAf2.PrimaryAFPoint",
            "Exif.OlympusFi.AFPoint",
            "Exif.Pentax.AFPoint",
            "Exif.Pentax.AFPointInFocus",
            "Exif.PentaxDng.AFPoint",
            "Exif.PentaxDng.AFPointInFocus",
            "Exif.Sony1Cs.LocalAFAreaPoint",
            "Exif.Sony2Cs.LocalAFAreaPoint",
            "Exif.Sony1Cs2.LocalAFAreaPoint",
            "Exif.Sony2Cs2.LocalAFAreaPoint",
            "Exif.Sony1MltCsA100.LocalAFAreaPoint",
            "Exif.Casio.AFPoint",
            "Exif.Casio2.AFPointPosition"
        };
        return findMetadatum(ed, keys, EXV_COUNTOF(keys));
    }

}                                       // namespace Exiv2
