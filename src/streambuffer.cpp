
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

#include "streambuffer.h"
#include "logger.h"

// ============================================================ //

FileBuffer::Pointer FileBuffer::create(const std::string &filename)
{
     Pointer buf = Pointer(new FileBuffer());

     if (!buf->init(filename)) {

         return nullptr;
     }

     return buf;
}

FileBuffer::Pointer FileBuffer::open(const std::string &filename)
{
    Pointer buf = Pointer(new FileBuffer());

    if (!buf->load(filename)) {

        return nullptr;
    }

    return buf;
}

FileBuffer::FileBuffer()
    : m_fh(nullptr)
{

}

FileBuffer::~FileBuffer()
{
    release();
}

bool FileBuffer::init(const std::string &filename)
{
    m_fh = fopen(filename.c_str(), "rb");

    if (m_fh) {

        fclose(m_fh);

        return false;
    }

    m_fh = fopen(filename.c_str(), "ab+");

    if (!m_fh) {

        return false;
    }

    return true;
}

bool FileBuffer::load(const std::string &filename)
{
    m_fh = fopen(filename.c_str(), "rb");

    if (!m_fh) {

        return false;
    }

    fseek(m_fh, 0, SEEK_END);

    m_size = ftell(m_fh);

    fseek(m_fh, 0, SEEK_SET);

    return true;
}

void FileBuffer::release()
{
    boost::mutex::scoped_lock locker(m_fh);

    if (m_fh) {

        fclose(m_fh);

        m_fh = nullptr;
    }
}

bool FileBuffer::read(uint8_t *data, size_t size, size_t offset, size_t *bytesRead)
{
    boost::mutex::scoped_lock locker(m_fh);

    if (fseek(m_fh, offset, SEEK_SET)) {

        return false;
    }

    uint32_t r = fread(data, size, 1, m_fh);

    if (!r) {

        return false;
    }

    if (bytesRead) {

        *bytesRead = size;
    }

    return true;
}

bool FileBuffer::write(const uint8_t *data, size_t size, size_t offset, size_t *bytesWritten)
{
    boost::mutex::scoped_lock locker(m_fh);

    if (fseek(m_fh, offset, SEEK_SET)) {

        return false;
    }

    size_t r = fwrite(data, size, 1, m_fh);

    if (!r) {

        return false;
    }

    fflush(m_fh);

    if (bytesWritten) {

        *bytesWritten = size;
    }

    return true;
}

void FileBuffer::flush()
{
     boost::mutex::scoped_lock locker(m_fh);

     if (m_fh) {

         fflush(m_fh);
     }
}

// ============================================================ //

STREAM_BUFFER StreamBuffer::create(const std::string &filename, const Zway::Packet &pkt)
{
    STREAM_BUFFER buffer = STREAM_BUFFER(new StreamBuffer(pkt));

    if (!buffer->init(filename)) {

        return nullptr;
    }

    return buffer;
}

STREAM_BUFFER StreamBuffer::open(const std::string &filename, uint32_t id)
{

    FILE *pf = fopen(filename.c_str(), "rb");

    if (!pf) {

        return nullptr;
    }

    fclose(pf);


    STREAM_BUFFER buffer = STREAM_BUFFER(new StreamBuffer());

    if (!buffer->load(filename)) {

        return nullptr;
    }

    buffer->m_streamId = id;

    return buffer;
}

StreamBuffer::StreamBuffer()
    : m_streamId(0),
      m_streamType(Zway::Packet::Undefined),
      m_streamParts(0),
      m_bytesReadable(0),
      m_lastActivity(0)
{

}

StreamBuffer::StreamBuffer(const Zway::Packet &pkt)
    : m_streamId(pkt.streamId()),
      m_streamType(pkt.streamType()),
      m_streamParts(pkt.parts()),
      m_bytesReadable(0),
      m_lastActivity(0)
{

}

bool StreamBuffer::init(const std::string &filename)
{
    if (!filename.empty()) {

        m_buffer = FileBuffer::create(filename);
    }
    else {

        m_buffer = Zway::Buffer::create(nullptr, m_streamParts * Zway::MAX_PACKET_BODY);
    }

    if (!m_buffer) {

        return false;
    }

    return true;
}

bool StreamBuffer::load(const std::string &filename)
{
    FileBuffer::Pointer buf = FileBuffer::open(filename);

    if (!buf) {

        return false;
    }

    m_bytesReadable = buf->size();

    m_streamType = Zway::Packet::Resource;

    m_streamParts = m_bytesReadable / Zway::MAX_PACKET_BODY;

    if (m_bytesReadable % Zway::MAX_PACKET_BODY) {

        m_streamParts++;
    }

    m_buffer = buf;

    return true;
}

bool StreamBuffer::read(uint8_t *data, size_t size, size_t offset, size_t *bytesRead)
{
    if (!m_buffer) {

        return false;
    }

    size_t bytesToRead = 0;

    size_t br = bytesReadable();

    if (offset < br) {

        bytesToRead = br - offset > size ? size : br - offset;
    }

    if (bytesToRead) {

        if (!m_buffer->read(data, bytesToRead, offset, bytesRead)) {

            return false;
        }
    }
    else
    if (bytesRead) {

        *bytesRead = 0;
    }

    {
        boost::mutex::scoped_lock lock(m_lastActivity);

        m_lastActivity = time(nullptr);
    }

    return true;
}

bool StreamBuffer::write(const uint8_t *data, size_t size, size_t offset, size_t *bytesWritten)
{
    if (!m_buffer) {

        return false;
    }

    size_t bw = 0;

    size_t br = bytesReadable();

    if (!m_buffer->write(data, size, br, &bw)) {

        return false;
    }

    {
        boost::mutex::scoped_lock lock(m_bytesReadable);

        m_bytesReadable += bw;
    }

    if (bytesWritten) {

        *bytesWritten = bw;
    }

    {
        boost::mutex::scoped_lock lock(m_lastActivity);

        m_lastActivity = time(nullptr);
    }

    return true;
}

void StreamBuffer::flush()
{
    if (m_buffer) {

        m_buffer->flush();
    }
}

uint32_t StreamBuffer::streamId()
{
    return m_streamId;
}

Zway::Packet::StreamType StreamBuffer::streamType()
{
    return m_streamType;
}

uint32_t StreamBuffer::streamParts()
{
    return m_streamParts;
}

uint32_t StreamBuffer::bytesReadable()
{
    boost::mutex::scoped_lock lock(m_bytesReadable);

    return m_bytesReadable;
}

uint64_t StreamBuffer::lastActivity()
{
    boost::mutex::scoped_lock lock(m_lastActivity);

    return m_lastActivity;
}

size_t StreamBuffer::size()
{
    if (m_buffer) {

        return m_buffer->size();
    }

    return 0;
}

// ============================================================ //
