
// ============================================================ //
//
//   d88888D db   d8b   db  .d8b.  db    db
//   YP  d8' 88   I8I   88 d8' `8b `8b  d8'
//      d8'  88   I8I   88 88ooo88  `8bd8'
//     d8'   Y8   I8I   88 88~~~88    88
//    d8' db `8b d8'8b d8' 88   88    88
//   d88888P  `8b8' `8d8'  YP   YP    YP
//
//   open-source, cross-platform, crypto-messenger
//
//   Copyright (C) 2018 Marc Weiler
//
//   This program is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program. If not, see <http://www.gnu.org/licenses/>.
//
// ============================================================ //

#ifndef ZWAY_SERVER_STREAM_BUFFER_H_
#define ZWAY_SERVER_STREAM_BUFFER_H_

#include "Zway/core/packet.h"
#include "thread.h"

// ============================================================ //

class FileBuffer : public Zway::Buffer
{
public:

    typedef std::shared_ptr<FileBuffer> Pointer;

    static Pointer create(const std::string& filename);

    static Pointer open(const std::string &filename);

    ~FileBuffer();

    void release();

    bool read(uint8_t* data, size_t size, size_t offset=0, size_t *bytesRead = nullptr);

    bool write(const uint8_t* data, size_t size, size_t offset=0, size_t *bytesWritten = nullptr);

    void flush();

protected:

    FileBuffer();

    bool init(const std::string &filename);

    bool load(const std::string &filename);

protected:

    ThreadSafe<FILE*> m_fh;

};

// ============================================================ //

class StreamBuffer : public Zway::Buffer, public std::enable_shared_from_this<StreamBuffer>
{
public:

    typedef std::shared_ptr<StreamBuffer> Pointer;

    static Pointer create(const std::string &filename, const Zway::Packet &pkt);

    static Pointer open(const std::string &filename, uint32_t id);

    bool read(uint8_t* data, size_t size, size_t offset=0, size_t *bytesRead = nullptr);

    bool write(const uint8_t *data, size_t size, size_t offset=0, size_t *bytesWritten = nullptr);

    void flush();

    uint32_t streamId();

    Zway::Packet::StreamType streamType();

    uint32_t streamParts();

    uint32_t bytesReadable();

    uint64_t lastActivity();

    size_t size();

protected:

    StreamBuffer();

    StreamBuffer(const Zway::Packet &pkt);

    bool init(const std::string &filename);

    bool load(const std::string &filename);

protected:

    Zway::BUFFER m_buffer;

    uint32_t m_streamId;

    Zway::Packet::StreamType m_streamType;

    uint32_t m_streamParts;

    ThreadSafe<uint64_t> m_bytesReadable;

    ThreadSafe<uint64_t> m_lastActivity;
};

typedef StreamBuffer::Pointer STREAM_BUFFER;

// ============================================================ //

#endif
