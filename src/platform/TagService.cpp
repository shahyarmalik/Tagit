#include "TagService.h"
#include "../core/Logger.h"

#include <QFile>
#include <QFileInfo>

#ifdef TAGIT_HAS_TAGLIB
// Generic TagLib API
#include <fileref.h>
#include <tag.h>
#include <tpropertymap.h>

// Format-specific headers for artwork and extended fields
#include <mpeg/mpegfile.h>
#include <mpeg/id3v2/id3v2tag.h>
#include <mpeg/id3v2/id3v2frame.h>
#include <mpeg/id3v2/frames/attachedpictureframe.h>
#include <mpeg/id3v2/frames/unsynchronizedlyricsframe.h>
#include <mpeg/id3v2/frames/textidentificationframe.h>

#include <flac/flacfile.h>
#include <ogg/xiphcomment.h>
#include <flac/flacpicture.h>

#include <mp4/mp4file.h>
#include <mp4/mp4tag.h>
#include <mp4/mp4coverart.h>

#include <ogg/vorbis/vorbisfile.h>
#include <ogg/opus/opusfile.h>
#endif

namespace tagit {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

#ifdef TAGIT_HAS_TAGLIB
static QString tstr(const TagLib::String &s)
{
    return QString::fromUtf8(s.toCString(true));
}

static TagLib::String qstr(const QString &s)
{
    return TagLib::String(s.toUtf8().constData(), TagLib::String::UTF8);
}

/// Read a single value from a PropertyMap, return empty if absent.
static QString propValue(const TagLib::PropertyMap &props, const char *key)
{
    const TagLib::StringList &list = props[key];
    if (list.isEmpty()) return {};
    return tstr(list.front());
}
#endif

// ---------------------------------------------------------------------------
// readTags
// ---------------------------------------------------------------------------

AudioMetadata TagService::readTags(const QString &filePath) const
{
    AudioMetadata md;
#ifdef TAGIT_HAS_TAGLIB
    TagLib::FileRef ref(filePath.toUtf8().constData());
    if (ref.isNull() || !ref.file()) return md;

    // ---- Basic tags ----
    if (ref.tag()) {
        TagLib::Tag *t = ref.tag();
        md.title       = tstr(t->title());
        md.artist      = tstr(t->artist());
        md.album       = tstr(t->album());
        md.genre       = tstr(t->genre());
        md.comment     = tstr(t->comment());
        md.trackNumber = static_cast<int>(t->track());
        md.year        = static_cast<int>(t->year());
    }

    // ---- Audio properties ----
    if (ref.audioProperties()) {
        md.durationMs = ref.audioProperties()->lengthInMilliseconds();
        md.bitrate    = ref.audioProperties()->bitrate();
    }

    // ---- Extended tags via PropertyMap ----
    const TagLib::PropertyMap props = ref.file()->properties();
    md.albumArtist = propValue(props, "ALBUMARTIST");
    if (md.albumArtist.isEmpty())
        md.albumArtist = propValue(props, "ALBUM ARTIST");
    md.composer    = propValue(props, "COMPOSER");
    md.lyrics      = propValue(props, "LYRICS");
    if (md.lyrics.isEmpty())
        md.lyrics  = propValue(props, "UNSYNCEDLYRICS");
    md.isrc        = propValue(props, "ISRC");
    md.copyright   = propValue(props, "COPYRIGHT");
    md.publisher   = propValue(props, "ORGANIZATION");
    if (md.publisher.isEmpty())
        md.publisher = propValue(props, "PUBLISHER");
    md.discNumber  = propValue(props, "DISCNUMBER").split('/').first().toInt();

    // ---- Artwork detection & extraction ----
    md.artworkData = extractArtwork(filePath);
    md.hasEmbeddedArtwork = !md.artworkData.isEmpty();
#else
    Q_UNUSED(filePath)
#endif
    return md;
}

// ---------------------------------------------------------------------------
// extractArtwork
// ---------------------------------------------------------------------------

QByteArray TagService::extractArtwork(const QString &filePath, QString *outMimeType) const
{
#ifdef TAGIT_HAS_TAGLIB
    TagLib::FileRef ref(filePath.toUtf8().constData());
    if (ref.isNull() || !ref.file()) return {};

    // MP3 / ID3v2
    if (auto *mp3 = dynamic_cast<TagLib::MPEG::File *>(ref.file())) {
        if (mp3->ID3v2Tag()) {
            const auto &frames = mp3->ID3v2Tag()->frameListMap()["APIC"];
            if (!frames.isEmpty()) {
                auto *pic = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(frames.front());
                if (pic) {
                    if (outMimeType) *outMimeType = tstr(pic->mimeType());
                    return QByteArray(pic->picture().data(), static_cast<int>(pic->picture().size()));
                }
            }
        }
    }
    // FLAC
    else if (auto *flac = dynamic_cast<TagLib::FLAC::File *>(ref.file())) {
        if (!flac->pictureList().isEmpty()) {
            auto *pic = flac->pictureList().front();
            if (pic) {
                if (outMimeType) *outMimeType = tstr(pic->mimeType());
                return QByteArray(pic->data().data(), static_cast<int>(pic->data().size()));
            }
        }
    }
    // MP4
    else if (auto *mp4 = dynamic_cast<TagLib::MP4::File *>(ref.file())) {
        if (mp4->tag()) {
            const auto &items = mp4->tag()->itemMap();
            if (items.contains("covr")) {
                TagLib::MP4::CoverArtList artList = items["covr"].toCoverArtList();
                if (!artList.isEmpty()) {
                    if (outMimeType) {
                        *outMimeType = (artList.front().format() == TagLib::MP4::CoverArt::PNG)
                                           ? "image/png"
                                           : "image/jpeg";
                    }
                    return QByteArray(artList.front().data().data(), static_cast<int>(artList.front().data().size()));
                }
            }
        }
    }
#else
    Q_UNUSED(filePath)
    Q_UNUSED(outMimeType)
#endif
    return {};
}

// ---------------------------------------------------------------------------
// writeArtwork
// ---------------------------------------------------------------------------

bool TagService::writeArtwork(const QString    &filePath,
                              const QByteArray &artworkData,
                              const QString    &mimeType,
                              bool              backupOriginal) const
{
#ifdef TAGIT_HAS_TAGLIB
    QFileInfo info(filePath);
    if (!info.exists()) return false;

    if (backupOriginal) {
        const QString bak = filePath + ".bak";
        if (!QFile::exists(bak)) {
            QFile::copy(filePath, bak);
        }
    }

    TagLib::FileRef ref(filePath.toUtf8().constData());
    if (ref.isNull() || !ref.file()) return false;

    bool changed = false;

    // MP3 / ID3v2
    if (auto *mp3 = dynamic_cast<TagLib::MPEG::File *>(ref.file())) {
        TagLib::ID3v2::Tag *id3 = mp3->ID3v2Tag(true);
        if (id3) {
            id3->removeFrames("APIC");
            if (!artworkData.isEmpty()) {
                auto *pic = new TagLib::ID3v2::AttachedPictureFrame();
                pic->setMimeType(qstr(mimeType.isEmpty() ? "image/jpeg" : mimeType));
                pic->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
                pic->setPicture(TagLib::ByteVector(
                    artworkData.constData(),
                    static_cast<unsigned int>(artworkData.size())));
                id3->addFrame(pic);
            }
            changed = true;
        }
    }
    // FLAC
    else if (auto *flac = dynamic_cast<TagLib::FLAC::File *>(ref.file())) {
        flac->removePictures();
        if (!artworkData.isEmpty()) {
            auto *pic = new TagLib::FLAC::Picture();
            pic->setMimeType(qstr(mimeType.isEmpty() ? "image/jpeg" : mimeType));
            pic->setType(TagLib::FLAC::Picture::FrontCover);
            pic->setData(TagLib::ByteVector(
                artworkData.constData(),
                static_cast<unsigned int>(artworkData.size())));
            flac->addPicture(pic);
        }
        changed = true;
    }
    // MP4
    else if (auto *mp4 = dynamic_cast<TagLib::MP4::File *>(ref.file())) {
        if (mp4->tag()) {
            if (artworkData.isEmpty()) {
                mp4->tag()->removeItem("covr");
            } else {
                TagLib::MP4::CoverArt::Format fmt = mimeType.contains("png", Qt::CaseInsensitive)
                                                        ? TagLib::MP4::CoverArt::PNG
                                                        : TagLib::MP4::CoverArt::JPEG;
                TagLib::MP4::CoverArt art(
                    fmt,
                    TagLib::ByteVector(
                        artworkData.constData(),
                        static_cast<unsigned int>(artworkData.size())));
                TagLib::MP4::CoverArtList list;
                list.append(art);
                mp4->tag()->setItem("covr", TagLib::MP4::Item(list));
            }
            changed = true;
        }
    }

    if (!changed) return false;

    if (!ref.save()) {
        Logger::warn("TagLib save() failed for artwork: " + filePath);
        return false;
    }

    Logger::info(QString("Artwork updated for: %1 (size: %2 bytes)").arg(filePath).arg(artworkData.size()));
    return true;
#else
    Q_UNUSED(filePath)
    Q_UNUSED(artworkData)
    Q_UNUSED(mimeType)
    Q_UNUSED(backupOriginal)
    return false;
#endif
}

// ---------------------------------------------------------------------------
// removeArtwork
// ---------------------------------------------------------------------------

bool TagService::removeArtwork(const QString &filePath, bool backupOriginal) const
{
    return writeArtwork(filePath, QByteArray(), QString(), backupOriginal);
}

// ---------------------------------------------------------------------------
// writeMissingTags
// ---------------------------------------------------------------------------

bool TagService::writeMissingTags(const QString      &filePath,
                                  const AudioMetadata &md,
                                  bool                backupOriginal) const
{
#ifdef TAGIT_HAS_TAGLIB
    QFileInfo info(filePath);
    if (!info.exists()) return false;

    if (backupOriginal) {
        const QString bak = filePath + ".bak";
        if (!QFile::exists(bak)) {
            QFile::copy(filePath, bak);
        }
    }

    TagLib::FileRef ref(filePath.toUtf8().constData());
    if (ref.isNull() || !ref.file()) return false;

    bool changed = false;

    // ---- Basic tags ----
    if (TagLib::Tag *t = ref.tag()) {
        auto writeStr = [&](const QString &val,
                            void (TagLib::Tag::*setter)(const TagLib::String &),
                            const TagLib::String &current) {
            if (!val.isEmpty() && current.isEmpty()) {
                (t->*setter)(qstr(val));
                changed = true;
            }
        };
        writeStr(md.title,  &TagLib::Tag::setTitle,  t->title());
        writeStr(md.artist, &TagLib::Tag::setArtist, t->artist());
        writeStr(md.album,  &TagLib::Tag::setAlbum,  t->album());
        writeStr(md.genre,  &TagLib::Tag::setGenre,  t->genre());

        if (md.trackNumber > 0 && t->track() == 0) {
            t->setTrack(static_cast<unsigned int>(md.trackNumber));
            changed = true;
        }
        if (md.year > 0 && t->year() == 0) {
            t->setYear(static_cast<unsigned int>(md.year));
            changed = true;
        }
    }

    // ---- Extended tags via PropertyMap ----
    TagLib::PropertyMap props = ref.file()->properties();

    auto setProp = [&](const char *key, const QString &val) {
        if (!val.isEmpty() && props[key].isEmpty()) {
            props[key] = TagLib::StringList(qstr(val));
            changed = true;
        }
    };

    setProp("ALBUMARTIST",    md.albumArtist);
    setProp("COMPOSER",       md.composer);
    setProp("LYRICS",         md.lyrics);
    setProp("ISRC",           md.isrc);
    setProp("COPYRIGHT",      md.copyright);
    setProp("ORGANIZATION",   md.publisher);

    if (md.discNumber > 0 && props["DISCNUMBER"].isEmpty()) {
        props["DISCNUMBER"] = TagLib::StringList(
            qstr(QString::number(md.discNumber)));
        changed = true;
    }

    if (changed) {
        ref.file()->setProperties(props);
    }

    // ---- Artwork (MP3 only for now — most common format) ----
    if (!md.artworkData.isEmpty() && !md.hasEmbeddedArtwork) {
        if (auto *mp3 = dynamic_cast<TagLib::MPEG::File *>(ref.file())) {
            TagLib::ID3v2::Tag *id3 = mp3->ID3v2Tag(true);
            if (id3 && id3->frameListMap()["APIC"].isEmpty()) {
                auto *pic = new TagLib::ID3v2::AttachedPictureFrame();
                pic->setMimeType("image/jpeg");
                pic->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
                pic->setPicture(TagLib::ByteVector(
                    md.artworkData.constData(),
                    static_cast<unsigned int>(md.artworkData.size())));
                id3->addFrame(pic);
                changed = true;
            }
        }
        // FLAC artwork
        else if (auto *flac = dynamic_cast<TagLib::FLAC::File *>(ref.file())) {
            if (flac->pictureList().isEmpty()) {
                auto *pic = new TagLib::FLAC::Picture();
                pic->setMimeType("image/jpeg");
                pic->setType(TagLib::FLAC::Picture::FrontCover);
                pic->setData(TagLib::ByteVector(
                    md.artworkData.constData(),
                    static_cast<unsigned int>(md.artworkData.size())));
                flac->addPicture(pic);
                changed = true;
            }
        }
        // MP4 artwork
        else if (auto *mp4 = dynamic_cast<TagLib::MP4::File *>(ref.file())) {
            if (mp4->tag()) {
                auto &items = mp4->tag()->itemMap();
                if (!items.contains("covr")) {
                    TagLib::MP4::CoverArt art(
                        TagLib::MP4::CoverArt::JPEG,
                        TagLib::ByteVector(
                            md.artworkData.constData(),
                            static_cast<unsigned int>(md.artworkData.size())));
                    TagLib::MP4::CoverArtList list;
                    list.append(art);
                    mp4->tag()->setItem("covr",
                        TagLib::MP4::Item(list));
                    changed = true;
                }
            }
        }
    }

    if (!changed) return false;

    if (!ref.save()) {
        Logger::warn("TagLib save() failed for: " + filePath);
        return false;
    }
    return true;
#else
    Q_UNUSED(filePath)
    Q_UNUSED(md)
    Q_UNUSED(backupOriginal)
    return false;
#endif
}

bool TagService::isAvailable() const
{
#ifdef TAGIT_HAS_TAGLIB
    return true;
#else
    return false;
#endif
}

// ---------------------------------------------------------------------------
// writeSelectedTags  — explicit user-save, overwrites chosen fields
// ---------------------------------------------------------------------------

bool TagService::writeSelectedTags(const QString       &filePath,
                                   const AudioMetadata &md,
                                   const QStringList   &fields) const
{
#ifdef TAGIT_HAS_TAGLIB
    QFileInfo info(filePath);
    if (!info.exists() || fields.isEmpty()) return false;

    // Always back up before an explicit overwrite
    const QString bak = filePath + ".bak";
    if (!QFile::exists(bak)) {
        QFile::copy(filePath, bak);
    }

    TagLib::FileRef ref(filePath.toUtf8().constData());
    if (ref.isNull() || !ref.file()) return false;

    bool changed = false;

    // ---- Basic tags ----
    if (TagLib::Tag *t = ref.tag()) {
        if (fields.contains("title") && !md.title.isEmpty()) {
            t->setTitle(qstr(md.title));  changed = true;
        }
        if (fields.contains("artist") && !md.artist.isEmpty()) {
            t->setArtist(qstr(md.artist));  changed = true;
        }
        if (fields.contains("album") && !md.album.isEmpty()) {
            t->setAlbum(qstr(md.album));  changed = true;
        }
        if (fields.contains("genre") && !md.genre.isEmpty()) {
            t->setGenre(qstr(md.genre));  changed = true;
        }
        if (fields.contains("comment")) {
            t->setComment(qstr(md.comment));  changed = true;
        }
        if (fields.contains("trackNumber") && md.trackNumber >= 0) {
            t->setTrack(static_cast<unsigned int>(md.trackNumber));  changed = true;
        }
        if (fields.contains("year") && md.year >= 0) {
            t->setYear(static_cast<unsigned int>(md.year));  changed = true;
        }
    }

    // ---- Extended tags via PropertyMap ----
    TagLib::PropertyMap props = ref.file()->properties();

    auto setProp = [&](const char *key, const QString &val) {
        props[key] = TagLib::StringList(qstr(val));
        changed = true;
    };

    if (fields.contains("albumArtist"))
        setProp("ALBUMARTIST", md.albumArtist);
    if (fields.contains("composer"))
        setProp("COMPOSER", md.composer);
    if (fields.contains("lyrics"))
        setProp("LYRICS", md.lyrics);
    if (fields.contains("discNumber")) {
        props["DISCNUMBER"] = TagLib::StringList(
            qstr(md.discNumber > 0 ? QString::number(md.discNumber) : QString()));
        changed = true;
    }

    if (changed) {
        ref.file()->setProperties(props);
    }

    if (!changed) return false;

    if (!ref.save()) {
        Logger::warn("TagLib save() failed for: " + filePath);
        return false;
    }

    Logger::info(QStringLiteral("User saved tags for: %1  fields=%2")
                     .arg(filePath, fields.join(',')));
    return true;
#else
    Q_UNUSED(filePath)
    Q_UNUSED(md)
    Q_UNUSED(fields)
    return false;
#endif
}

} // namespace tagit
