
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

#include "request/acceptcontact.h"
#include "logger.h"

// ============================================================ //

AcceptContactRequest::Pointer AcceptContactRequest::create(uint32_t id, const Zway::UBJ::Object &request, Callback callback)
{
    return AcceptContactRequest::Pointer(new AcceptContactRequest(id, request, callback));
}

AcceptContactRequest::AcceptContactRequest(uint32_t id, const Zway::UBJ::Object &request, Callback callback)
    : Request(AcceptContact, UBJ_OBJ("requestId" << id)),
      m_callback(callback)
{
    m_head = request;
}

bool AcceptContactRequest::processResponse(const Zway::UBJ::Object &head)
{
    uint32_t status = head["status"].toInt();

    if (m_callback) {

        m_callback(std::dynamic_pointer_cast<AcceptContactRequest>(shared_from_this()), head);
    }
}

// ============================================================ //

